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

#pragma once

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>
#include <functional>
#include <string>

// WASAPI-based capture from a microphone endpoint, opened in the
// AudioCategory_Communications stream category. That category routes
// the capture stream through the system's voice DSP chain:
//   - acoustic echo cancellation (AEC)
//   - noise suppression (NS)
//   - automatic gain control (AGC)
// which is what makes Teams/Zoom/Discord recordings sound clean even
// in noisy rooms.
//
// MFCreateDeviceSource (the path AudioMedia::GetAudioMediaSourceFromEndpoint
// uses) does NOT engage these APOs, which is why the existing recording
// path sounds "warbly" and unfiltered next to those apps.
//
// Delivers samples through a callback that matches the prior
// AsyncMediaSourceReader contract so the sink writer wiring is unchanged.
class CommunicationsAudioCapture
{
public:
    using SampleCallback = std::function<void(IMFSample*, HRESULT)>;

    // Endpoint id is the same string MF returns via
    // MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, which is
    // also the form IMMDeviceEnumerator::GetDevice accepts.
    CommunicationsAudioCapture(std::wstring endpointId, SampleCallback callback);
    ~CommunicationsAudioCapture();

    CommunicationsAudioCapture(const CommunicationsAudioCapture&) = delete;
    CommunicationsAudioCapture& operator=(const CommunicationsAudioCapture&) = delete;

    // The input media type the encoder should accept. Determined from
    // the device's shared-mode mix format after the Communications
    // category is applied, so it reflects what the DSP actually outputs.
    winrt::com_ptr<IMFMediaType> MediaType() const { return mMediaType; }

    void Start();
    void Stop();

private:
    void CaptureLoop();
    void EmitPacket(const BYTE* data, UINT32 frames, UINT64 qpc100ns);
    void EmitSilence(UINT32 frames, UINT64 qpc100ns);

    std::wstring mEndpointId;
    SampleCallback mCallback;

    winrt::com_ptr<IMMDevice> mDevice;
    winrt::com_ptr<IAudioClient2> mAudioClient;
    winrt::com_ptr<IAudioCaptureClient> mCaptureClient;
    WAVEFORMATEX* mWaveFormat;
    winrt::com_ptr<IMFMediaType> mMediaType;
    HANDLE mAudioReadyEvent;

    std::atomic<bool> mRunning;
    std::thread mCaptureThread;
};
