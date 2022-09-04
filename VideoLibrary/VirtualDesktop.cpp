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
#include "DesktopMonitor.h"
#include "DxResource.h"
#include "VirtualDesktop.h"

VirtualDesktop::VirtualDesktop()
    : mDevice { DxResource::MakeDevice() }
    , mRotatingKeys { std::make_shared<RotatingKeys>() }
    , mVirtualDesktopBounds { INT_MAX, INT_MAX, INT_MIN, INT_MIN }
{
}

VirtualDesktop::~VirtualDesktop()
{
}

std::vector<DesktopMonitor> VirtualDesktop::GetAllDesktopMonitors()
{
    std::vector<DesktopMonitor> desktopMonitors;
    auto factory = DxResource::MakeDxgiFactory();

    winrt::com_ptr<IDXGIAdapter1> adapter = nullptr;
    for (int adapterIndex = 0;
        DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, adapter.put());
        adapter = nullptr, adapterIndex++) {

        DXGI_ADAPTER_DESC1 adapterDesc;
        winrt::check_hresult(adapter->GetDesc1(&adapterDesc));

        // software adapter; skip
        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        auto displayAdapter = std::make_shared<DesktopMonitor::DisplayAdapter>(adapter);
        winrt::com_ptr<IDXGIOutput> output = nullptr;
        for (int outputIndex = 0;
            DXGI_ERROR_NOT_FOUND != adapter->EnumOutputs(outputIndex, output.put());
            output = nullptr, outputIndex++) {

            winrt::com_ptr<IDXGIOutput1> output1{ output.as<IDXGIOutput1>() };
            DXGI_OUTPUT_DESC outputDesc;
            winrt::check_hresult(output1->GetDesc(&outputDesc));

            if (outputDesc.AttachedToDesktop) {
                DesktopMonitor desktopMonitor{ this->shared_from_this(), displayAdapter, output1, outputIndex };
                desktopMonitors.push_back(desktopMonitor);
            }
        }
    }

    return desktopMonitors;
}

std::unique_ptr<DesktopMonitor::ScreenDuplicator> VirtualDesktop::RecordMonitor(const DesktopMonitor& monitor)
{
    auto monitors = std::vector<DesktopMonitor>{};
    monitors.push_back(monitor);
    auto duplicators = this->RecordMonitors(monitors);
    return std::move(duplicators.front());
}

std::vector<std::unique_ptr<DesktopMonitor::ScreenDuplicator>> VirtualDesktop::RecordMonitors(std::vector<class DesktopMonitor> monitors)
{
    mDesktopMonitors = monitors;
    ResetSharedSurface();

    std::vector<std::unique_ptr<DesktopMonitor::ScreenDuplicator>> result;
    for (auto& monitor : monitors) {
        result.push_back(std::move(std::make_unique<DesktopMonitor::ScreenDuplicator>(monitor)));
    }

    return result;
}

winrt::com_ptr<ID3D11Device> VirtualDesktop::Device() const
{
    return mDevice;
}

std::unique_ptr<KeyedMutexLock> VirtualDesktop::LockWithMutex(winrt::com_ptr<IDXGIKeyedMutex> keyedMutex) const
{
    return std::make_unique<KeyedMutexLock>(keyedMutex, mRotatingKeys);
}

winrt::com_ptr<ID3D11Texture2D> VirtualDesktop::OpenSharedSurfaceWithDevice(winrt::com_ptr<ID3D11Device> device)
{
    HANDLE handle = nullptr;
    winrt::com_ptr<IDXGIResource> resource = mSharedSurface.as<IDXGIResource>();
    winrt::check_hresult(resource->GetSharedHandle(&handle));

    winrt::com_ptr<ID3D11Texture2D> sharedSurface;
    winrt::check_hresult(device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), sharedSurface.put_void()));
    return sharedSurface;
}

RECT VirtualDesktop::CalculateDesktopMonitorBounds(const std::vector<DesktopMonitor>& desktopMonitors)
{
    // determine desktop bounds
    auto bounds = RECT{ INT_MAX, INT_MAX, INT_MIN, INT_MIN };
    for (const auto& monitor : desktopMonitors)
    {
        RECT monitorBounds = monitor.DesktopMonitorBounds();
        bounds.left = std::min(bounds.left, monitorBounds.left);
        bounds.top = std::min(bounds.top, monitorBounds.top);
        bounds.right = std::max(bounds.right, monitorBounds.right);
        bounds.bottom = std::max(bounds.bottom, monitorBounds.bottom);
    }

    return bounds;
}

RECT VirtualDesktop::VirtualDesktopBounds() const
{
    assert(mDesktopMonitors.size() > 0);
    return mVirtualDesktopBounds;
}

void VirtualDesktop::ResetSharedSurface()
{
    // determine desktop bounds
    auto bounds = CalculateDesktopMonitorBounds(mDesktopMonitors);

    auto width = bounds.right - bounds.left;
    auto height = bounds.bottom - bounds.top;

    // TODO see if shared surface can be reused
    {
        auto currentWidth = mVirtualDesktopBounds.right - mVirtualDesktopBounds.left;
        auto currentHeight = mVirtualDesktopBounds.bottom - mVirtualDesktopBounds.top;

        if (currentWidth == width && currentHeight == height)
        {
            return;
        }
    }

    mSharedSurface = nullptr;
    mMutex = nullptr;
    mRotatingKeys.reset(new RotatingKeys);
    mVirtualDesktopBounds = bounds;

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
}
