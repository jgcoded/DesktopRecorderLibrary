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

class DesktopDuplicationMediaStream;
/*

However, you might want to implement a custom media source for some other type of device or other live data source. There are only a few differences between a live source and other media sources:

In the IMFMediaSource::GetCharacteristics method, return the MFMEDIASOURCE_IS_LIVE flag.
The first sample should have a time stamp of zero.
Events and streaming states are handled the same as media sources, with the exception of the paused state.
While paused, do not queue samples. Drop any data that is generated while paused.
Live sources typically do not support seeking, reverse play, or rate control.

use event queue in media source and media stream so implement the IMFMediaEventGenerator interface
https://msdn.microsoft.com/en-us/library/windows/desktop/ms704617(v=vs.85).aspx
*/
class DesktopDuplicationMediaSource : public IMFMediaSource
{
public:
    const std::size_t DefaultOutputIndex = 0;
    const std::size_t DefaultStreamIndex = 0;

    DesktopDuplicationMediaSource();

    // Inherited via IMFMediaSource
    virtual HRESULT QueryInterface(REFIID riid, void **ppvObject) override;

    virtual ULONG AddRef(void) override;
    virtual ULONG Release(void) override;
    virtual HRESULT GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent) override;
    virtual HRESULT BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState) override;
    virtual HRESULT EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent) override;
    virtual HRESULT QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue) override;
    virtual HRESULT GetCharacteristics(DWORD *pdwCharacteristics) override;
    virtual HRESULT CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor) override;
    virtual HRESULT Start(IMFPresentationDescriptor *pPresentationDescriptor, const GUID *pguidTimeFormat, const PROPVARIANT *pvarStartPosition) override;
    virtual HRESULT Stop(void) override;
    virtual HRESULT Pause(void) override;
    virtual HRESULT Shutdown(void) override;

private:
    enum class SourceState
    {
        Invalid,
        Opening,
        Started,
        Paused,
        Stopped
    };

    SourceState mState;
    winrt::com_ptr<DesktopDuplicationMediaStream> mStream;
    winrt::com_ptr<IMFPresentationDescriptor> mPresentationDescriptor;
    ULONG mReferenceCount;
    winrt::com_ptr<IMFMediaEventQueue> mEventQueue;
};
