/*
    Copyright (C) 2022 by Julio Gutierrez (desktoprecorderapp@gmail.com)

    This file is part of DesktopRecorderLibrary.

    DesktopRecorderLibrary is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    DesktopRecorderLibrary is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DesktopRecorderLibrary. If not, see <https://www.gnu.org/licenses/>.
*/

/*
    This application was ported over from another application
    that used stdin/stdout to communicate with a parent process.
    That is why some of the functions here take a JSON
    as input and output JSON to stdout.
*/

#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Foundation::Collections;
using namespace Windows::Data::Json;

using namespace std;

winrt::com_ptr<IMFMediaType> GetMediaTypeFromMediaSource(winrt::com_ptr<IMFMediaSource> source)
{
    winrt::com_ptr<IMFPresentationDescriptor> videoDesc;
    source->CreatePresentationDescriptor(videoDesc.put());

    BOOL isSelected = false;
    winrt::com_ptr<IMFStreamDescriptor> videoStreamDescriptor;
    videoDesc->GetStreamDescriptorByIndex(0, &isSelected, videoStreamDescriptor.put());

    winrt::com_ptr<IMFMediaTypeHandler> videoMediaTypeHandler;
    videoStreamDescriptor->GetMediaTypeHandler(videoMediaTypeHandler.put());

    winrt::com_ptr<IMFMediaType> videoMediaType;
    videoMediaTypeHandler->GetMediaTypeByIndex(0, videoMediaType.put());

    return videoMediaType;
}

winrt::com_ptr<IMFMediaType> GetMediaTypeFromDuplicator(DesktopMonitor::ScreenDuplicator& duplicator)
{
    // create media type
    winrt::com_ptr<IMFMediaType> mediaType;
    winrt::check_hresult(MFCreateMediaType(mediaType.put()));

    winrt::check_hresult(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

    const auto inputFormat = MFVideoFormat_ARGB32;
    winrt::check_hresult(mediaType->SetGUID(MF_MT_SUBTYPE, inputFormat));

    auto rect = duplicator.VirtualDesktop()->VirtualDesktopBounds();
    auto width = rect.right - rect.left;
    auto height = rect.bottom - rect.top;

    winrt::check_bool(width > 0 && height > 0);

    winrt::check_hresult(MFSetAttributeSize(mediaType.get(), MF_MT_FRAME_SIZE, width, height));

    return mediaType;
}

void PrintDevices()
{
    auto virtualDesktop = std::make_shared<VirtualDesktop>();
    std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->GetAllDesktopMonitors();

    auto virtualDesktopBounds = virtualDesktop->VirtualDesktopBounds();
    JsonArray monitorList;
    int i = 0;
    for (const DesktopMonitor& monitor : desktopMonitors) {
        JsonObject monitorObject;
        auto bounds = monitor.DesktopMonitorBounds();

        monitorObject.Insert(L"name", JsonValue::CreateStringValue(monitor.OutputName()));
        monitorObject.Insert(L"adapter", JsonValue::CreateStringValue(monitor.Adapter().Name()));
        monitorObject.Insert(L"top", JsonValue::CreateNumberValue(bounds.top - virtualDesktopBounds.top));
        monitorObject.Insert(L"left", JsonValue::CreateNumberValue(bounds.left - virtualDesktopBounds.left));
        monitorObject.Insert(L"bottom", JsonValue::CreateNumberValue(bounds.bottom - virtualDesktopBounds.top));
        monitorObject.Insert(L"right", JsonValue::CreateNumberValue(bounds.right - virtualDesktopBounds.left));
        monitorObject.Insert(L"rotation", JsonValue::CreateNumberValue(monitor.Rotation()));
        monitorObject.Insert(L"index", JsonValue::CreateNumberValue(i++));
        monitorList.Append(monitorObject);
    }

    JsonArray microphoneList;
    auto audioRecordingDevices = AudioMedia::GetAudioRecordingDevices();
    for (const auto& audioInput : audioRecordingDevices)
    {
        JsonObject audioObject;
        audioObject.Insert(L"name", JsonValue::CreateStringValue(audioInput.friendlyName));
        audioObject.Insert(L"endpoint", JsonValue::CreateStringValue(audioInput.endpoint));

        microphoneList.Append(audioObject);
    }

    JsonObject devicesObject;
    devicesObject.Insert(L"monitors", monitorList);
    devicesObject.Insert(L"microphones", microphoneList);

    std::wstring output{ devicesObject.Stringify() };
    wcout << output << endl;
}

void PipelineThread(
    std::shared_ptr<DesktopMonitor::ScreenDuplicator> duplicator,
    std::function<void(IMFSample* sample)> sampleCallback,
    std::function<void(HRESULT hr)> errorCallback,
    std::shared_ptr<std::atomic_bool> stop,
    int frameRate)
{
    {
        const auto startTime = std::chrono::high_resolution_clock::now();
        while (!stop->load())
        {
            try
            {
                HDESK currentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
                winrt::check_pointer(currentDesktop);
                bool desktopAttached = SetThreadDesktop(currentDesktop) != 0;
                (void)CloseDesktop(currentDesktop);
                winrt::check_bool(desktopAttached);
                break;
            }
            catch (...)
            {
                Sleep(100);
                const auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(startTime - now) > std::chrono::seconds{ 3 })
                {
                    stop->store(true);
                    errorCallback(winrt::to_hresult());
                }
            }
        }
    }

    std::unique_ptr<Pipeline> duplicationPipeline = std::make_unique<Pipeline>(duplicator);

    while (!stop->load())
    {
        try
        {
            (void)SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
            duplicationPipeline->Perform();
            if (duplicationPipeline->Sample())
            {
                sampleCallback(duplicationPipeline->Sample().get());
            }
            Sleep(1000 / frameRate);
        }
        catch (...)
        {
            errorCallback(winrt::to_hresult());
            stop->store(true);
        }
    }

    winrt::com_ptr<ID3D11DeviceContext> context;
    duplicator->Device()->GetImmediateContext(context.put());
    context->ClearState();
    context->Flush();
}

