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

#include <winrt/Windows.Media.MediaProperties.h>
#include "EncodingContext.h"

class ScreenMediaSinkWriter
{
public:

    ScreenMediaSinkWriter(const EncodingContext& encodingContext);

    void Begin();

    void SignalGap();

    void ResetDevice(winrt::com_ptr<ID3D11Device> device);

    void WriteSample(IMFSample* sample);

    void End();

    ~ScreenMediaSinkWriter();

private:
    winrt::com_ptr<IMFSinkWriter> mSinkWriter;
    UINT mManagerResetToken;
    winrt::com_ptr<IMFDXGIDeviceManager> mDeviceManager;
    winrt::com_ptr<IMFAttributes> mSinkWriterAttributes;
    winrt::com_ptr<IMFMediaType> mVideoInputMediaType;
    winrt::com_ptr<IMFMediaType> mVideoOutputMediaType;
    winrt::com_ptr<IMFMediaType> mAudioInputMediaType;
    winrt::com_ptr<IMFMediaType> mAudioOutputMediaType;
    winrt::com_ptr<ID3D11Device> mDevice;
    DWORD mVideoStreamIndex;
    DWORD mAudioStreamIndex;
    bool mIsWriting;
    std::chrono::high_resolution_clock::time_point mWriteStartTime;
    UINT32 mVideoFrameDuration;
    bool mComAlreadyLoaded;

    std::mutex mMutex;
};
