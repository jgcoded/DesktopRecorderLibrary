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
#include "DesktopDuplicationMediaSource.h"
#include "DesktopDuplicationMediaStream.h"

DesktopDuplicationMediaStream::DesktopDuplicationMediaStream(std::size_t outputIndex, DesktopDuplicationMediaSource* source)
    : mOutputIndex{ outputIndex }
    , mDx{ new DXResource }
{
    mDupl = std::make_unique<DxgiDuplication>(mDx, mOutputIndex);
    mSource = source;
}

// Inherited via IMFMediaStream

inline HRESULT DesktopDuplicationMediaStream::QueryInterface(REFIID riid, void ** ppvObject)
{
    static QITAB entries[] = {
        QITABENT(DesktopDuplicationMediaStream, IMFMediaEventGenerator),
        QITABENT(DesktopDuplicationMediaStream, IMFMediaStream),
        { 0 }
    };

    return QISearch(this, entries, riid, ppvObject);
}

inline ULONG DesktopDuplicationMediaStream::AddRef(void)
{
    return InterlockedIncrement(&mReferenceCount);
}

inline ULONG DesktopDuplicationMediaStream::Release(void)
{
    auto count = InterlockedDecrement(&mReferenceCount);
    if (count == 0) {
        delete this;
    }
    return count;
}

inline HRESULT DesktopDuplicationMediaStream::GetEvent(DWORD dwFlags, IMFMediaEvent ** ppEvent)
{
    return E_NOTIMPL;
}

inline HRESULT DesktopDuplicationMediaStream::BeginGetEvent(IMFAsyncCallback * pCallback, IUnknown * punkState)
{
    // TODO lock source
    // TODO check shutdown
    return mEventQueue->BeginGetEvent(pCallback, punkState);
}

inline HRESULT DesktopDuplicationMediaStream::EndGetEvent(IMFAsyncResult * pResult, IMFMediaEvent ** ppEvent)
{
    // TODO lock source
    // todo cehck shutdown
    return mEventQueue->EndGetEvent(pResult, ppEvent);
}

inline HRESULT DesktopDuplicationMediaStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT * pvValue)
{
    // TODO lock
    // todo check shutdown
    return mEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}

inline HRESULT DesktopDuplicationMediaStream::GetMediaSource(IMFMediaSource ** ppMediaSource)
{
    // TODO lock source
    // check shutdown
    return mSource->QueryInterface(IID_PPV_ARGS(ppMediaSource));
}

inline HRESULT DesktopDuplicationMediaStream::GetStreamDescriptor(IMFStreamDescriptor ** ppStreamDescriptor)
{
    if (ppStreamDescriptor == nullptr) {
        return  E_POINTER;
    }

    if (!mStreamDescriptor) {
        InitializeStreamDescriptor();
    }

    *ppStreamDescriptor = mStreamDescriptor.Get();

    return S_OK;
}

inline HRESULT DesktopDuplicationMediaStream::RequestSample(IUnknown * pToken)
{
    return E_NOTIMPL;
}

inline void DesktopDuplicationMediaStream::InitializeMainMediaType()
{
    auto hr = MFCreateMediaType(mMediaType.GetAddressOf());
    CheckAndThrowHRException("Could not create video input media type");

    hr = mMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    CheckAndThrowHRException("Could not set video input media type attribue");

    const auto inputFormat = MFVideoFormat_ARGB32;
    hr = mMediaType->SetGUID(MF_MT_SUBTYPE, inputFormat);
    CheckAndThrowHRException("Could not set video input media type format attribue");

    hr = mMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    CheckAndThrowHRException("Could not set video input media type interlace mode attribue");

    auto desc = mDupl->GetOutputDesc();
    auto width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
    auto height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
    hr = MFSetAttributeSize(mMediaType.Get(), MF_MT_FRAME_SIZE, width, height);
    CheckAndThrowHRException("Could not set video input media type frame size attribue");

    hr = MFSetAttributeRatio(mMediaType.Get(), MF_MT_FRAME_RATE, FPS_NUMERATOR, FPS_DENOMINATOR);
    CheckAndThrowHRException("Could not set video input media type frame rate attribue");

    hr = MFSetAttributeRatio(mMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    CheckAndThrowHRException("Could not set video input media type aspect ratio attribue");
}

inline void DesktopDuplicationMediaStream::InitializeStreamDescriptor()
{
    InitializeMainMediaType();

    auto hr = MFCreateStreamDescriptor((DWORD)mOutputIndex, 1, mMediaType.GetAddressOf(), mStreamDescriptor.GetAddressOf());
    CheckAndThrowHRException("Could not create media type stream descriptor");

    WinPtr<IMFMediaTypeHandler> mediaTypeHandler;
    hr = mStreamDescriptor->GetMediaTypeHandler(mediaTypeHandler.ReleaseAndGetAddressOf());
    CheckAndThrowHRException("Could not get media type handler from stream descriptor");

    hr = mediaTypeHandler->SetCurrentMediaType(mMediaType.Get());
    CheckAndThrowHRException("Could not set media type for media type handler");
}
