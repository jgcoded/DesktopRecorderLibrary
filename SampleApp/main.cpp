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

winrt::com_ptr<IMFMediaType> GetMediaType(RECT virtualDesktopBounds)
{
    // create media type
    winrt::com_ptr<IMFMediaType> mediaType;
    winrt::check_hresult(MFCreateMediaType(mediaType.put()));

    winrt::check_hresult(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

    const auto inputFormat = MFVideoFormat_ARGB32;
    winrt::check_hresult(mediaType->SetGUID(MF_MT_SUBTYPE, inputFormat));

    auto rect = virtualDesktopBounds;
    auto width = rect.right - rect.left;
    auto height = rect.bottom - rect.top;

    winrt::check_bool(width > 0 && height > 0);

    winrt::check_hresult(MFSetAttributeSize(mediaType.get(), MF_MT_FRAME_SIZE, width, height));

    // ARGB32 desktop pixels are full-range 0..255. Without this tag the
    // H.264 encoder assumes 16..235 studio range, which crushes blacks
    // and washes out highlights; the encoded stream is also tagged as
    // limited-range so players "expand" 16..235 -> 0..255, compounding
    // the damage. Tag the pipeline as full-range end to end.
    winrt::check_hresult(mediaType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255));

    return mediaType;
}

void PrintDevices(const std::vector<DesktopMonitor>& desktopMonitors, const std::vector<AudioDevice>& audioDevices)
{
    auto virtualDesktopBounds = VirtualDesktop::CalculateDesktopMonitorBounds(desktopMonitors);
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
    for (const auto& audioInput : audioDevices)
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
    std::wcout << output << endl;
}

void SetupPipelineThread(std::shared_ptr<std::atomic_bool> stop, std::shared_ptr<std::atomic<HRESULT>> threadHResult)
{
    (void)SetThreadDescription(GetCurrentThread(), L"RecordingThread");

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
                    threadHResult->store(winrt::to_hresult());
                    stop->store(true);
                }
            }
        }
    }
}

