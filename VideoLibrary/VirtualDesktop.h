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

#include "DesktopMonitor.h"
#include "KeyedMutexLock.h"

class VirtualDesktop : public std::enable_shared_from_this<VirtualDesktop>
{
public:
    VirtualDesktop();

    ~VirtualDesktop();

    std::vector<DesktopMonitor> GetAllDesktopMonitors();

    std::unique_ptr<DesktopMonitor::ScreenDuplicator> RecordMonitor(const DesktopMonitor& monitor);

    std::vector<std::unique_ptr<DesktopMonitor::ScreenDuplicator>> RecordMonitors(std::vector<DesktopMonitor> monitors);

    RECT VirtualDesktopBounds() const;

    static RECT CalculateDesktopMonitorBounds(const std::vector<DesktopMonitor>& desktopMonitors);

    winrt::com_ptr<ID3D11Device> Device() const;

    std::unique_ptr<KeyedMutexLock> LockWithMutex(winrt::com_ptr<IDXGIKeyedMutex> keyedMutex) const;

    winrt::com_ptr<ID3D11Texture2D> OpenSharedSurfaceWithDevice(winrt::com_ptr<ID3D11Device> device);

private:

    void ResetSharedSurface();

    winrt::com_ptr<ID3D11Texture2D> mSharedSurface;
    std::shared_ptr<RotatingKeys> mRotatingKeys;
    winrt::com_ptr<IDXGIKeyedMutex> mMutex;
    winrt::com_ptr<ID3D11Device> mDevice;

    std::vector<DesktopMonitor> mDesktopMonitors;
    RECT mVirtualDesktopBounds;
};

