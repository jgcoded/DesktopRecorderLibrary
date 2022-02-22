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

// TODO Finish porting and use winrt

#pragma once

#include "DXResource.h"
#include "DxgiDuplication.h"

class DesktopDuplicationMediaSource;

class DesktopDuplicationMediaStream : public IMFMediaStream
{
public:
    const int FPS_NUMERATOR = 30000;
    const int FPS_DENOMINATOR = 1001;

    DesktopDuplicationMediaStream(std::size_t outputIndex, DesktopDuplicationMediaSource *source);

    // Inherited via IMFMediaStream
    virtual HRESULT QueryInterface(REFIID riid, void **ppvObject) override;
    virtual ULONG AddRef(void) override;
    virtual ULONG Release(void) override;
    virtual HRESULT GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent) override;
    virtual HRESULT BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState) override;
    virtual HRESULT EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent) override;
    virtual HRESULT QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue) override;
    virtual HRESULT GetMediaSource(IMFMediaSource **ppMediaSource) override;
    virtual HRESULT GetStreamDescriptor(IMFStreamDescriptor **ppStreamDescriptor) override;
    virtual HRESULT RequestSample(IUnknown *pToken) override;

private:
    void InitializeMainMediaType();

    void InitializeStreamDescriptor();

    WinPtr<IMFMediaType> mMediaType;
    WinPtr<IMFStreamDescriptor> mStreamDescriptor;
    WinPtr<IMFMediaEventQueue> mEventQueue;
    WinPtr<DesktopDuplicationMediaSource> mSource;
    std::shared_ptr<DXResource> mDx;
    std::unique_ptr<DxgiDuplication> mDupl;
    std::size_t mOutputIndex;
    ULONG mReferenceCount;
};
