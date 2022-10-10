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

#include "DesktopPointer.h"
#include "DisplayAdapter.h"
#include "DesktopMonitor.h"

class ScreenDuplicator
{
public:

    ScreenDuplicator(
        DesktopMonitor const& monitor,
        std::shared_ptr<DesktopPointer> desktopPointer
    );

    winrt::com_ptr<ID3D11Device> Device() const { return mDevice; }

    std::shared_ptr<DesktopPointer> DesktopPointerPtr();

    winrt::com_ptr<IDXGIOutput1> Output() const { return mOutput; }

    int OutputIndex() const { return mOutputIndex; }

    winrt::com_ptr<IDXGIOutputDuplication> Duplication() const { return mDupl; }

    std::shared_ptr<std::vector<byte>> Buffer() const { return mRectBuffer; };

    ~ScreenDuplicator();

private:

    winrt::com_ptr<ID3D11Device> mDevice;
    winrt::com_ptr<IDXGIOutput1> mOutput;
    winrt::com_ptr<IDXGIOutputDuplication> mDupl;
    std::wstring mOutputName;
    int mOutputIndex;

    // Holds move and dirty rects
    std::shared_ptr<std::vector<byte>> mRectBuffer;

    std::shared_ptr<DesktopPointer> mDesktopPointer;
};