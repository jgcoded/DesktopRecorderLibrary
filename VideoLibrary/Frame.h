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

#include "ScreenDuplicator.h"

class Frame
{
public:
    Frame(ScreenDuplicator& duplicator);
    ~Frame();

    winrt::com_ptr<ID3D11Texture2D> DesktopImage() const;

    RECT DesktopMonitorBounds() const;

    int64_t PresentationTime() const;

    bool Captured() const;

    DXGI_MODE_ROTATION Rotation() const;

    DXGI_OUTDUPL_MOVE_RECT* MoveRects() const;

    RECT* DirtyRects() const;

    size_t MoveRectsCount() const;

    size_t DirtyRectsCount() const;

private:
    RECT mDesktopMonitorBounds;
    winrt::com_ptr<ID3D11Texture2D> mFrameTexture;
    DXGI_OUTDUPL_FRAME_INFO mFrameInfo;
    winrt::com_ptr<IDXGIOutputDuplication> mDupl;
    std::shared_ptr<std::vector<byte>> mRectBuffer;

    bool mCaptured;

    // move/dirty rects data
    DXGI_OUTDUPL_MOVE_RECT* mMoveRects;
    size_t mNumMoveRects;

    RECT* mDirtyRects;
    size_t mNumDirtyRects;

    DXGI_MODE_ROTATION mRotation;
};
