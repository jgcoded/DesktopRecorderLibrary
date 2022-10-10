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
#include "SharedSurface.h"

SharedSurface::SharedSurface(winrt::com_ptr<ID3D11Device> device, int width, int height)
    : mDevice{ device }
    , mSharedSurface{ nullptr }
    , mMutex { nullptr }
    , mRotatingKeys { new RotatingKeys }
    , mWidth{ width }
    , mHeight{ height }
{
    D3D11_TEXTURE2D_DESC desc;
    RtlZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.CPUAccessFlags = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    winrt::check_hresult(mDevice->CreateTexture2D(&desc, nullptr, mSharedSurface.put()));
    mMutex = mSharedSurface.as<IDXGIKeyedMutex>();
    winrt::check_pointer(mMutex.get());
    mSharedSurface->GetDesc(&mDesc);
}

SharedSurface::SharedSurface(
    winrt::com_ptr<ID3D11Device> device,
    winrt::com_ptr<ID3D11Texture2D> texture,
    std::shared_ptr<RotatingKeys> rotatingKeys,
    int width,
    int height)
    : mDevice{ device }
    , mSharedSurface{ texture }
    , mMutex{ mSharedSurface.as<IDXGIKeyedMutex>() }
    , mRotatingKeys { rotatingKeys }
    , mWidth{ width }
    , mHeight{ height }
{
    winrt::check_pointer(mDevice.get());
    winrt::check_pointer(mMutex.get());
    winrt::check_pointer(mSharedSurface.get());
    mSharedSurface->GetDesc(&mDesc);
}

std::unique_ptr<KeyedMutexLock> SharedSurface::Lock() const
{
    return std::make_unique<KeyedMutexLock>(mSharedSurface, mMutex, mRotatingKeys);
}

std::shared_ptr<SharedSurface> SharedSurface::OpenSharedSurfaceWithDevice(winrt::com_ptr<ID3D11Device> device) const
{
    HANDLE handle = nullptr;
    winrt::com_ptr<IDXGIResource> resource = mSharedSurface.as<IDXGIResource>();
    winrt::check_hresult(resource->GetSharedHandle(&handle));

    winrt::com_ptr<ID3D11Texture2D> sharedSurface;
    winrt::check_hresult(device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), sharedSurface.put_void()));
    return std::make_shared<SharedSurface>(device, sharedSurface, mRotatingKeys, mWidth, mHeight);
}