void PipelineThread(
    JsonObject data,
    std::shared_ptr<std::atomic_bool> stop,
    std::shared_ptr<std::atomic<HRESULT>> threadHResult)
{
    check_hresult(MFStartup(MF_VERSION));
    init_apartment();

    SetupPipelineThread(stop, threadHResult);

    if (stop->load())
    {
        return;
    }

    JsonObject settings = data.Lookup(L"settings").GetObjectW();
    hstring fileName = settings.Lookup(L"filename").GetString();
    int monitorIndex = (int)settings.Lookup(L"monitor").GetNumber();
    hstring audioEndpoint = settings.Lookup(L"audioEndpoint").GetString();
    ResolutionOption resolutionOption = (ResolutionOption)((int)settings.Lookup(L"resolutionOption").GetNumber());
    AudioQuality audioQuality = (AudioQuality)((int)settings.Lookup(L"audioQuality").GetNumber());
    int frameRate = (int)settings.Lookup(L"framerate").GetNumber();
    int bitRate = (int)settings.Lookup(L"bitrate").GetNumber();

    auto virtualDesktop = std::make_shared<VirtualDesktop>();
    com_ptr<IMFMediaType> videoMediaType = GetMediaType(virtualDesktop->VirtualDesktopBounds());
    com_ptr<IMFMediaType> audioMediaType;

    std::unique_ptr<ScreenMediaSinkWriter> writer;

    // RNNoise applies only when the capture format matches its
    // training: 48 kHz mono FP32. CommunicationsAudioCapture is
    // expected to produce exactly that on Windows engines, but
    // gate explicitly so e.g. a stereo card silently bypasses
    // rather than corrupting audio.
    //
    // Declared BEFORE audioCapture so destruction order joins the
    // capture thread (~audioCapture) before the filter goes away.
    // The explicit `audioCapture->Stop()` in the teardown block
    // also makes this safe, but ordering belt-and-suspenders.
    std::unique_ptr<RnnoiseFilter> rnnoise;
    bool rnnoiseEnabled = false;
    LONGLONG rnnoiseAccumStartTime100ns = 0;
    bool rnnoiseAccumStartSet = false;

    std::unique_ptr<CommunicationsAudioCapture> audioCapture;

    auto audioCallback = [&writer, &stop, &rnnoise, &rnnoiseEnabled,
                          &rnnoiseAccumStartTime100ns, &rnnoiseAccumStartSet](
        IMFSample* sample, HRESULT hr)
    {
        if (sample == nullptr || FAILED(hr))
        {
            stop->store(true);
            return;
        }
        sample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);

        if (!rnnoiseEnabled || !rnnoise)
        {
            writer->WriteSample(sample);
            return;
        }

        // Pull the float PCM out of the IMFSample, run it through
        // RNNoise, and write back a new sample carrying the
        // suppressed audio. The model emits in 480-sample chunks,
        // so the output's frame count is floor((accum + in)/480)*480
        // - prior accumulator; missing tail samples ride in the
        // accumulator until the next packet completes a frame.
        winrt::com_ptr<IMFMediaBuffer> inBuffer;
        winrt::check_hresult(sample->GetBufferByIndex(0, inBuffer.put()));

        BYTE* inData = nullptr;
        DWORD inBytes = 0;
        winrt::check_hresult(inBuffer->Lock(&inData, nullptr, &inBytes));
        const size_t inFrames = inBytes / sizeof(float);

        LONGLONG sampleTime = 0;
        winrt::check_hresult(sample->GetSampleTime(&sampleTime));
        // If the accumulator is empty, this packet's leading sample
        // is the start of the next emitted frame's timeline.
        if (!rnnoiseAccumStartSet)
        {
            rnnoiseAccumStartTime100ns = sampleTime;
            rnnoiseAccumStartSet = true;
        }

        winrt::com_ptr<IMFMediaBuffer> outBuffer;
        winrt::check_hresult(MFCreateMemoryBuffer(static_cast<DWORD>(inFrames * sizeof(float)), outBuffer.put()));

        BYTE* outData = nullptr;
        winrt::check_hresult(outBuffer->Lock(&outData, nullptr, nullptr));
        const size_t outFrames = rnnoise->Process(
            reinterpret_cast<const float*>(inData),
            inFrames,
            reinterpret_cast<float*>(outData));
        winrt::check_hresult(outBuffer->Unlock());
        winrt::check_hresult(inBuffer->Unlock());

        if (outFrames == 0)
        {
            // Nothing drained yet; just accumulating. Drop this packet
            // — the audio it carried will reappear in the next emit.
            return;
        }

        winrt::check_hresult(outBuffer->SetCurrentLength(static_cast<DWORD>(outFrames * sizeof(float))));

        winrt::com_ptr<IMFSample> outSample;
        winrt::check_hresult(MFCreateSample(outSample.put()));
        winrt::check_hresult(outSample->AddBuffer(outBuffer.get()));
        outSample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);

        const LONGLONG duration100ns = static_cast<LONGLONG>(outFrames) * 10'000'000 / 48000;
        winrt::check_hresult(outSample->SetSampleTime(rnnoiseAccumStartTime100ns));
        winrt::check_hresult(outSample->SetSampleDuration(duration100ns));

        // The emitted samples cover [accumStart, accumStart + duration);
        // whatever is still buffered starts where this output ended.
        rnnoiseAccumStartTime100ns += duration100ns;

        writer->WriteSample(outSample.get());
    };

    if (!audioEndpoint.empty())
    {
        // CommunicationsAudioCapture opens the mic in
        // AudioCategory_Communications, which routes through the system's
        // voice DSP (AEC/NS/AGC). Replaces the prior MFCreateDeviceSource
        // path that gave the unfiltered "warbly" sound.
        audioCapture = std::make_unique<CommunicationsAudioCapture>(
            std::wstring{ audioEndpoint }, audioCallback);
        audioMediaType = audioCapture->MediaType();

        // RNNoise gate: must be 48 kHz mono FP32 to match the training.
        // Communications-mode shared-mode capture on Win10/11 reports
        // plain WAVE_FORMAT_IEEE_FLOAT (0x3) at this configuration —
        // not WAVE_FORMAT_EXTENSIBLE — so a tag-equality check is the
        // right gate. Anything else passes through unfiltered.
        const WAVEFORMATEX* wf = audioCapture->WaveFormat();
        if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
            && wf->nSamplesPerSec == 48000
            && wf->nChannels == 1
            && wf->wBitsPerSample == 32)
        {
            rnnoise = std::make_unique<RnnoiseFilter>();
            rnnoiseEnabled = true;
        }
    }

    std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->DesktopMonitors();
    std::shared_ptr<DesktopPointer> desktopPointer = std::make_shared<DesktopPointer>(virtualDesktop->VirtualDesktopBounds());

    // One duplicator per monitor. Each duplicator inherits its device
    // from its monitor's display adapter, so this is naturally multi-
    // GPU: monitors on different adapters end up with different
    // ID3D11Devices, and Pipeline opens the shared surface on each
    // device via the keyed-mutex shared handle.
    std::vector<std::shared_ptr<ScreenDuplicator>> duplicators;
    duplicators.reserve(desktopMonitors.size());
    for (auto const& monitor : desktopMonitors)
    {
        duplicators.push_back(std::make_shared<ScreenDuplicator>(monitor, desktopPointer));
    }
    (void)monitorIndex;  // retained in the settings JSON for back-compat; ignored in multi-monitor mode

    RECT bounds = virtualDesktop->VirtualDesktopBounds();
    LONG width = bounds.right - bounds.left;
    LONG height = bounds.bottom - bounds.top;

    // The shared surface lives on the master device. Pick the first
    // duplicator's device as master; the sink writer encodes from the
    // same device.
    auto masterDevice = duplicators.front()->Device();
    std::shared_ptr<SharedSurface> sharedSurface = std::make_shared<SharedSurface>(
        masterDevice,
        width,
        height
    );
    {
        std::wstring fileNameW{ fileName };
        EncodingContext encodingContext{};
        encodingContext.fileName = fileNameW;
        encodingContext.resolutionOption = resolutionOption;
        encodingContext.audioQuality = audioQuality;
        encodingContext.frameRate = frameRate;
        encodingContext.bitRate = bitRate;
        encodingContext.videoInputMediaType = videoMediaType;
        encodingContext.audioInputMediaType = audioMediaType;
        encodingContext.device = masterDevice;

        writer = std::make_unique<ScreenMediaSinkWriter>(encodingContext);
    }

    writer->Begin();

    if (audioCapture)
    {
        audioCapture->Start();
    }

    // Enable away mode and prevent display and system idle timeouts
    (void)SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);

    std::unique_ptr<Pipeline> duplicationPipeline = std::make_unique<Pipeline>(
        duplicators,
        desktopPointer,
        sharedSurface,
        virtualDesktop->VirtualDesktopBounds()
    );

    while (!stop->load())
    {
        try
        {
            duplicationPipeline->Perform();
            winrt::com_ptr<IMFSample> sample = duplicationPipeline->Sample();

            if (duplicationPipeline->Sample())
            {
                sample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                writer->WriteSample(sample.get());
            }
            Sleep(1000 / frameRate);
        }
        catch (...)
        {
            threadHResult->store(winrt::to_hresult());
            stop->store(true);
        }
    }
    // Disable away mode
    (void)SetThreadExecutionState(ES_CONTINUOUS);

    // clear resources
    {
        if (audioCapture)
        {
            audioCapture->Stop();
            audioCapture.reset();
        }

        writer->End();
        writer.reset(nullptr);

        // Release the pipeline (drops its MonitorContributors which
        // hold opened views of the shared surface on each device),
        // then the duplicators, then the shared surface.
        duplicationPipeline.reset();

        std::vector<winrt::com_ptr<ID3D11Device>> devices;
        devices.reserve(duplicators.size());
        for (auto const& dup : duplicators)
        {
            devices.push_back(dup->Device());
        }
        duplicators.clear();
        sharedSurface.reset();
        desktopMonitors.clear();

        // Flush every device we used. With multi-GPU there can be more
        // than one — each needs to drop pending GPU work before we
        // MFShutdown, otherwise the debug layer reports dangling MF
        // references on objects still queued.
        for (auto const& device : devices)
        {
            winrt::com_ptr<ID3D11DeviceContext> context;
            device->GetImmediateContext(context.put());
            context->ClearState();
            context->Flush();

#if _DEBUG
            // Only available when the Graphics Tools optional Windows
            // feature is installed and DxResource succeeded in creating
            // a debug-layer device; otherwise the QI returns null.
            if (auto debug = device.try_as<ID3D11Debug>())
            {
                debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            }
#endif
        }
    }

    winrt::check_hresult(MFShutdown());
}

