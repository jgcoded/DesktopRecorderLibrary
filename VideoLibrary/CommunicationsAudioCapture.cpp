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

#include "pch.h"
#include <sstream>
#include "CommunicationsAudioCapture.h"

namespace
{
    // Buffer duration requested from WASAPI, in 100-ns units. Shared-mode
    // capture ignores this and uses the engine period, but it has to be
    // non-zero. 100 ms is a comfortable upper bound.
    constexpr REFERENCE_TIME kRequestedBufferDuration = 1'000'000;
}

CommunicationsAudioCapture::CommunicationsAudioCapture(
    std::wstring endpointId,
    SampleCallback callback)
    : mEndpointId{ std::move(endpointId) }
    , mCallback{ std::move(callback) }
    , mWaveFormat{ nullptr }
    , mAudioReadyEvent{ nullptr }
    , mRunning{ false }
{
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::check_hresult(CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), enumerator.put_void()));

    winrt::check_hresult(enumerator->GetDevice(mEndpointId.c_str(), mDevice.put()));

    winrt::check_hresult(mDevice->Activate(
        __uuidof(IAudioClient2), CLSCTX_INPROC_SERVER, nullptr,
        mAudioClient.put_void()));

    // SetClientProperties MUST be called before Initialize. The
    // AudioCategory_Communications category is what engages the voice
    // DSP chain. Without it, the stream uses the generic Media category
    // and the raw mic input flows through unfiltered.
    AudioClientProperties props = {};
    props.cbSize = sizeof(AudioClientProperties);
    props.bIsOffload = FALSE;
    props.eCategory = AudioCategory_Communications;
    props.Options = AUDCLNT_STREAMOPTIONS_NONE;
    winrt::check_hresult(mAudioClient->SetClientProperties(&props));

    // GetMixFormat reflects what the engine will deliver *after* the
    // Communications APOs run. If the system honored the category, this
    // typically drops to mono / 16 kHz; if it stayed at the device's
    // default (e.g. 48 kHz stereo float) the APO chain probably didn't
    // engage and the recorded audio will sound like the raw mic.
    winrt::check_hresult(mAudioClient->GetMixFormat(&mWaveFormat));

    // Diagnostic: surface the post-Communications format so the user can
    // confirm DSP engagement without reaching for an external tool.
    {
        std::wstringstream ss;
        ss << L"CommunicationsAudioCapture mix format: "
           << mWaveFormat->nSamplesPerSec << L" Hz, "
           << mWaveFormat->nChannels << L" ch, "
           << mWaveFormat->wBitsPerSample << L" bits, tag=0x"
           << std::hex << mWaveFormat->wFormatTag << L"\n";
        OutputDebugStringW(ss.str().c_str());
    }

    mAudioReadyEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    winrt::check_pointer(mAudioReadyEvent);

    // No AUTOCONVERTPCM: we Initialize with exactly the engine's mix
    // format, so no resampler is needed. The earlier AUTOCONVERTPCM flag
    // can push the stream through a converter chain that bypasses parts
    // of the APO graph.
    DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    winrt::check_hresult(mAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED, streamFlags,
        kRequestedBufferDuration, 0,
        mWaveFormat, nullptr));

    winrt::check_hresult(mAudioClient->SetEventHandle(mAudioReadyEvent));

    winrt::check_hresult(mAudioClient->GetService(
        __uuidof(IAudioCaptureClient), mCaptureClient.put_void()));

    winrt::check_hresult(MFCreateMediaType(mMediaType.put()));
    winrt::check_hresult(MFInitMediaTypeFromWaveFormatEx(
        mMediaType.get(), mWaveFormat, sizeof(WAVEFORMATEX) + mWaveFormat->cbSize));
}

CommunicationsAudioCapture::~CommunicationsAudioCapture()
{
    Stop();
    if (mWaveFormat)
    {
        CoTaskMemFree(mWaveFormat);
        mWaveFormat = nullptr;
    }
    if (mAudioReadyEvent)
    {
        CloseHandle(mAudioReadyEvent);
        mAudioReadyEvent = nullptr;
    }
}

void CommunicationsAudioCapture::Start()
{
    if (mRunning.exchange(true))
    {
        return;
    }
    mCaptureThread = std::thread{ &CommunicationsAudioCapture::CaptureLoop, this };
}

