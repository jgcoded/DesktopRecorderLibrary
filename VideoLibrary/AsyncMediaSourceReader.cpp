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
#include "ScreenMediaSinkWriter.h"
#include "DesktopMonitor.h"
#include "AsyncMediaSourceReader.h"

AsyncMediaSourceReader::AsyncMediaSourceReader(
    winrt::com_ptr<IMFMediaSource> mediaSource,
    std::function<void(IMFSample*, HRESULT)> callback,
    int samplingDelayMs,
    DWORD streamIndex)
    : mSource{ mediaSource }
    , mCallback{ callback }
    , mSamplingDelayMs{ samplingDelayMs }
    , mStreamIndex{ streamIndex }
    , m_refCount{ 1 }
{
}

AsyncMediaSourceReader::~AsyncMediaSourceReader()
{
    assert(m_refCount == 0);

    if (!mStopping)
    {
        this->Stop();
    }
}

void AsyncMediaSourceReader::Start()
{
    winrt::com_ptr<IMFAttributes> attributes;
    winrt::check_hresult(MFCreateAttributes(attributes.put(), 1));

    winrt::com_ptr<IUnknown> unk;
    winrt::check_hresult(this->QueryInterface(IID_PPV_ARGS(unk.put())));

    winrt::check_hresult(attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, unk.get()));

    winrt::com_ptr<IMFMediaSource> source = mSource.as<IMFMediaSource>();
    winrt::check_hresult(MFCreateSourceReaderFromMediaSource(source.get(), attributes.get(), mReader.put()));

    winrt::check_hresult(mReader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE));

    winrt::check_hresult(mReader->SetStreamSelection(mStreamIndex, TRUE));

    this->RequestSample();
}

void AsyncMediaSourceReader::Stop()
{
    mStopping = true;
}

HRESULT __stdcall AsyncMediaSourceReader::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample * pSample) noexcept
{
    UNREFERENCED_PARAMETER(llTimestamp);
    UNREFERENCED_PARAMETER(dwStreamFlags);
    UNREFERENCED_PARAMETER(dwStreamIndex);
    UNREFERENCED_PARAMETER(dwStreamFlags);

    if (FAILED(hrStatus))
    {
        mCallback(nullptr, hrStatus);
    }

    try
    {
        if (mStopping)
        {
            return S_OK;
        }

        if (mCallback && pSample)
        {
            mCallback(pSample, S_OK);
        }

        Sleep(mSamplingDelayMs);

        if (!mStopping)
        {
            this->RequestSample();
        }
    }
    catch (...)
    {
        if(!mStopping)
        return winrt::to_hresult();
    }

    return S_OK;
}

inline HRESULT __stdcall AsyncMediaSourceReader::OnFlush(DWORD dwStreamIndex) noexcept
{
    UNREFERENCED_PARAMETER(dwStreamIndex);
    return E_NOTIMPL;
}

HRESULT __stdcall AsyncMediaSourceReader::OnEvent(DWORD dwStreamIndex, IMFMediaEvent * pEvent) noexcept
{
    UNREFERENCED_PARAMETER(dwStreamIndex);
    UNREFERENCED_PARAMETER(pEvent);
    return S_OK;
}

inline HRESULT __stdcall AsyncMediaSourceReader::OnTransformChange(void) noexcept
{
    return E_NOTIMPL;
}

inline HRESULT __stdcall AsyncMediaSourceReader::OnStreamError(DWORD dwStreamIndex, HRESULT hrStatus) noexcept
{
    UNREFERENCED_PARAMETER(dwStreamIndex);
    UNREFERENCED_PARAMETER(hrStatus);
    return E_NOTIMPL;
}

HRESULT AsyncMediaSourceReader::QueryInterface(REFIID riid, void ** ppv) noexcept
{
    static const QITAB qit[] =
    {
        QITABENT(AsyncMediaSourceReader, IMFSourceReaderCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

void AsyncMediaSourceReader::RequestSample()
{
    std::scoped_lock<std::mutex> lock{ mMutex };
    winrt::check_hresult(mReader->ReadSample(mStreamIndex, 0, nullptr, nullptr, nullptr, nullptr));
}
