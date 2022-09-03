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
#include "VirtualDesktop.h"
#include "DesktopMonitor.h"
#include "RenderDirtyRectsStep.h"
#include "RenderMoveRectsStep.h"


RenderMoveRectsStep::RenderMoveRectsStep(
    std::shared_ptr<DesktopMonitor::ScreenDuplicator::Frame> frame,
    winrt::com_ptr<ID3D11Texture2D> stagingTexture,
    winrt::com_ptr<ID3D11Texture2D> sharedSurface)
    : mFrame { frame }
    , mStagingTexture{ stagingTexture }
    , mSharedSurface{ sharedSurface }
{
    if (mFrame == nullptr)
    {
        throw std::exception("null frame");
    }

    winrt::check_pointer(mStagingTexture.get());
    winrt::check_pointer(mSharedSurface.get());
}

RenderMoveRectsStep::~RenderMoveRectsStep()
{
}

void RenderMoveRectsStep::Perform()
{
    if (mFrame->MoveRectsCount() == 0)
    {
        return;
    }

    std::shared_ptr<VirtualDesktop> virtualDesktop = mFrame->VirtualDesktop();
    DXGI_OUTDUPL_MOVE_RECT* moveRects = mFrame->MoveRects();
    const RECT virtualDesktopBounds = virtualDesktop->VirtualDesktopBounds();
    const LONG offsetX = virtualDesktopBounds.left;
    const LONG offsetY = virtualDesktopBounds.top;

    D3D11_TEXTURE2D_DESC desktopImageDesc;
    mFrame->DesktopImage()->GetDesc(&desktopImageDesc);
    const LONG desktopWidth = (LONG)desktopImageDesc.Width;
    const LONG desktopHeight = (LONG)desktopImageDesc.Height;

    winrt::com_ptr<ID3D11Device> device;
    mSharedSurface->GetDevice(device.put());

    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());

    for (size_t i = 0; i < mFrame->MoveRectsCount(); ++i) {
        const DXGI_OUTDUPL_MOVE_RECT& moveRect = moveRects[i];

        RECT srcRect{}, dstRect{};

        // set src and dstRect based on rotation of output device
        switch (mFrame->Rotation())
        {
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        case DXGI_MODE_ROTATION_IDENTITY:
            srcRect.left = moveRect.SourcePoint.x;
            srcRect.top = moveRect.SourcePoint.y;
            srcRect.bottom = moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top;
            srcRect.right = moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left;
            dstRect = moveRect.DestinationRect;
            break;

        case DXGI_MODE_ROTATION_ROTATE90:
            srcRect.left = desktopHeight - (moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top);
            srcRect.top = moveRect.SourcePoint.x;
            srcRect.right = desktopHeight - moveRect.SourcePoint.y;
            srcRect.bottom = moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left;

            dstRect.left = desktopHeight - moveRect.DestinationRect.bottom;
            dstRect.top = moveRect.DestinationRect.left;
            dstRect.right = desktopHeight - moveRect.DestinationRect.top;
            dstRect.bottom = moveRect.DestinationRect.right;
            break;

        case DXGI_MODE_ROTATION_ROTATE180:
            srcRect.left = desktopWidth - (moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left);
            srcRect.top = desktopHeight - (moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top);
            srcRect.right = desktopWidth - moveRect.SourcePoint.x;
            srcRect.bottom = desktopHeight - moveRect.SourcePoint.y;

            dstRect.left = desktopWidth - moveRect.DestinationRect.right;
            dstRect.top = desktopHeight - moveRect.DestinationRect.bottom;
            dstRect.right = desktopWidth - moveRect.DestinationRect.left;
            dstRect.bottom = desktopHeight - moveRect.DestinationRect.top;
            break;

        case DXGI_MODE_ROTATION_ROTATE270:
            srcRect.left = moveRect.SourcePoint.x;
            srcRect.top = desktopWidth - (moveRect.SourcePoint.x + moveRect.DestinationRect.right - moveRect.DestinationRect.left);
            srcRect.right = moveRect.SourcePoint.y + moveRect.DestinationRect.bottom - moveRect.DestinationRect.top;
            srcRect.bottom = desktopWidth - moveRect.SourcePoint.x;

            dstRect.left = moveRect.DestinationRect.top;
            dstRect.top = desktopWidth - moveRect.DestinationRect.right;
            dstRect.right = moveRect.DestinationRect.bottom;
            dstRect.bottom = desktopWidth - moveRect.DestinationRect.left;
            break;

        default:
            throw std::exception("Move rect rotation unimplemented");
        }

        // copy rect from shared surface to move surface, keeping same position
        const auto desktopCoordinates = mFrame->DesktopMonitorBounds();
        D3D11_BOX box;
        box.left = desktopCoordinates.left + srcRect.left - offsetX;
        box.right = desktopCoordinates.left + srcRect.right - offsetX;
        box.top = desktopCoordinates.top + srcRect.top - offsetY;
        box.bottom = desktopCoordinates.top + srcRect.bottom - offsetY;
        box.front = 0;
        box.back = 1;

        context->CopySubresourceRegion(
            mStagingTexture.get(), 0,
            srcRect.left,
            srcRect.top,
            0,
            mSharedSurface.get(), 0,
            &box
        );

        // copy from move surface to new relative position in shared surface
        box.left = srcRect.left;
        box.right = srcRect.right;
        box.top = srcRect.top;
        box.bottom = srcRect.bottom;
        box.front = 0;
        box.back = 1;
        context->CopySubresourceRegion(
            mSharedSurface.get(), 0,
            desktopCoordinates.left + dstRect.left - offsetX,
            desktopCoordinates.top + dstRect.top - offsetY,
            0,
            mStagingTexture.get(), 0,
            &box
        );
    }
}
