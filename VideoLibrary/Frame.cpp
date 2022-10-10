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

        // Don't care about move or dirty rects, just get the pointer data and update the pointer cache
        if (mFrameInfo.LastMouseUpdateTime.QuadPart != 0 && mFrameInfo.PointerShapeBufferSize != 0) {

            UINT requiredBufferSize;
            DXGI_OUTDUPL_POINTER_SHAPE_INFO pointerInfo;
            winrt::check_hresult(mDupl->GetFramePointerShape(mFrameInfo.PointerShapeBufferSize,
                reinterpret_cast<void*>(duplicator.DesktopPointerPtr()->PutBuffer(mFrameInfo.PointerShapeBufferSize)),
                &requiredBufferSize,
                &pointerInfo));

            duplicator.DesktopPointerPtr()->ShapeInfo(pointerInfo);
        }

        DXGI_OUTPUT_DESC desc;
        winrt::check_hresult(duplicator.Output()->GetDesc(&desc));
        mDesktopMonitorBounds = desc.DesktopCoordinates;
        mRotation = desc.Rotation;

        duplicator.DesktopPointerPtr()->UpdatePosition(
            mFrameInfo.PointerPosition,
            mFrameInfo.LastMouseUpdateTime,
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
    //    LARGE_INTEGER frequency;
      //  QueryPerformanceFrequency(&frequency);

    int64_t nanoSeconds = mFrameInfo.LastPresentTime.QuadPart;

    // ticks / ticks per second = seconds
    // save precision by dividing first and then multipling by 1e9 (1e9 ns in one sec)

  //  nanoSeconds /= frequency.QuadPart;
  //  nanoSeconds *= 1'000'000'000;


    return nanoSeconds;
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
