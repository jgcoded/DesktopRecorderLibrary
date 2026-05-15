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
    // Prefer DuplicateOutput1 with a format preference list so we get the
    // native framebuffer format DWM is composing in. Microsoft documents
    // plain DuplicateOutput as always coercing to BGRA8 — and the auto-
    // conversion path is the source of the "oversaturated colors with HDR
    // on" bug (Microsoft Q&A on this is explicit). With DuplicateOutput1
    // and FP16 in the list, HDR mode delivers R16G16B16A16_FLOAT (scRGB
    // linear Rec.709) and SDR mode delivers BGRA8 — both predictable.
    HRESULT hr = E_NOTIMPL;
    winrt::com_ptr<IDXGIOutput5> output5;
    if (SUCCEEDED(mOutput->QueryInterface(__uuidof(IDXGIOutput5), output5.put_void())))
    {
        DXGI_FORMAT preferredFormats[] = {
            DXGI_FORMAT_R16G16B16A16_FLOAT,  // scRGB linear Rec.709 (HDR)
            DXGI_FORMAT_R10G10B10A2_UNORM,   // HDR10 PQ Rec.2020 (rare on the desktop)
            DXGI_FORMAT_B8G8R8A8_UNORM,      // sRGB Rec.709 (SDR)
        };
        hr = output5->DuplicateOutput1(mDevice.get(), 0,
            ARRAYSIZE(preferredFormats), preferredFormats, mDupl.put());
    }
    if (FAILED(hr))
    {
        // Fall back to plain DuplicateOutput on older OS / driver combos
        // where IDXGIOutput5 isn't available. Will hit the buggy auto-
        // converted BGRA8 path under HDR but at least works on SDR.
        hr = mOutput->DuplicateOutput(mDevice.get(), mDupl.put());
    }
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
