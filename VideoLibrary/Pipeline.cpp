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
#include "DxMultithread.h"
#include "CaptureFrameStep.h"
#include "RenderMoveRectsStep.h"
#include "RenderDirtyRectsStep.h"
#include "RenderPointerTextureStep.h"
#include "TextureToMediaSampleStep.h"
#include "Pipeline.h"

Pipeline::Pipeline(
    std::shared_ptr<ScreenDuplicator> duplicator,
    std::shared_ptr<SharedSurface> sharedSurface,
    RECT virtualDesktopBounds
)
    : mDuplicator{ duplicator }
    , mSharedSurface{ sharedSurface }
    , mGpuVertexBufferCapacity{ 0 }
    , mVirtualDesktopBounds{ virtualDesktopBounds }
    , mPresentationTimeBaselineQpc{ 0 }
    , mPresentationTimeBaselineSet{ false }
{
    QueryPerformanceFrequency(&mQpcFrequency);
    if (mDuplicator == nullptr)
    {
        throw std::exception("Null duplicator");
    }

    winrt::check_pointer(mSharedSurface.get());
    mShaderCache = std::make_shared<ShaderCache>(mDuplicator->Device());
    mVertexBuffer = std::make_shared<std::vector<Vertex>>();

    // Resources that don't depend on the first captured frame are
    // allocated up front so Perform() stays focused on per-frame work
    // and doesn't carry lazy-init branches. The staging texture's
    // format/size come from the duplicator's desktop image (which we
    // don't see until first frame), so it stays lazy below.
    AllocateTexturePool();
    winrt::check_hresult(mDuplicator->Device()->CreateRenderTargetView(
        mSharedSurface->Texture(),
        nullptr,
        mRenderTargetView.put()));
}

Pipeline::~Pipeline()
{
}

void Pipeline::Perform()
{
    mSample = nullptr;
    auto device = mDuplicator->Device();
    // need to use multithread protect because of Media Foundation api
    // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-resetdevice#remarks
    DxMultithread multithread{ device.as<ID3D10Multithread>() };
    int64_t framePresentationTimeQpc = 0;
    {
        auto lock = mSharedSurface->Lock();

        if (!lock->Locked())
        {
            return;
        }

        CaptureFrameStep captureFrame{ *mDuplicator };
        captureFrame.Perform();

        std::shared_ptr<Frame> frame = captureFrame.Result();
        mDesktopMonitorBounds = frame->DesktopMonitorBounds();
        framePresentationTimeQpc = frame->PresentationTime();
        if (frame->Captured())
        {
            // Staging texture's desc comes from the first captured frame's
            // desktop image — that's the one resource we can't allocate
            // at ctor time without a frame in hand.
            if (mStagingTexture == nullptr)
            {
                D3D11_TEXTURE2D_DESC stagingDesc;
                frame->DesktopImage()->GetDesc(&stagingDesc);
                stagingDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
                stagingDesc.MiscFlags = 0;
                AllocateStagingTexture(device, stagingDesc);
            }

            RenderMoveRectsStep renderMoves{
                frame,
                mVirtualDesktopBounds,
                mStagingTexture,
                lock->TexturePtr()
            };

            renderMoves.Perform();

            // Pick the conversion based on the per-frame texture format.
            // With DuplicateOutput1 in the duplicator we now get the
            // native framebuffer format: FP16 (scRGB) under HDR, BGRA8
            // (sRGB) under SDR. Default sdrWhiteNits to 240 — a common
            // "SDR content brightness" value on HDR-capable laptops;
            // ideally we'd query DISPLAYCONFIG_SDR_WHITE_LEVEL but a
            // sensible constant is close enough as a first cut.
            ColorSpaceCBData csParams{ ColorSpaceCBData::None, 240.0f, 0.0f, 0.0f };
            switch (frame->Format())
            {
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
                csParams.conversionMode = ColorSpaceCBData::ScRgbLinearToSrgb;
                break;
            case DXGI_FORMAT_R10G10B10A2_UNORM:
                if (mDuplicator->ColorSpace() == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                    || mDuplicator->ColorSpace() == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020)
                {
                    csParams.conversionMode = ColorSpaceCBData::Hdr10PqToSrgb;
                }
                break;
            default:
                // BGRA8 (and anything else) passes through. For BGRA8 in
                // HDR-on mode DDA has already mapped to a sRGB-ish 8-bit
                // surface; passthrough produces correct colors.
                break;
            }

            RenderDirtyRectsStep renderDirty{
                frame,
                mVirtualDesktopBounds,
                mVertexBuffer,
                mGpuVertexBuffer,
                mGpuVertexBufferCapacity,
                mShaderCache,
                lock->TexturePtr(),
                mRenderTargetView,
                csParams
            };
            renderDirty.Perform();
        }
    }

    RenderPointerTextureStep renderPointer{
        mDuplicator->DesktopPointerPtr(),
        mSharedSurface,
        mDuplicator->Device(),
        mShaderCache,
        mTexturePool,
        mVirtualDesktopBounds,
        mDesktopMonitorBounds
    };
    renderPointer.Perform();

    if (renderPointer.Result() == nullptr)
    {
        return;
    }

    winrt::com_ptr<ID3D11Texture2D> desktopTexture = renderPointer.Result();
    
    TextureToMediaSampleStep convertTexture{
        desktopTexture,
        mTexturePool
    };
    convertTexture.Perform();

    mSample = convertTexture.Result();

    // Tag the sample with the GPU present time of the frame this output
    // is derived from. The sink writer treats sample time as authoritative
    // when set, so dropped frames don't compress the encoded timeline
    // (which they would with a wall-clock-at-write strategy).
    if (mSample && framePresentationTimeQpc > 0 && mQpcFrequency.QuadPart > 0)
    {
        if (!mPresentationTimeBaselineSet)
        {
            mPresentationTimeBaselineQpc = framePresentationTimeQpc;
            mPresentationTimeBaselineSet = true;
        }
        int64_t elapsedQpc = framePresentationTimeQpc - mPresentationTimeBaselineQpc;
        int64_t qpf = mQpcFrequency.QuadPart;
        // Split-multiply to avoid overflowing int64 on long recordings
        // when qpf is not a power-of-10 divisor of 10_000_000.
        int64_t sampleTime100ns = (elapsedQpc / qpf) * 10'000'000
            + (elapsedQpc % qpf) * 10'000'000 / qpf;
        winrt::check_hresult(mSample->SetSampleTime(sampleTime100ns));
    }
}

winrt::com_ptr<IMFSample> Pipeline::Sample() const
{
    return mSample;
}

void Pipeline::AllocateTexturePool()
{
    D3D11_TEXTURE2D_DESC desc = mSharedSurface->Desc();
    // Use the same device that was used to open the shared surface
    // instead of the device used by Desktop Duplication API's desktop image.
    mTexturePool.attach(new TexturePool(mDuplicator->Device(), desc));
    winrt::check_pointer(mTexturePool.get());
}

void Pipeline::AllocateStagingTexture(winrt::com_ptr<ID3D11Device> device, const D3D11_TEXTURE2D_DESC& desc)
{
    winrt::check_hresult(device->CreateTexture2D(
        &desc,
        nullptr,
        mStagingTexture.put()));
}
