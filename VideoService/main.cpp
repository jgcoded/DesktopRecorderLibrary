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
    VideoService — a stdio-driven host for VideoLibrary, designed to
    be spawned as a child process by the Electron app at
    DesktopRecorderApp/. The parent writes one JSON command per line
    to stdin; the service writes JSON responses to stdout.

    Commands:
      {"command":"getdevices"}
          → emits {"monitors":[...],"microphones":[...]} and exits 0.

      {"command":"startrecording","settings":{...}}
          → begins recording. Stays running until a `stoprecording`
            command arrives on stdin, then tears down and exits 0.
            On unexpected error it emits {"status":"error","hr":...}
            and exits non-zero.

      {"command":"stoprecording"}
          → only meaningful while a recording is in progress; signals
            the recording thread to stop.

    One process handles exactly one session (or one device query). The
    Electron parent spawns a fresh VideoService for each recording —
    a crash isolates to that recording, the app survives.

    NOTE: PipelineThread + GetMediaType + SetupPipelineThread are
    duplicated from SampleApp/main.cpp. When changing recording
    behavior, update both. A future refactor should pull these into
    a shared file (VideoLibrary header, or a sibling helper lib).
*/

#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Data::Json;

using namespace std;

winrt::com_ptr<IMFMediaType> GetMediaType(RECT virtualDesktopBounds)
{
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

// Block until a non-empty line arrives on stdin, then parse as JSON.
JsonObject GetNextCommand()
{
    std::wstring input;
    // std::getline returns the stream, which is truthy unless EOF/error.
    // We retry on empty lines (Electron may flush a stray newline) but
    // bail out cleanly on EOF — if the parent closed our stdin we have
    // no way to receive commands and there's nothing left to do.
    while (std::wcin)
    {
        if (!std::getline(std::wcin, input))
        {
            // EOF or error — signal up the stack.
            throw winrt::hresult_error{ E_HANDLE, L"stdin closed" };
        }
        if (!input.empty())
        {
            break;
        }
    }
    return JsonObject::Parse(hstring{ input });
}

void EmitJson(JsonObject object)
{
    std::wstring output{ object.Stringify() };
    std::wcout << output << std::endl;
    std::wcout.flush();
}

void EmitErrorJson(HRESULT hr)
{
    JsonObject object;
    object.Insert(L"status", JsonValue::CreateStringValue(L"error"));
    object.Insert(L"hr", JsonValue::CreateNumberValue(static_cast<double>(static_cast<int32_t>(hr))));
    EmitJson(object);
}

void GetDevices(JsonObject /*data*/)
{
    auto virtualDesktop = std::make_shared<VirtualDesktop>();
    std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->DesktopMonitors();
    auto virtualDesktopBounds = virtualDesktop->VirtualDesktopBounds();

    JsonArray monitorList;
    int i = 0;
    for (const DesktopMonitor& monitor : desktopMonitors)
    {
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
    auto audioDevices = AudioMedia::GetAudioRecordingDevices();
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

    EmitJson(devicesObject);
}

void SetupPipelineThread(std::shared_ptr<std::atomic_bool> stop, std::shared_ptr<std::atomic<HRESULT>> threadHResult)
{
    (void)SetThreadDescription(GetCurrentThread(), L"RecordingThread");

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
            return;
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

    // RNNoise / pre-roll state — see SampleApp/main.cpp for the
    // full design notes on destruction order, accumulator timing,
    // and AGC convergence. Mirrored here exactly.
    std::unique_ptr<RnnoiseFilter> rnnoise;
    bool rnnoiseEnabled = false;
    LONGLONG rnnoiseAccumStartTime100ns = 0;
    bool rnnoiseAccumStartSet = false;

    constexpr auto kAudioWarmUpDuration = std::chrono::milliseconds(1000);
    std::atomic<bool> audioWarmUpDone{ false };

    std::unique_ptr<CommunicationsAudioCapture> audioCapture;

    auto audioCallback = [&writer, &stop, &rnnoise, &rnnoiseEnabled,
                          &rnnoiseAccumStartTime100ns, &rnnoiseAccumStartSet,
                          &audioWarmUpDone](
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
            if (audioWarmUpDone.load(std::memory_order_acquire))
            {
                writer->WriteSample(sample);
            }
            return;
        }

        winrt::com_ptr<IMFMediaBuffer> inBuffer;
        winrt::check_hresult(sample->GetBufferByIndex(0, inBuffer.put()));

        BYTE* inData = nullptr;
        DWORD inBytes = 0;
        winrt::check_hresult(inBuffer->Lock(&inData, nullptr, &inBytes));
        const size_t inFrames = inBytes / sizeof(float);

        LONGLONG sampleTime = 0;
        winrt::check_hresult(sample->GetSampleTime(&sampleTime));
        if (!rnnoiseAccumStartSet)
        {
            rnnoiseAccumStartTime100ns = sampleTime;
            rnnoiseAccumStartSet = true;
        }

        const size_t outCapacityFrames = RnnoiseFilter::MaxOutputFor(inFrames);
        winrt::com_ptr<IMFMediaBuffer> outBuffer;
        winrt::check_hresult(MFCreateMemoryBuffer(static_cast<DWORD>(outCapacityFrames * sizeof(float)), outBuffer.put()));

        BYTE* outData = nullptr;
        winrt::check_hresult(outBuffer->Lock(&outData, nullptr, nullptr));
        const size_t outFrames = rnnoise->Process(
            reinterpret_cast<const float*>(inData),
            inFrames,
            reinterpret_cast<float*>(outData),
            outCapacityFrames);
        winrt::check_hresult(outBuffer->Unlock());
        winrt::check_hresult(inBuffer->Unlock());

        if (outFrames == 0)
        {
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

        rnnoiseAccumStartTime100ns += duration100ns;

        if (audioWarmUpDone.load(std::memory_order_acquire))
        {
            writer->WriteSample(outSample.get());
        }
    };

    if (!audioEndpoint.empty())
    {
        audioCapture = std::make_unique<CommunicationsAudioCapture>(
            std::wstring{ audioEndpoint }, audioCallback);
        audioMediaType = audioCapture->MediaType();

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

    if (audioCapture)
    {
        audioCapture->Start();
        std::this_thread::sleep_for(kAudioWarmUpDuration);
    }

    writer->Begin();
    audioWarmUpDone.store(true, std::memory_order_release);

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
    (void)SetThreadExecutionState(ES_CONTINUOUS);

    {
        if (audioCapture)
        {
            audioCapture->Stop();
            audioCapture.reset();
        }

        writer->End();
        writer.reset(nullptr);

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

        for (auto const& device : devices)
        {
            winrt::com_ptr<ID3D11DeviceContext> context;
            device->GetImmediateContext(context.put());
            context->ClearState();
            context->Flush();
        }
    }

    winrt::check_hresult(MFShutdown());
}

void StartRecording(JsonObject data)
{
    std::shared_ptr<std::atomic_bool> stopThread = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic<HRESULT>> threadHResult = std::make_shared<std::atomic<HRESULT>>(S_OK);

    std::thread pipelineThread{ PipelineThread, data, stopThread, threadHResult };

    // Block on stdin for the stoprecording command. If the parent's
    // stdin closes before stoprecording arrives, GetNextCommand
    // throws — we still want to tear down the recording cleanly.
    try
    {
        while (true)
        {
            JsonObject command = GetNextCommand();
            hstring name = command.Lookup(L"command").GetString();
            if (name == L"stoprecording")
            {
                break;
            }
            // Unknown commands during recording are ignored. We could
            // emit an error, but the parent shouldn't be sending them
            // anyway.
        }
    }
    catch (...)
    {
        // stdin closed or parse error — fall through to teardown.
    }

    stopThread->store(true);
    if (pipelineThread.joinable())
    {
        pipelineThread.join();
    }

    HRESULT hr = threadHResult->load();
    if (FAILED(hr))
    {
        EmitErrorJson(hr);
    }
    else
    {
        JsonObject object;
        object.Insert(L"status", JsonValue::CreateStringValue(L"stopped"));
        EmitJson(object);
    }
}

int main()
{
    // UTF-8 over stdio. Without this, std::wcin/wcout convert via the
    // system codepage (Windows-1252 on most Western installs), so
    // non-ASCII filenames sent from Electron would be mangled. _O_U8TEXT
    // makes the wide streams decode/encode UTF-8 directly.
    _setmode(_fileno(stdin), _O_U8TEXT);
    _setmode(_fileno(stdout), _O_U8TEXT);

    std::map<winrt::hstring, std::function<void(JsonObject)>> commandHandlers = {
        { L"getdevices", GetDevices },
        { L"startrecording", StartRecording },
    };

    try
    {
        JsonObject jsonObject = GetNextCommand();
        hstring command = jsonObject.Lookup(L"command").GetString();

        auto it = commandHandlers.find(command);
        if (it == commandHandlers.end())
        {
            EmitErrorJson(E_INVALIDARG);
            return -1;
        }

        it->second(jsonObject);
    }
    catch (...)
    {
        HRESULT hr = winrt::to_hresult();
        EmitErrorJson(hr);
        return hr;
    }

    return 0;
}
