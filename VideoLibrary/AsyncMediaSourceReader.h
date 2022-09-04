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

class AsyncMediaSourceReader : public IMFSourceReaderCallback
{
public:

    AsyncMediaSourceReader(
        winrt::com_ptr<IMFMediaSource> mediaSource,
        std::function<void(IMFSample*, HRESULT)> callback,
        int desiredFrameRate,
        DWORD streamIndex);

    virtual ~AsyncMediaSourceReader();

    virtual void Start();
    virtual void Stop();
    virtual void RequestSample();

    // Inherited via IMFSourceReaderCallback2
    virtual HRESULT __stdcall OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample * pSample) noexcept override;
    virtual HRESULT __stdcall OnFlush(DWORD dwStreamIndex) noexcept override;
    virtual HRESULT __stdcall OnEvent(DWORD dwStreamIndex, IMFMediaEvent * pEvent) noexcept override;
    virtual HRESULT __stdcall OnTransformChange(void) noexcept;
     
    virtual HRESULT __stdcall OnStreamError(DWORD dwStreamIndex, HRESULT hrStatus) noexcept;

     STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_refCount); }
     STDMETHODIMP_(ULONG) Release()
     {
         assert(m_refCount > 0);
         ULONG uCount = InterlockedDecrement(&m_refCount);
         if (uCount == 0)
         {
             delete this;
         }
         return uCount;
     }
     virtual HRESULT QueryInterface(REFIID riid, void** ppv) noexcept override;

private:


    winrt::com_ptr<IMFMediaSource> mSource;

    /*
    https://docs.microsoft.com/en-us/windows/desktop/medfound/mf-readwrite-enable-hardware-transforms
    By default, the source reader and sink writer do not use hardware decoders or encoders.
    To enable the use of hardware MFTs, set this attribute to TRUE
    when you create the source reader or sink writer.

    https://docs.microsoft.com/en-us/windows/desktop/medfound/source-reader-attributes
    https://docs.microsoft.com/en-us/windows/desktop/medfound/sink-writer-attributes

    There is one exception to the default behavior.
    The source reader and sink writer automatically use MFTs
    that are registered locally in the caller's process.
    To register an MFT locally, call MFTRegisterLocal or MFTRegisterLocalByCLSID.
    Hardware MFTs that are registered locally are used even if
    the MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS attribute is not set.

    see MFCaptureToFile to see how to use MFTRegisterLocalByCLSID to use Color Converter DSP
    */
    winrt::com_ptr<IMFSourceReader> mReader;
    std::atomic<bool> mStopping = false;
    std::mutex mMutex;
    int mSamplingDelayMs;
    DWORD mStreamIndex;

    //std::function<void(winrt::com_ptr<IMFSample>)> mCallback;
    std::function<void(IMFSample*, HRESULT)> mCallback;

    volatile long   m_refCount;
};