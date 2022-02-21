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

#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "..\VideoLibrary\DxResource.h"
#include <wmcodecdsp.h>
#include <uuids.h>
#include <dmort.h>

// Msdmo.lib;dmoguids.lib;uuid.lib;amstrmid.lib;wmcodecdspuuid.lib;%(AdditionalDependencies)
#pragma comment(lib, "msdmo")
#pragma comment(lib, "dmoguids")
#pragma comment(lib, "uuid")
#pragma comment(lib, "amstrmid")
#pragma comment(lib, "wmcodecdspuuid")


namespace VideoLibraryTests
{
    class CBaseMediaBuffer : public IMediaBuffer {
    public:
        CBaseMediaBuffer() {}
        CBaseMediaBuffer(BYTE *pData, ULONG ulSize, ULONG ulData) :
            m_pData(pData), m_ulSize(ulSize), m_ulData(ulData), m_cRef(1) {}
        STDMETHODIMP_(ULONG) AddRef() {
            return InterlockedIncrement((long*)&m_cRef);
        }
        STDMETHODIMP_(ULONG) Release() {
            long l = InterlockedDecrement((long*)&m_cRef);
            if (l == 0)
                delete this;
            return l;
        }
        STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
            if (riid == IID_IUnknown) {
                AddRef();
                *ppv = (IUnknown*)this;
                return NOERROR;
            }
            else if (riid == IID_IMediaBuffer) {
                AddRef();
                *ppv = (IMediaBuffer*)this;
                return NOERROR;
            }
            else
                return E_NOINTERFACE;
        }
        STDMETHODIMP SetLength(DWORD ulLength) { m_ulData = ulLength; return NOERROR; }
        STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength) { *pcbMaxLength = m_ulSize; return NOERROR; }
        STDMETHODIMP GetBufferAndLength(BYTE **ppBuffer, DWORD *pcbLength) {
            if (ppBuffer) *ppBuffer = m_pData;
            if (pcbLength) *pcbLength = m_ulData;
            return NOERROR;
        }
    protected:
        BYTE *m_pData;
        ULONG m_ulSize;
        ULONG m_ulData;
        ULONG m_cRef;
    };

    class CStaticMediaBuffer : public CBaseMediaBuffer {
    public:
        STDMETHODIMP_(ULONG) AddRef() { return 2; }
        STDMETHODIMP_(ULONG) Release() { return 1; }
        void Init(BYTE *pData, ULONG ulSize, ULONG ulData) {
            m_pData = pData;
            m_ulSize = ulSize;
            m_ulData = ulData;
        }
    };

    TEST_CLASS(DxResourceTests)
    {
    public:

        TEST_METHOD(CreateDevice)
        {
            winrt::check_hresult(MFStartup(MF_VERSION));
            CoInitialize(NULL);
            //winrt::init_apartment();

            winrt::com_ptr<IMediaObject> voiceCaptureDsp;
            winrt::check_hresult(CoCreateInstance(CLSID_CWMAudioAEC, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(voiceCaptureDsp.put())));

            auto properties = voiceCaptureDsp.as<IPropertyStore>();

            // set Audio echo cancellation and Microphone array processing modes
            PROPVARIANT prop{};
            PropVariantInit(&prop);
            prop.vt = VT_I4;
            prop.lVal = 5;
            winrt::check_hresult(properties->SetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, prop));
            PropVariantClear(&prop);

            // Turn on feature modes
            PROPVARIANT pvFeatrModeOn;
            PropVariantInit(&pvFeatrModeOn);
            pvFeatrModeOn.vt = VT_BOOL;
            pvFeatrModeOn.boolVal = (VARIANT_BOOL)-1;
            winrt::check_hresult(properties->SetValue(MFPKEY_WMAAECMA_FEATURE_MODE, pvFeatrModeOn));
            PropVariantClear(&pvFeatrModeOn);

            // Turn on/off noise suppression
            PROPVARIANT pvNoiseSup;
            PropVariantInit(&pvNoiseSup);
            pvNoiseSup.vt = VT_I4;
            pvNoiseSup.lVal = (LONG)1;
            winrt::check_hresult(properties->SetValue(MFPKEY_WMAAECMA_FEATR_NS, pvNoiseSup));
            PropVariantClear(&pvNoiseSup);

            // Turn on/off AGC
            PROPVARIANT pvAGC;
            PropVariantInit(&pvAGC);
            pvAGC.vt = VT_BOOL;
            pvAGC.boolVal = (VARIANT_BOOL)-1;
            winrt::check_hresult(properties->SetValue(MFPKEY_WMAAECMA_FEATR_AGC, pvAGC));
            PropVariantClear(&pvAGC);

            // Turn on/off center clip
            PROPVARIANT pvCntrClip;
            PropVariantInit(&pvCntrClip);
            pvCntrClip.vt = VT_BOOL;
            pvCntrClip.boolVal = (VARIANT_BOOL)-1;
            winrt::check_hresult(properties->SetValue(MFPKEY_WMAAECMA_FEATR_CENTER_CLIP, pvCntrClip));
            PropVariantClear(&pvCntrClip);


            DMO_MEDIA_TYPE mt;  // Media type.
            mt.majortype = MEDIATYPE_Audio;
            mt.subtype = MEDIASUBTYPE_PCM;
            mt.lSampleSize = 0;
            mt.bFixedSizeSamples = TRUE;
            mt.bTemporalCompression = FALSE;
            mt.formattype = FORMAT_WaveFormatEx;

            // Allocate the format block to hold the WAVEFORMATEX structure.
            winrt::check_hresult(MoInitMediaType(&mt, sizeof(WAVEFORMATEX)));

            WAVEFORMATEX *pwav = (WAVEFORMATEX*)mt.pbFormat;
            pwav->wFormatTag = WAVE_FORMAT_PCM;
            pwav->nChannels = 1;
            pwav->nSamplesPerSec = 16000;
            pwav->nAvgBytesPerSec = 32000;
            pwav->nBlockAlign = 2;
            pwav->wBitsPerSample = 16;
            pwav->cbSize = 0;

            // Set the output type.
            winrt::check_hresult(voiceCaptureDsp->SetOutputType(0, &mt, 0));
            // Free the format block.
            MoFreeMediaType(&mt);

            winrt::check_hresult(voiceCaptureDsp->AllocateStreamingResources());

            CStaticMediaBuffer outputBuffer;
            DMO_OUTPUT_DATA_BUFFER OutputBufferStruct = { 0 };
            OutputBufferStruct.pBuffer = &outputBuffer;
            DWORD status = 0;
            int i = 0;
            WAVEFORMATEX wfxOut = { WAVE_FORMAT_PCM, 1, 16000, 32000, 2, 16, 0 };

            auto cOutputBufLen = wfxOut.nSamplesPerSec * wfxOut.nBlockAlign;
            auto pbOutputBuffer = new BYTE[cOutputBufLen];

            while (i++ < 100)
            {
                outputBuffer.Init((byte*)pbOutputBuffer, cOutputBufLen, 0);
                OutputBufferStruct.dwStatus = 0;
                auto hr = voiceCaptureDsp->ProcessOutput(0, 1, &OutputBufferStruct, &status);
                DWORD c = 0;
                outputBuffer.GetBufferAndLength(nullptr, &c);
                printf("%d %d", c, status);
                Sleep(30);
            }

            delete[] (pbOutputBuffer);
            
        }
    };
}