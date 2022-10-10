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

DesktopMonitor::DesktopMonitor(
    std::shared_ptr<DisplayAdapter> displayAdapter,
    winrt::com_ptr<IDXGIOutput1> output,
    int outputIndex)
    : mDisplayAdapter{ displayAdapter }
    , mOutput{ output }
    , mOutputIndex{ outputIndex }
{
    DXGI_OUTPUT_DESC outputDesc;
    winrt::check_hresult(mOutput->GetDesc(&outputDesc));

    DISPLAY_DEVICE displayDevice;
    displayDevice.cb = sizeof(DISPLAY_DEVICE);
    winrt::check_bool(EnumDisplayDevices(outputDesc.DeviceName, 0, &displayDevice, 0));

    mOutputName = displayDevice.DeviceString;
    mDesktopMonitorBounds = outputDesc.DesktopCoordinates;
    mRotation = outputDesc.Rotation;
}
