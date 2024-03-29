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
#include "DisplayAdapter.h"
#include "DesktopPointer.h"

class DesktopMonitor
{
public:

    DesktopMonitor(
        std::shared_ptr<DisplayAdapter> displayAdapter,
        winrt::com_ptr<IDXGIOutput1> output,
        int outputIndex);

    std::wstring OutputName() const { return mOutputName; }

    RECT DesktopMonitorBounds() const { return mDesktopMonitorBounds; }

    DXGI_MODE_ROTATION Rotation() const { return mRotation; }

    DisplayAdapter const& Adapter() const { return *mDisplayAdapter; }

    winrt::com_ptr<IDXGIOutput1> Output() const { return mOutput; }

    int OutputIndex() const { return mOutputIndex; }

private:

    winrt::com_ptr<IDXGIOutput1> mOutput;
    RECT mDesktopMonitorBounds;
    std::wstring mOutputName;
    int mOutputIndex;
    DXGI_MODE_ROTATION mRotation;

    std::shared_ptr<DisplayAdapter> mDisplayAdapter;
};