std::atomic stopRecordingByUser = false;
BOOL CtrlHandler(DWORD)
{
    stopRecordingByUser = true;
    return TRUE;
}

void StartRecording(JsonObject data)
{
    auto virtualDesktop = std::make_shared<VirtualDesktop>();
#if _DEBUG
    auto debug = virtualDesktop->Device().as<ID3D11Debug>();
#endif
    std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->GetAllDesktopMonitors();

    JsonObject settings = data.Lookup(L"settings").GetObjectW();
    hstring fileName = settings.Lookup(L"filename").GetString();
    int monitorIndex = (int)settings.Lookup(L"monitor").GetNumber();
    hstring audioEndpoint = settings.Lookup(L"audioEndpoint").GetString();
    ResolutionOption resolutionOption = (ResolutionOption)((int)settings.Lookup(L"resolutionOption").GetNumber());
    AudioQuality audioQuality = (AudioQuality)((int)settings.Lookup(L"audioQuality").GetNumber());
    int frameRate = (int)settings.Lookup(L"framerate").GetNumber();
    int bitRate = (int)settings.Lookup(L"bitrate").GetNumber();

    check_hresult(MFStartup(MF_VERSION));

    init_apartment();

    // record!
    std::unique_ptr<DesktopMonitor::ScreenDuplicator> duplicator = std::move(virtualDesktop->RecordMonitor(desktopMonitors[monitorIndex]));

    com_ptr<IMFMediaType> videoMediaType = GetMediaTypeFromDuplicator(*duplicator);

    com_ptr<IMFMediaSource> audioMediaSource;
    com_ptr<IMFMediaType> audioMediaType;

    if (!audioEndpoint.empty())
    {
        audioMediaSource = AudioMedia::GetAudioMediaSourceFromEndpoint(std::wstring{ audioEndpoint });
        audioMediaType = GetMediaTypeFromMediaSource(audioMediaSource);
    }

    std::wstring fileNameW{ fileName };

    EncodingContext encodingContext{};
    encodingContext.fileName = fileNameW;
    encodingContext.resolutionOption = resolutionOption;
    encodingContext.audioQuality = audioQuality;
    encodingContext.frameRate = frameRate;
    encodingContext.bitRate = bitRate;
    encodingContext.videoInputMediaType = videoMediaType;
    encodingContext.audioInputMediaType = audioMediaType;
    encodingContext.device = duplicator->Device();

    auto writer = std::make_unique<ScreenMediaSinkWriter>(encodingContext);

    winrt::com_ptr<AsyncMediaSourceReader> audioReader;
    std::shared_ptr<std::atomic_bool> stopThread{ new std::atomic_bool{false} };
    std::thread pipelineThread;

    auto shutdownCallback = [&](HRESULT reason) {

        if (audioReader)
        {
            audioReader->Stop();
        }

        stopThread->store(true);

        if (stopRecordingByUser && pipelineThread.joinable())
        {
            pipelineThread.join();
        }

        if (audioReader)
        {
            audioReader = nullptr;
        }

        writer->End();

        writer.reset(nullptr);

        winrt::check_hresult(MFShutdown());

        desktopMonitors.clear();

        winrt::com_ptr<ID3D11DeviceContext> context;
        virtualDesktop->Device()->GetImmediateContext(context.put());
        context->ClearState();
        context->Flush();

        virtualDesktop.reset();

        if (!stopRecordingByUser)
        {
            // unexpected shutdown
            exit(reason);
        }
    };

    auto videoCallback = [&writer, &shutdownCallback](IMFSample* sample)
    {
        if (sample == nullptr)
        {
            shutdownCallback(E_POINTER);
        }

        sample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        writer->WriteSample(sample);
    };

    auto audioCallback = [&writer, &shutdownCallback](IMFSample* sample, HRESULT hr)
    {
        if (sample == nullptr && FAILED(hr))
        {
            shutdownCallback(hr);
        }

        sample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        writer->WriteSample(sample);
    };

    if (audioMediaSource)
    {
        audioReader.attach(new AsyncMediaSourceReader{
            audioMediaSource,
            audioCallback,
            1000 / frameRate,
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM });
    }

    writer->Begin();

    pipelineThread = std::thread{
        PipelineThread,
        std::move(duplicator),
        videoCallback,
        shutdownCallback,
        stopThread,
        frameRate};

    if (audioReader)
    {
        audioReader->Start();
    }

    winrt::check_bool(SetConsoleCtrlHandler(&CtrlHandler, TRUE));

    while (!stopRecordingByUser) {
        Sleep(100);
    }

    shutdownCallback(S_OK);

#if _DEBUG
    if (debug)
    {
        debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
#endif
}

int main()
{
    PrintDevices();
    cout << endl << endl;

    auto audioRecordingDevices = AudioMedia::GetAudioRecordingDevices();
    hstring audioEndpoint = L"";
    if (!audioRecordingDevices.empty())
    {
        audioEndpoint = audioRecordingDevices.front().endpoint;
        wcout << "Using Audio Endpoint: " << audioEndpoint.c_str() << endl;
    }
    else
    {
        wcout << "No audio recording devices found" << endl;
    }

    hstring filename = L"test-recording.mp4";
    wcout << L"Saving recording as file: " << filename.c_str() << endl;

    JsonObject settings;
    settings.Insert(L"filename", JsonValue::CreateStringValue(filename));
    settings.Insert(L"monitor", JsonValue::CreateNumberValue(0));
    settings.Insert(L"audioEndpoint", JsonValue::CreateStringValue(audioEndpoint));
    settings.Insert(L"resolutionOption", JsonValue::CreateNumberValue((int)ResolutionOption::Auto));
    settings.Insert(L"audioQuality", JsonValue::CreateNumberValue((int)AudioQuality::Auto));
    settings.Insert(L"framerate", JsonValue::CreateNumberValue(30));
    settings.Insert(L"bitrate", JsonValue::CreateNumberValue(9000000));

    JsonObject object;
    object.Insert(L"settings", settings);
    object.Insert(L"command", JsonValue::CreateStringValue(L"startrecording"));

    StartRecording(object);

    return 0;
}
