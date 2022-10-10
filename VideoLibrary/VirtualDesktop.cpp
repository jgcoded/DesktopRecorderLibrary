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
#include "DisplayAdapter.h"
#include "VirtualDesktop.h"

VirtualDesktop::VirtualDesktop()
    : mDesktopMonitors { VirtualDesktop::GetAllDesktopMonitors() }
    , mVirtualDesktopBounds { VirtualDesktop::CalculateDesktopMonitorBounds(mDesktopMonitors) }
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

        auto displayAdapter = std::make_shared<DisplayAdapter>(adapter);
        winrt::com_ptr<IDXGIOutput> output = nullptr;
        for (int outputIndex = 0;
            DXGI_ERROR_NOT_FOUND != adapter->EnumOutputs(outputIndex, output.put());
            output = nullptr, outputIndex++) {

            winrt::com_ptr<IDXGIOutput1> output1{ output.as<IDXGIOutput1>() };
            DXGI_OUTPUT_DESC outputDesc;
            winrt::check_hresult(output1->GetDesc(&outputDesc));

            if (outputDesc.AttachedToDesktop) {
                DesktopMonitor desktopMonitor{ displayAdapter, output1, outputIndex };
                desktopMonitors.push_back(desktopMonitor);
            }
        }
    }

    return desktopMonitors;
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
    return mVirtualDesktopBounds;
}

std::vector<DesktopMonitor> VirtualDesktop::DesktopMonitors() const
{
    return mDesktopMonitors;
}