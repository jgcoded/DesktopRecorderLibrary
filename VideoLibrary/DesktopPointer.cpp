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
#include "DesktopPointer.h"


DesktopPointer::DesktopPointer()
    : mIsPointerTextureStale { true }
    , mPointerOwnerIndex { UINT_MAX }
    , mLastUpdateTime { 0 }
    , mPosition{}
    , mShapeInfo{}
    , mVisible{ false }
{
}

DesktopPointer::~DesktopPointer()
{
}

std::size_t DesktopPointer::BufferSize() const
{
    return mBuffer.size();
}

byte* DesktopPointer::PutBuffer(std::size_t requiredSize)
{
    mIsPointerTextureStale = true;

    if (requiredSize > mBuffer.size()) {
        mBuffer.resize(requiredSize);
    }

    return mBuffer.data();
}

DXGI_OUTDUPL_POINTER_POSITION DesktopPointer::Position() const
{
    return mPosition;
}

void DesktopPointer::UpdatePosition(
    DXGI_OUTDUPL_POINTER_POSITION newPosition,
    LARGE_INTEGER updateTime,
    UINT outputIndex)
{
    if (updateTime.QuadPart == 0) {
        return;
    }

    // In a multi-monitor recording scenario, need to figure out which output is showing the most recent pointer

    // update pointer position if the pointer is visible or on this output
    bool shouldUpdatePosition =
        newPosition.Visible ||
        mPointerOwnerIndex == outputIndex;

    // update mouse position if two outputs have visible pointers, but this output has a more recent time
    shouldUpdatePosition = shouldUpdatePosition ||
           (newPosition.Visible &&
            mPosition.Visible &&
            mPointerOwnerIndex != outputIndex &&
            updateTime.QuadPart > mLastUpdateTime.QuadPart);

    if (shouldUpdatePosition) {
        mLastUpdateTime = updateTime;
        mPosition = newPosition;
        mPointerOwnerIndex = outputIndex;
        mVisible = newPosition.Visible != 0;
    }
}

DXGI_OUTDUPL_POINTER_SHAPE_INFO DesktopPointer::ShapeInfo() const
{
    return mShapeInfo;
}

void DesktopPointer::ShapeInfo(DXGI_OUTDUPL_POINTER_SHAPE_INFO newShapeInfo)
{
    mShapeInfo = newShapeInfo;
}

void DesktopPointer::UpdateTexture(winrt::com_ptr<ID3D11Texture2D> const& newImage)
{
    if (mPointerTexture) {
        mPointerTexture = nullptr;
    }
    mPointerTexture = newImage;
    mIsPointerTextureStale = false;
}

winrt::com_ptr<ID3D11Texture2D> DesktopPointer::Texture() const
{
    return mPointerTexture;
}

bool DesktopPointer::Visible() const
{
    return mVisible;
}
