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
#include "ScreenDuplicator.h"
#include "KeyedMutexLock.h"

class VirtualDesktop
{
public:
    VirtualDesktop();

    ~VirtualDesktop();

    static std::vector<DesktopMonitor> GetAllDesktopMonitors();

    std::vector<DesktopMonitor> DesktopMonitors() const;
    RECT VirtualDesktopBounds() const;

    static RECT CalculateDesktopMonitorBounds(const std::vector<DesktopMonitor>& desktopMonitors);

private:

    std::vector<DesktopMonitor> mDesktopMonitors;
    RECT mVirtualDesktopBounds;
};

