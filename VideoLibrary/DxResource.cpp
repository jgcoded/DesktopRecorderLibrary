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
    // ID3D11Debug / ID3D11InfoQueue are only present when the device was
    // created with D3D11_CREATE_DEVICE_DEBUG *and* the Graphics Tools
    // optional Windows feature is installed. If either is missing, fall
    // out silently rather than throwing from a QI failure.
    auto debug = device.try_as<ID3D11Debug>();
    if (!debug)
    {
        return;
    }
    auto info = debug.try_as<ID3D11InfoQueue>();
    if (!info)
    {
        return;
    }
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_INFO, true);
    //info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_MESSAGE, true);
    info->SetMuteDebugOutput(false);
}

namespace
{
    // Create an ID3D11Device, requesting the debug layer in Debug builds.
    // Retries without the debug flag if the D3D11 SDK layers
    // (Graphics Tools optional feature) are not installed on the host.
    winrt::com_ptr<ID3D11Device> CreateDeviceWithOptionalDebug(
        IDXGIAdapter1* adapter,
        D3D_DRIVER_TYPE driverType,
        UINT baseFlags)
    {
        winrt::com_ptr<ID3D11Device> device;

        auto create = [&](UINT flags)
        {
            return D3D11CreateDevice(
                adapter, driverType, nullptr,
                flags,
                nullptr, 0,
                D3D11_SDK_VERSION,
                device.put(),
                nullptr,
                nullptr);
        };

#ifdef _DEBUG
        HRESULT hr = create(baseFlags | D3D11_CREATE_DEVICE_DEBUG);
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            hr = create(baseFlags);
        }
        winrt::check_hresult(hr);
#else
        winrt::check_hresult(create(baseFlags));
#endif

        return device;
    }
}

winrt::com_ptr<ID3D11Device> DxResource::MakeDevice()
{
    auto device = CreateDeviceWithOptionalDebug(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT);

    winrt::com_ptr<ID3D10Multithread> multithread{ device.as<ID3D10Multithread>() };
    multithread->SetMultithreadProtected(true);

#ifdef _DEBUG
    EnableDebugOnDevice(device);
#endif

    return device;
}

winrt::com_ptr<ID3D11Device> DxResource::MakeVideoEnabledDevice(winrt::com_ptr<IDXGIAdapter1> const& adapter)
{
    // This flag is needed by Media Foundation:
    // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfcreatedxgidevicemanager#remarks
    auto device = CreateDeviceWithOptionalDebug(
        adapter.get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT);

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
