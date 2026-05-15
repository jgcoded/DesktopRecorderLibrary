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
#include "Errors.h"
#include "Frame.h"

Frame::Frame(ScreenDuplicator& duplicator)
    : mDupl{ duplicator.Duplication()}
    , mCaptured{ false }
    , mRectBuffer{ duplicator.Buffer()}
    , mNumMoveRects{ 0 }
    , mNumDirtyRects{ 0 }
    , mMoveRects{ nullptr }
    , mDirtyRects{ nullptr }
    , mDesktopMonitorBounds{ }
    , mFormat{ DXGI_FORMAT_UNKNOWN }
    , mRotation{ DXGI_MODE_ROTATION_UNSPECIFIED }
{
    try
    {
        winrt::com_ptr<IDXGIResource> desktopImageResource;
        HRESULT hr = mDupl->AcquireNextFrame(1, &mFrameInfo, desktopImageResource.put());

        if (hr == DXGI_ERROR_WAIT_TIMEOUT || hr == DXGI_STATUS_OCCLUDED)
        {
            return;
        }

        winrt::check_hresult(hr);

        mCaptured = true;
        mFrameTexture = desktopImageResource.as<ID3D11Texture2D>();

        // Capture the DXGI format so Pipeline can pick a per-frame
        // color conversion. DDA flips between BGRA8 and FP16 when HDR
        // is enabled depending on whether any HDR content is on screen.
        D3D11_TEXTURE2D_DESC td{};
        mFrameTexture->GetDesc(&td);
        mFormat = td.Format;

        DXGI_OUTPUT_DESC desc;
        winrt::check_hresult(duplicator.Output()->GetDesc(&desc));
        mDesktopMonitorBounds = desc.DesktopCoordinates;
        mRotation = desc.Rotation;

        // Pointer state lives on DesktopPointer; let it own its own
        // refresh from the frame info we just captured rather than
        // reaching in to mutate it from Frame.
        duplicator.DesktopPointerPtr()->UpdateFromFrame(
            mFrameInfo,
            mDupl.get(),
            duplicator.OutputIndex(),
            mDesktopMonitorBounds);

        // get frame metadata
        if (mFrameInfo.TotalMetadataBufferSize != 0) {

            UINT totalBufferSize = mFrameInfo.TotalMetadataBufferSize;
            if (totalBufferSize > mRectBuffer->size()) {
                mRectBuffer->resize(totalBufferSize);
            }

            UINT moveRectsBufferSize = 0;
            winrt::check_hresult(mDupl->GetFrameMoveRects(
                totalBufferSize,
                reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(mRectBuffer->data()),
                &moveRectsBufferSize));

            UINT dirtyRectsBufferSize = totalBufferSize - moveRectsBufferSize;

            winrt::check_hresult(mDupl->GetFrameDirtyRects(
                dirtyRectsBufferSize,
                reinterpret_cast<RECT*>(mRectBuffer->data() + moveRectsBufferSize),
                &dirtyRectsBufferSize));

            mNumMoveRects = moveRectsBufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);
            mNumDirtyRects = dirtyRectsBufferSize / sizeof(RECT);
        }
    }
    catch (...)
    {
        HRESULT hr = winrt::to_hresult();
        ThrowExceptionCheckRecoverable(duplicator.Device(), FrameInfoExpectedErrors, hr);
    }
}

Frame::~Frame()
{
    if (mDupl)
    {
        try
        {
            (void)mDupl->ReleaseFrame();
        }
        catch (...)
        {
        }
    }
}

winrt::com_ptr<ID3D11Texture2D> Frame::DesktopImage() const
{
    return mFrameTexture;
}

RECT Frame::DesktopMonitorBounds() const
{
    return mDesktopMonitorBounds;
}

int64_t Frame::PresentationTime() const
{
    // Raw QPC ticks of when the GPU presented this frame. Convert via
    // QueryPerformanceFrequency. Returns 0 when DDA didn't report a
    // present time (e.g. AcquireNextFrame timeout).
    return mFrameInfo.LastPresentTime.QuadPart;
}

bool Frame::Captured() const
{
    return mCaptured;
}

DXGI_MODE_ROTATION Frame::Rotation() const { return mRotation; }

DXGI_OUTDUPL_MOVE_RECT* Frame::MoveRects() const
{
    return reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(mRectBuffer->data());
}

RECT* Frame::DirtyRects() const
{
    return reinterpret_cast<RECT*>(mRectBuffer->data() + (mNumMoveRects * sizeof(DXGI_OUTDUPL_MOVE_RECT)));
}

size_t Frame::MoveRectsCount() const { return mNumMoveRects; }

size_t Frame::DirtyRectsCount() const { return mNumDirtyRects; }
