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
    , mVirtualDesktopBounds{ virtualDesktopBounds }
{
    if (mDuplicator == nullptr)
    {
        throw std::exception("Null duplicator");
    }

    mSharedSurface = sharedSurface->OpenSharedSurfaceWithDevice(duplicator->Device());
    winrt::check_pointer(mSharedSurface.get());

    {
        auto lock = mSharedSurface->Lock();
        assert(lock->Locked());
        if (lock->Locked())
        {
            winrt::check_hresult(mDuplicator->Device()->CreateRenderTargetView(
                lock->TexturePtr(),
                nullptr,
                mRenderTargetView.put()
            ));
        }
    }

    mShaderCache = std::make_shared<ShaderCache>(mDuplicator->Device());
    mVertexBuffer = std::make_shared<std::vector<Vertex>>();

}

Pipeline::~Pipeline()
{
}

void Pipeline::Perform()
{
    mSample == nullptr;
    auto device = mDuplicator->Device();
    // need to use multithread protect because of Media Foundation api
    // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-resetdevice#remarks
    DxMultithread multithread{ device.as<ID3D10Multithread>() };
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

            RenderMoveRectsStep renderMoves{
                frame,
                mVirtualDesktopBounds,
                mStagingTexture,
                lock->TexturePtr()
            };

            renderMoves.Perform();

            RenderDirtyRectsStep renderDirty{
                frame,
                mVirtualDesktopBounds,
                mVertexBuffer,
                mShaderCache,
                lock->TexturePtr(),
                mRenderTargetView
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
