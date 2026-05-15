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
            if (mTexturePool == nullptr)
            {
                AllocateTexturePool();
            }

            if (mStagingTexture == nullptr)
            {
                D3D11_TEXTURE2D_DESC stagingDesc;
                frame->DesktopImage()->GetDesc(&stagingDesc);
                stagingDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
                stagingDesc.MiscFlags = 0;
                AllocateStagingTexture(device, stagingDesc);
            }

            if (mRenderTargetView == nullptr)
            {
                winrt::check_hresult(mDuplicator->Device()->CreateRenderTargetView(
                    lock->TexturePtr(),
                    nullptr,
                    mRenderTargetView.put()
                ));
            }

            RenderMoveRectsStep renderMoves{
                frame,
                mVirtualDesktopBounds,
                mStagingTexture,
                lock->TexturePtr()
            };

            renderMoves.Perform();

            // Conversion temporarily disabled while we collect actual
            // ColorSpace + texture-format values from the user's HDR
            // setup. The previous mapping (HDR10 -> mode 2 PQ decode)
            // made the picture worse than the SDR-tagged baseline on the
            // user's machine, which means the shader's assumption about
            // PQ-encoded input doesn't hold for their DDA delivery
            // format. Once we know what DDA actually hands us we'll
            // reinstate the correct branch.
            ColorSpaceCBData csParams{ ColorSpaceCBData::None, 100.0f, 0.0f, 0.0f };

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
