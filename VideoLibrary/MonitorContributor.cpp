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
#include "CaptureFrameStep.h"
#include "DxMultithread.h"
#include "MonitorContributor.h"
#include "RenderDirtyRectsStep.h"
#include "RenderMoveRectsStep.h"

namespace
{
    // Translate the duplicator's per-frame texture format into the
    // shader's color-space conversion mode. HDR-on Windows hands us FP16
    // scRGB linear when HDR content is present and BGRA8 (tone-mapped to
    // sRGB-ish) otherwise; HDR10 PQ Rec.2020 is included for the rare
    // R10G10B10A2 path.
    ColorSpaceCBData PickConversion(DXGI_FORMAT frameFormat, DXGI_COLOR_SPACE_TYPE outputColorSpace)
    {
        ColorSpaceCBData csParams{ ColorSpaceCBData::None, 240.0f, 0.0f, 0.0f };
        switch (frameFormat)
        {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            csParams.conversionMode = ColorSpaceCBData::ScRgbLinearToSrgb;
            break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            if (outputColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                || outputColorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020)
            {
                csParams.conversionMode = ColorSpaceCBData::Hdr10PqToSrgb;
            }
            break;
        default:
            // BGRA8 and anything else passes through.
            break;
        }
        return csParams;
    }
}

MonitorContributor::MonitorContributor(
    std::shared_ptr<ScreenDuplicator> duplicator,
    std::shared_ptr<SharedSurface> masterSharedSurface,
    RECT virtualDesktopBounds)
    : mDuplicator{ duplicator }
    , mGpuVertexBufferCapacity{ 0 }
    , mVirtualDesktopBounds{ virtualDesktopBounds }
{
    winrt::check_pointer(masterSharedSurface.get());

    auto device = mDuplicator->Device();

    // Cross-device case: open the master shared surface on this
    // duplicator's device so both sides can keyed-mutex around the
    // same texture memory. Same-device case: reuse the master surface
    // instance directly — the open call would still work but creates a
    // redundant SharedSurface wrapper.
    if (device.get() == masterSharedSurface->Device().get())
    {
        mSharedSurface = masterSharedSurface;
    }
    else
    {
        mSharedSurface = masterSharedSurface->OpenSharedSurfaceWithDevice(device);
    }

    mShaderCache = std::make_shared<ShaderCache>(device);
    mVertexBuffer = std::make_shared<std::vector<Vertex>>();

    // RTV onto this device's view of the shared surface. CreateRenderTargetView
    // is CPU-only (no GPU access), so it doesn't need to be inside the lock.
    winrt::check_hresult(device->CreateRenderTargetView(
        mSharedSurface->Texture(),
        nullptr,
        mRenderTargetView.put()));
}

MonitorContributor::~MonitorContributor() = default;

winrt::com_ptr<ID3D11Device> MonitorContributor::Device() const
{
    return mDuplicator->Device();
}

std::shared_ptr<Frame> MonitorContributor::Contribute()
{
    auto device = mDuplicator->Device();
    // MF interop with D3D11 requires the multi-thread protect interface
    // to be in the entered state during contexts that may interleave.
    DxMultithread multithread{ device.as<ID3D10Multithread>() };

    std::shared_ptr<Frame> frame;
    {
        auto lock = mSharedSurface->Lock();
        if (!lock->Locked())
        {
            return nullptr;
        }

        CaptureFrameStep captureFrame{ *mDuplicator };
        captureFrame.Perform();
        frame = captureFrame.Result();
        if (!frame->Captured())
        {
            // No new frame this tick (DDA timeout or occluded). The
            // shared surface keeps its previous content; pointer state
            // may still update via Frame's mutation hook on
            // DesktopPointer.
            return frame;
        }

        // Staging texture's format and dimensions come from this
        // duplicator's first captured desktop image — we can't pre-
        // allocate it because in HDR mode DDA may deliver FP16 or
        // BGRA8 depending on what's on screen, and the staging copy
        // must match.
        if (mStagingTexture == nullptr)
        {
            D3D11_TEXTURE2D_DESC stagingDesc;
            frame->DesktopImage()->GetDesc(&stagingDesc);
            stagingDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
            stagingDesc.MiscFlags = 0;
            winrt::check_hresult(device->CreateTexture2D(
                &stagingDesc, nullptr, mStagingTexture.put()));
        }

        RenderMoveRectsStep renderMoves{
            frame,
            mVirtualDesktopBounds,
            mStagingTexture,
            lock->TexturePtr()
        };
        renderMoves.Perform();

        ColorSpaceCBData csParams = PickConversion(frame->Format(), mDuplicator->ColorSpace());

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

    return frame;
}
