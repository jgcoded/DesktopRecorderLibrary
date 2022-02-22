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

#include "pch.h"
#include "DesktopDuplicationMediaStream.h"
#include "DesktopDuplicationMediaSource.h"

DesktopDuplicationMediaSource::DesktopDuplicationMediaSource()
    : mState{ SourceState::Invalid }
{
    MFCreateEventQueue(mEventQueue.ReleaseAndGetAddressOf());
}

// Inherited via IMFMediaSource

inline HRESULT Native::DesktopDuplicationMediaSource::QueryInterface(REFIID riid, void ** ppvObject)
{
    static QITAB entries[] = {
        QITABENT(DesktopDuplicationMediaSource, IMFMediaEventGenerator),
        QITABENT(DesktopDuplicationMediaSource, IMFMediaSource),
        { 0 }
    };

    return QISearch(this, entries, riid, ppvObject);
}

inline ULONG DesktopDuplicationMediaSource::AddRef(void)
{
    return InterlockedIncrement(&mReferenceCount);
}

inline ULONG DesktopDuplicationMediaSource::Release(void)
{
    auto count = InterlockedDecrement(&mReferenceCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

inline HRESULT DesktopDuplicationMediaSource::GetEvent(DWORD dwFlags, IMFMediaEvent ** ppEvent)
{
    auto hr = S_OK;
    WinPtr<IMFMediaEventQueue> q = nullptr;
    {
        // TODO LOCK
        // TODO check shutdown
        q = mEventQueue;
    }

    hr = q->GetEvent(dwFlags, ppEvent);
    return hr;
}

inline HRESULT DesktopDuplicationMediaSource::BeginGetEvent(IMFAsyncCallback * pCallback, IUnknown * punkState)
{
    auto hr = S_OK;

    // TODO lock
    // TODO check shutdown
    hr = mEventQueue->BeginGetEvent(pCallback, punkState);
    CheckAndThrowHRException("Could not get event queue begin event");

    return hr;
}

inline HRESULT DesktopDuplicationMediaSource::EndGetEvent(IMFAsyncResult * pResult, IMFMediaEvent ** ppEvent)
{
    // TODO lock
    // TODO check shutdown

    auto hr = S_OK;
    hr = mEventQueue->EndGetEvent(pResult, ppEvent);

    return hr;
}

inline HRESULT DesktopDuplicationMediaSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT * pvValue)
{

    // TODO LOCK
    // TODO check shutdown

    return mEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}

inline HRESULT DesktopDuplicationMediaSource::GetCharacteristics(DWORD * pdwCharacteristics)
{
    if (pdwCharacteristics == nullptr) {
        return E_POINTER;
    }

    *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;

    return S_OK;
}

inline HRESULT DesktopDuplicationMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor ** ppPresentationDescriptor)
{
    if (ppPresentationDescriptor == nullptr) {
        return  E_POINTER;
    }

    // get all streams (outputs in desktop dupl) and then create presentation descriptor
    HRESULT hr;
    if (!mPresentationDescriptor) {

        if (!mStream) {
            mStream = new DesktopDuplicationMediaStream(DefaultOutputIndex, this);
        }

        WinPtr<IMFStreamDescriptor> streamDescriptor;
        hr = mStream->GetStreamDescriptor(streamDescriptor.ReleaseAndGetAddressOf());
        CheckAndThrowHRException("could not get stream descriptor from from duplication media stream");

        hr = MFCreatePresentationDescriptor(1, streamDescriptor.GetAddressOf(), mPresentationDescriptor.ReleaseAndGetAddressOf());
        CheckAndThrowHRException("Could not create presentation descriptor");

        hr = mPresentationDescriptor->SelectStream((DWORD)DefaultStreamIndex);
        CheckAndThrowHRException("Could not select stream in presentation descriptor");
    }

    hr = mPresentationDescriptor->Clone(ppPresentationDescriptor);
    CheckAndThrowHRException("Could not clone presentation descriptor");

    return hr;
}

inline HRESULT DesktopDuplicationMediaSource::Start(IMFPresentationDescriptor * pPresentationDescriptor, const GUID * pguidTimeFormat, const PROPVARIANT * pvarStartPosition)
{
    return E_NOTIMPL;
}

inline HRESULT DesktopDuplicationMediaSource::Stop(void)
{
    return E_NOTIMPL;
}

inline HRESULT DesktopDuplicationMediaSource::Pause(void)
{
    return E_NOTIMPL;
}

inline HRESULT DesktopDuplicationMediaSource::Shutdown(void)
{
    return E_NOTIMPL;
}
