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

#include "KeyedMutexLock.h"

class SharedSurface
{
public:
    SharedSurface(winrt::com_ptr<ID3D11Device> device, int width, int height);
    SharedSurface(winrt::com_ptr<ID3D11Device> device, winrt::com_ptr<ID3D11Texture2D> texture, std::shared_ptr<RotatingKeys> rotatingKeys, int width, int height);

    winrt::com_ptr<ID3D11Device> Device() const { return mDevice; }
    std::unique_ptr<KeyedMutexLock> Lock() const;
    std::shared_ptr<SharedSurface> OpenSharedSurfaceWithDevice(winrt::com_ptr<ID3D11Device> device) const;
    D3D11_TEXTURE2D_DESC Desc() const { return mDesc; }
private:

    winrt::com_ptr<ID3D11Texture2D> mSharedSurface;
    std::shared_ptr<RotatingKeys> mRotatingKeys;
    winrt::com_ptr<IDXGIKeyedMutex> mMutex;
    winrt::com_ptr<ID3D11Device> mDevice;
    D3D11_TEXTURE2D_DESC mDesc;
    int mWidth;
    int mHeight;
};

