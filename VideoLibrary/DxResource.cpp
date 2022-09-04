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
#include "DxResource.h"

void EnableDebugOnDevice(winrt::com_ptr<ID3D11Device> device)
{
    winrt::com_ptr<ID3D11Debug> debug{ device.as<ID3D11Debug>() };
    winrt::com_ptr<ID3D11InfoQueue> info{ debug.as<ID3D11InfoQueue>() };
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_INFO, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_MESSAGE, true);
    info->SetMuteDebugOutput(false);
}

winrt::com_ptr<ID3D11Device> DxResource::MakeDevice()
{
    winrt::com_ptr<ID3D11Device> device;

    int flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    winrt::check_hresult(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        device.put(),
        nullptr,
        nullptr
    ));

    winrt::com_ptr<ID3D10Multithread> multithread{ device.as<ID3D10Multithread>() };
    multithread->SetMultithreadProtected(true);

#ifdef _DEBUG
    EnableDebugOnDevice(device);
#endif

    return device;
}

winrt::com_ptr<ID3D11Device> DxResource::MakeVideoEnabledDevice(winrt::com_ptr<IDXGIAdapter1> const& adapter)
{
    winrt::com_ptr<ID3D11Device> device;

    // This flag is needed by Media Foundation:
    // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfcreatedxgidevicemanager#remarks
    int flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    winrt::check_hresult(D3D11CreateDevice(
        adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        device.put(),
        nullptr,
        nullptr
    ));

    winrt::com_ptr<ID3D10Multithread> multithread{ device.as<ID3D10Multithread>() };
    multithread->SetMultithreadProtected(true);

#ifdef _DEBUG
    EnableDebugOnDevice(device);
#endif

    return device;
}

winrt::com_ptr<IDXGIFactory1> DxResource::MakeDxgiFactory()
{
    winrt::com_ptr<IDXGIFactory1> factory;
    winrt::check_hresult(CreateDXGIFactory1(__uuidof(factory), factory.put_void()));
    return factory;
}

DxResource::DxResource()
{
}