JsonObject MakeRecordCommand(hstring fileName, size_t monitorIndex)
{
    auto audioRecordingDevices = AudioMedia::GetAudioRecordingDevices();
    hstring audioEndpoint = L"";
    if (!audioRecordingDevices.empty())
    {
        audioEndpoint = audioRecordingDevices.front().endpoint;
    }

    JsonObject settings;
    settings.Insert(L"filename", JsonValue::CreateStringValue(fileName));
    settings.Insert(L"monitor", JsonValue::CreateNumberValue((double)monitorIndex));
    settings.Insert(L"audioEndpoint", JsonValue::CreateStringValue(audioEndpoint));
    settings.Insert(L"resolutionOption", JsonValue::CreateNumberValue((int)ResolutionOption::Auto));
    settings.Insert(L"audioQuality", JsonValue::CreateNumberValue((int)AudioQuality::Auto));
    settings.Insert(L"framerate", JsonValue::CreateNumberValue(30));
    settings.Insert(L"bitrate", JsonValue::CreateNumberValue(9000000));

    JsonObject object;
    object.Insert(L"settings", settings);
    object.Insert(L"command", JsonValue::CreateStringValue(L"startrecording"));

    return object;
}

struct RecordingContext
{
    std::shared_ptr<std::atomic_bool> stopThread;
    std::shared_ptr<std::atomic<HRESULT>> stopThreadResult;
    std::shared_ptr<std::atomic_bool> stopRecordingByUser;
    std::shared_ptr<BorderWindow> borderWindow;
    std::thread pipelineThread;

