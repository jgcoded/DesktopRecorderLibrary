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
#include "Errors.h"
#include "ScreenDuplicator.h"

ScreenDuplicator::ScreenDuplicator(
    DesktopMonitor const& monitor,
    std::shared_ptr<DesktopPointer> desktopPointer)
    : mDevice {monitor.Adapter().Device() }
    , mOutput{ monitor.Output()}
    , mDesktopPointer{ desktopPointer }
    , mRectBuffer{ std::make_shared<std::vector<byte>>() }
    , mOutputIndex{ monitor.OutputIndex() }
{
    HRESULT hr = mOutput->DuplicateOutput(mDevice.get(), mDupl.put());
    if (FAILED(hr))
    {
        ThrowExceptionCheckRecoverable(mDevice, CreateDuplicationExpectedErrors, hr);
    }
}

std::shared_ptr<DesktopPointer> ScreenDuplicator::DesktopPointerPtr()
{
    return mDesktopPointer;
}

ScreenDuplicator::~ScreenDuplicator()
{
    if (mDupl)
    {
        // ignore hr, just release in case this object went out of scope
        (void)mDupl->ReleaseFrame();
        mDupl = nullptr;
    }
}