void CommunicationsAudioCapture::Stop()
{
    if (!mRunning.exchange(false))
    {
        return;
    }
    // Nudge the capture thread so it doesn't block on the event for the
    // full timeout before checking mRunning.
    if (mAudioReadyEvent)
    {
        SetEvent(mAudioReadyEvent);
    }
    if (mCaptureThread.joinable())
    {
        mCaptureThread.join();
    }
}

void CommunicationsAudioCapture::CaptureLoop()
{
    (void)SetThreadDescription(GetCurrentThread(), L"CommsAudioCapture");
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(comHr);

    try
    {
        winrt::check_hresult(mAudioClient->Start());

        while (mRunning.load())
        {
            DWORD wait = WaitForSingleObject(mAudioReadyEvent, 200);
            if (wait != WAIT_OBJECT_0)
            {
                if (wait == WAIT_TIMEOUT)
                {
                    continue;
                }
                break;
            }

            UINT32 packetFrames = 0;
            winrt::check_hresult(mCaptureClient->GetNextPacketSize(&packetFrames));
            while (packetFrames > 0 && mRunning.load())
            {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                UINT64 devicePosition = 0;
                UINT64 qpcPosition = 0;

                winrt::check_hresult(mCaptureClient->GetBuffer(
                    &data, &frames, &flags, &devicePosition, &qpcPosition));

                if (frames > 0)
                {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    {
                        EmitSilence(frames, qpcPosition);
                    }
                    else
                    {
                        EmitPacket(data, frames, qpcPosition);
                    }
                }

                winrt::check_hresult(mCaptureClient->ReleaseBuffer(frames));
                winrt::check_hresult(mCaptureClient->GetNextPacketSize(&packetFrames));
            }
        }

        (void)mAudioClient->Stop();
    }
    catch (...)
    {
        mCallback(nullptr, winrt::to_hresult());
    }

    if (comInitialized)
    {
        CoUninitialize();
    }
}

void CommunicationsAudioCapture::EmitPacket(const BYTE* data, UINT32 frames, UINT64 qpc100ns)
{
    const UINT32 bytes = frames * mWaveFormat->nBlockAlign;

    winrt::com_ptr<IMFMediaBuffer> buffer;
    winrt::check_hresult(MFCreateMemoryBuffer(bytes, buffer.put()));

    BYTE* dest = nullptr;
    winrt::check_hresult(buffer->Lock(&dest, nullptr, nullptr));
    memcpy(dest, data, bytes);
    winrt::check_hresult(buffer->Unlock());
    winrt::check_hresult(buffer->SetCurrentLength(bytes));

    winrt::com_ptr<IMFSample> sample;
    winrt::check_hresult(MFCreateSample(sample.put()));
    winrt::check_hresult(sample->AddBuffer(buffer.get()));
    winrt::check_hresult(sample->SetSampleTime(static_cast<LONGLONG>(qpc100ns)));

    const LONGLONG duration = static_cast<LONGLONG>(frames) * 10'000'000
        / mWaveFormat->nSamplesPerSec;
    winrt::check_hresult(sample->SetSampleDuration(duration));

    mCallback(sample.get(), S_OK);
}

void CommunicationsAudioCapture::EmitSilence(UINT32 frames, UINT64 qpc100ns)
{
    const UINT32 bytes = frames * mWaveFormat->nBlockAlign;

    winrt::com_ptr<IMFMediaBuffer> buffer;
    winrt::check_hresult(MFCreateMemoryBuffer(bytes, buffer.put()));

    BYTE* dest = nullptr;
    winrt::check_hresult(buffer->Lock(&dest, nullptr, nullptr));
    memset(dest, 0, bytes);
    winrt::check_hresult(buffer->Unlock());
    winrt::check_hresult(buffer->SetCurrentLength(bytes));

    winrt::com_ptr<IMFSample> sample;
    winrt::check_hresult(MFCreateSample(sample.put()));
    winrt::check_hresult(sample->AddBuffer(buffer.get()));
    winrt::check_hresult(sample->SetSampleTime(static_cast<LONGLONG>(qpc100ns)));

    const LONGLONG duration = static_cast<LONGLONG>(frames) * 10'000'000
        / mWaveFormat->nSamplesPerSec;
    winrt::check_hresult(sample->SetSampleDuration(duration));

    mCallback(sample.get(), S_OK);
}
