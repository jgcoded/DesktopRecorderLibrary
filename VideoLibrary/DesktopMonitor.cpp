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
#include "DxResource.h"
#include "VirtualDesktop.h"
#include "Errors.h"
#include "DesktopMonitor.h"

DesktopMonitor::DesktopMonitor(
    std::shared_ptr<VirtualDesktop> virtualDesktop,
    std::shared_ptr<DisplayAdapter> displayAdapter,
    winrt::com_ptr<IDXGIOutput1> output,
    int outputIndex)
    : mVirtualDesktop { virtualDesktop }
    , mDisplayAdapter{ displayAdapter }
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

DesktopMonitor::DisplayAdapter::DisplayAdapter(winrt::com_ptr<IDXGIAdapter1> const& adapter)
    : mAdapter{ adapter }
{
    DXGI_ADAPTER_DESC1 adapterDesc;
    winrt::check_hresult(mAdapter->GetDesc1(&adapterDesc));

    mAdapterName = adapterDesc.Description;
    mDevice = DxResource::MakeVideoEnabledDevice(mAdapter);
}

DesktopMonitor::ScreenDuplicator::ScreenDuplicator(DesktopMonitor const& monitor)
    : mDisplayAdapter{ monitor.mDisplayAdapter }
    , mDevice{ monitor.mDisplayAdapter->Device() }
    , mOutput{ monitor.mOutput }
    , mOutputIndex { monitor.mOutputIndex }
    , mOutputName { monitor.mOutputName }
    , mVirtualDesktop { monitor.mVirtualDesktop }
    , mDesktopPointer { std::make_shared<class DesktopPointer>() }
    , mRectBuffer{ std::make_shared<std::vector<byte>>() }
{
    DuplicateOutput();
}

std::weak_ptr<VirtualDesktop> DesktopMonitor::ScreenDuplicator::VirtualDesktop() const
{
    return mVirtualDesktop;
}

std::shared_ptr<class DesktopPointer> DesktopMonitor::ScreenDuplicator::DesktopPointer()
{
    return mDesktopPointer;
}

void DesktopMonitor::ScreenDuplicator::ResetDuplication()
{
    auto virtualDesktop = mVirtualDesktop.lock();
    if (!virtualDesktop)
    {
        return;
    }

    auto displayAdapter = mDisplayAdapter.lock();
    if (!displayAdapter)
    {
        return;
    }

    std::vector<DesktopMonitor> monitors = virtualDesktop->GetAllDesktopMonitors();
    if (mDupl)
    {
        mDupl = nullptr;
    }

    for (const auto& monitor : monitors)
    {
        if (monitor.mOutputName == mOutputName
            && monitor.mOutputIndex == mOutputIndex
            && monitor.mDisplayAdapter->Name() == displayAdapter->Name()
            && (monitor.Rotation() == DXGI_MODE_ROTATION_IDENTITY ||
                monitor.Rotation() == DXGI_MODE_ROTATION_UNSPECIFIED))
        {
            // only change the recorded monitor if DuplicateOutput did not throw
            std::unique_ptr<ScreenDuplicator> duplicator = virtualDesktop->RecordMonitor(monitor);
            this->mDisplayAdapter = duplicator->mDisplayAdapter;
            this->mDevice = displayAdapter->Device();
            this->mDupl = duplicator->mDupl;
            this->mOutput = duplicator->mOutput;
            this->mOutputIndex = duplicator->mOutputIndex;
            this->mOutputName = duplicator->mOutputName;
            this->mVirtualDesktop = duplicator->mVirtualDesktop;
            this->mDesktopPointer = duplicator->mDesktopPointer;
            this->mRectBuffer = duplicator->mRectBuffer;
            return;
        }
    }

    winrt::check_hresult(0xDEADD01D);
}

DesktopMonitor::ScreenDuplicator::~ScreenDuplicator()
{
    if (mDupl)
    {
        // ignore hr, just release in case this object went out of scope
        (void)mDupl->ReleaseFrame();
        mDupl = nullptr;
    }
}

void DesktopMonitor::ScreenDuplicator::DuplicateOutput()
{
    HRESULT hr = mOutput->DuplicateOutput(mDevice.get(), mDupl.put());
    
    if (FAILED(hr))
    {
        ThrowExceptionCheckRecoverable(mDevice, CreateDuplicationExpectedErrors, hr);
    }
}

