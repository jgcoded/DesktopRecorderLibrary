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

class DesktopPointer
{
public:
    DesktopPointer();
    virtual ~DesktopPointer();

    std::size_t BufferSize() const;

    byte* PutBuffer(std::size_t requiredSize = 0);

    /*
        TODO in a multi monitor recording scenario, need to offset the Position based on entire virtual desktop
        The returned position will work when recording a single monitor because it is
        relative to the monitor that currently owns the pointer
    */
    DXGI_OUTDUPL_POINTER_POSITION Position() const;
    void UpdatePosition(DXGI_OUTDUPL_POINTER_POSITION newPosition, LARGE_INTEGER updateTime, UINT outputIndex);

    DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo() const;
    void ShapeInfo(DXGI_OUTDUPL_POINTER_SHAPE_INFO newShapeInfo);

    void UpdateTexture(winrt::com_ptr<ID3D11Texture2D> const& newImage);
    winrt::com_ptr<ID3D11Texture2D> Texture() const;

    bool Visible() const;

private:
    std::vector<byte> mBuffer;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO mShapeInfo;
    bool mIsPointerTextureStale;
    winrt::com_ptr<ID3D11Texture2D> mPointerTexture;
    DXGI_OUTDUPL_POINTER_POSITION mPosition;
    LARGE_INTEGER mLastUpdateTime;
    UINT mPointerOwnerIndex;
    bool mVisible;
};