    ~RecordingContext()
    {
        if (stopThread)
        {
            stopThread->store(true);
        }

        if (pipelineThread.joinable())
        {
            pipelineThread.join();
        }
    }
};

std::unique_ptr<RecordingContext> StartRecording(hstring filename, RECT borderBounds, WindowFactory<BorderWindow>& windowFactory)
{
    std::unique_ptr<RecordingContext> recordingThread{ new RecordingContext{} };
    recordingThread->stopThread.reset(new atomic_bool{ false });
    recordingThread->stopThreadResult.reset(new atomic<HRESULT>{ S_OK });
    recordingThread->stopRecordingByUser.reset(new atomic_bool{ false });
    recordingThread->borderWindow = std::move(windowFactory.NewWindow());

    // Border covers the whole virtual desktop now that we record every
    // monitor at once.
    recordingThread->borderWindow->SizeAndPosition(
        borderBounds.left,
        borderBounds.top,
        borderBounds.right - borderBounds.left + 1,
        borderBounds.bottom - borderBounds.top + 1
    );

    JsonObject data = MakeRecordCommand(filename, 0);

    recordingThread->pipelineThread = std::thread {
        PipelineThread,
        data,
        recordingThread->stopThread,
        recordingThread->stopThreadResult
    };

    return recordingThread;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    std::wstring fileNameBase = L"test-recording";

    check_hresult(MFStartup(MF_VERSION));
    init_apartment();

    auto virtualDesktop = std::make_shared<VirtualDesktop>();

    {
        std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->GetAllDesktopMonitors();
        auto audioDevices = AudioMedia::GetAudioRecordingDevices();
        PrintDevices(desktopMonitors, audioDevices);
    }

    auto windowFactory = WindowFactory<Window>::Create(hInstance);

    auto window = windowFactory.NewWindow();
    window->Size(400, 120);

    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerwindowmessagea
    const auto startRecordingMessage = RegisterWindowMessage(L"DesktopRecorderStartRecording");

    const auto stopRecordingMessage = RegisterWindowMessage(L"DesktopRecorderStopRecording");

    std::unique_ptr<RecordingContext> recordingThread;

    auto borderWindowFactory = WindowFactory<BorderWindow>::Create(hInstance);

    window->Button(L"Start Recording", 10, 10, 200, 40, [&](HWND hwnd) {

        if (recordingThread)
        {
            // don't process the button click if already stopping
            if (!recordingThread->stopRecordingByUser->load())
            {
                recordingThread->stopRecordingByUser->store(true);
                recordingThread->stopThread->store(true);
                SetWindowText(hwnd, L"Start Recording");
                PostMessage(nullptr, stopRecordingMessage, 0, 0);
            }
        }
        else
        {
            SetWindowText(hwnd, L"Stop Recording");
            PostMessage(nullptr, startRecordingMessage, 0, 0);
        }
    });

    int fileNumber = 0;

    // https://docs.microsoft.com/en-us/windows/win32/learnwin32/window-messages
    // The Window class posts WM_QUIT from its WM_DESTROY handler, which is
    // what makes GetMessage return 0 and break this loop. We then tear
    // down the recording thread AFTER the loop, so window close isn't
    // blocked on joining the pipeline+audio threads.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == startRecordingMessage)
        {
            std::wstringstream ss;
            ss << fileNameBase << "-" << fileNumber++ << ".mp4";
            hstring filename{ ss.str() };
            // Border + recording target are now the entire virtual
            // desktop; Pipeline captures from every monitor.
            recordingThread = std::move(StartRecording(filename, virtualDesktop->VirtualDesktopBounds(), borderWindowFactory));
        }
        else if (msg.message == stopRecordingMessage)
        {
            if (recordingThread)
            {
                recordingThread.reset(nullptr);
            }
        }

        // check if the thread stopped due to an error
        if (recordingThread
            && !recordingThread->stopRecordingByUser->load()
            && recordingThread->stopThread->load())
        {
            // Restart the recording
            recordingThread.reset(nullptr);
            PostMessage(nullptr, startRecordingMessage, 0, 0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // After WM_QUIT: stop the recording thread (joins the pipeline and
    // audio capture threads via RecordingContext's dtor) before MFShutdown.
    if (recordingThread)
    {
        recordingThread.reset(nullptr);
    }

    winrt::check_hresult(MFShutdown());

    return 0;
}