DesktopMonitor::ScreenDuplicator::Frame::Frame(ScreenDuplicator& duplicator)
    : mDupl { duplicator.mDupl }
    , mVirtualDesktop { duplicator.mVirtualDesktop }
    , mCaptured{ false }
    , mRectBuffer{ duplicator.mRectBuffer }
    , mNumMoveRects{ 0 }
    , mNumDirtyRects{ 0 }
    , mMoveRects{ nullptr }
    , mDirtyRects{ nullptr }
{
    try
    {
        winrt::com_ptr<IDXGIResource> desktopImageResource;
        HRESULT hr = duplicator.mDupl->AcquireNextFrame(1, &mFrameInfo, desktopImageResource.put());

        if (hr == DXGI_ERROR_WAIT_TIMEOUT || hr == DXGI_STATUS_OCCLUDED)
        {
            return;
        }

        winrt::check_hresult(hr);

        mCaptured = true;
        mFrameTexture = desktopImageResource.as<ID3D11Texture2D>();

        // Don't care about move or dirty rects, just get the pointer data and update the pointer cache
        if (mFrameInfo.LastMouseUpdateTime.QuadPart != 0 && mFrameInfo.PointerShapeBufferSize != 0) {

            UINT requiredBufferSize;
            DXGI_OUTDUPL_POINTER_SHAPE_INFO pointerInfo;
            winrt::check_hresult(duplicator.mDupl->GetFramePointerShape(mFrameInfo.PointerShapeBufferSize,
                reinterpret_cast<void*>(duplicator.DesktopPointer()->PutBuffer(mFrameInfo.PointerShapeBufferSize)),
                &requiredBufferSize,
                &pointerInfo));

            duplicator.DesktopPointer()->ShapeInfo(pointerInfo);
        }

        DXGI_OUTPUT_DESC desc;
        winrt::check_hresult(duplicator.mOutput->GetDesc(&desc));
        mDesktopMonitorBounds = desc.DesktopCoordinates;
        mRotation = desc.Rotation;

        if (auto virtualDesktop = mVirtualDesktop.lock())
        {
            auto virtualDesktopBounds = virtualDesktop->VirtualDesktopBounds();
            mFrameInfo.PointerPosition.Position.x += mDesktopMonitorBounds.left - virtualDesktopBounds.left;
            mFrameInfo.PointerPosition.Position.y += mDesktopMonitorBounds.top - virtualDesktopBounds.top;
        }
        duplicator.DesktopPointer()->UpdatePosition(
            mFrameInfo.PointerPosition,
            mFrameInfo.LastMouseUpdateTime,
            duplicator.mOutputIndex);

        // get frame metadata
        if (mFrameInfo.TotalMetadataBufferSize != 0) {

            UINT totalBufferSize = mFrameInfo.TotalMetadataBufferSize;
            if (totalBufferSize > mRectBuffer->size()) {
                mRectBuffer->resize(totalBufferSize);
            }

            UINT moveRectsBufferSize = 0;
            winrt::check_hresult(mDupl->GetFrameMoveRects(
                totalBufferSize,
                reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(mRectBuffer->data()),
                &moveRectsBufferSize));

            UINT dirtyRectsBufferSize = totalBufferSize - moveRectsBufferSize;

            winrt::check_hresult(mDupl->GetFrameDirtyRects(
                dirtyRectsBufferSize,
                reinterpret_cast<RECT*>(mRectBuffer->data() + moveRectsBufferSize),
                &dirtyRectsBufferSize));

            mNumMoveRects = moveRectsBufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);
            mNumDirtyRects = dirtyRectsBufferSize / sizeof(RECT);
        }
    }
    catch (...)
    {
        HRESULT hr = winrt::to_hresult();
        ThrowExceptionCheckRecoverable(duplicator.Device(), FrameInfoExpectedErrors, hr);
    }
}

DesktopMonitor::ScreenDuplicator::Frame::~Frame()
{
    if (mDupl)
    {
        try
        {
            (void)mDupl->ReleaseFrame();
        }
        catch (...)
        {
        }
    }
}

winrt::com_ptr<ID3D11Texture2D> DesktopMonitor::ScreenDuplicator::Frame::DesktopImage() const
{
    return mFrameTexture;
}

std::weak_ptr<VirtualDesktop> DesktopMonitor::ScreenDuplicator::Frame::VirtualDesktop() const
{
    return mVirtualDesktop;
}

RECT DesktopMonitor::ScreenDuplicator::Frame::DesktopMonitorBounds() const
{
    return mDesktopMonitorBounds;
}

int64_t DesktopMonitor::ScreenDuplicator::Frame::PresentationTime() const
{
//    LARGE_INTEGER frequency;
  //  QueryPerformanceFrequency(&frequency);

    int64_t nanoSeconds = mFrameInfo.LastPresentTime.QuadPart;

    // ticks / ticks per second = seconds
    // save precision by dividing first and then multipling by 1e9 (1e9 ns in one sec)
    
  //  nanoSeconds /= frequency.QuadPart;
  //  nanoSeconds *= 1'000'000'000;
    

    return nanoSeconds;
}

bool DesktopMonitor::ScreenDuplicator::Frame::Captured() const
{
    return mCaptured;
}

DXGI_MODE_ROTATION DesktopMonitor::ScreenDuplicator::Frame::Rotation() const { return mRotation; }

DXGI_OUTDUPL_MOVE_RECT * DesktopMonitor::ScreenDuplicator::Frame::MoveRects() const
{
    return reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(mRectBuffer->data());
}

RECT * DesktopMonitor::ScreenDuplicator::Frame::DirtyRects() const
{
    return reinterpret_cast<RECT*>(mRectBuffer->data() + (mNumMoveRects * sizeof(DXGI_OUTDUPL_MOVE_RECT)));
}

size_t DesktopMonitor::ScreenDuplicator::Frame::MoveRectsCount() const { return mNumMoveRects; }

size_t DesktopMonitor::ScreenDuplicator::Frame::DirtyRectsCount() const { return mNumDirtyRects; }
