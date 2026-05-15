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
#include <sstream>
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
    , mColorSpace{ DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 }
{
    HRESULT hr = mOutput->DuplicateOutput(mDevice.get(), mDupl.put());
    if (FAILED(hr))
    {
        ThrowExceptionCheckRecoverable(mDevice, CreateDuplicationExpectedErrors, hr);
    }

    // Read the output's color space + per-channel bit depth. With HDR
    // enabled in Windows, DDA hands us framebuffer pixels in scRGB
    // (linear Rec.709, FP16) or HDR10 (PQ-encoded Rec.2020, 10-bit
    // UNORM). Treating those values as sRGB downstream produces the
    // oversaturated-reds look users see in Movies & TV.
    {
        winrt::com_ptr<IDXGIOutput6> output6;
        if (SUCCEEDED(mOutput->QueryInterface(__uuidof(IDXGIOutput6), output6.put_void())))
        {
            DXGI_OUTPUT_DESC1 desc1{};
            if (SUCCEEDED(output6->GetDesc1(&desc1)))
            {
                mColorSpace = desc1.ColorSpace;
                std::wstringstream ss;
                ss << L"ScreenDuplicator: ColorSpace=" << static_cast<int>(mColorSpace)
                   << L" BitsPerColor=" << desc1.BitsPerColor
                   << L" MinLuma=" << desc1.MinLuminance
                   << L" MaxLuma=" << desc1.MaxLuminance
                   << L" MaxFullFrameLuma=" << desc1.MaxFullFrameLuminance << L"\n";
                OutputDebugStringW(ss.str().c_str());
            }
        }
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
