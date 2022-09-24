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

class DesktopMonitor
{
    friend class VirtualDesktop;

private:

    class DisplayAdapter
    {
    public:

        DisplayAdapter(winrt::com_ptr<IDXGIAdapter1> const& adapter);

        winrt::com_ptr<ID3D11Device> Device() const { return mDevice; }

        std::wstring const& Name() const { return mAdapterName; }

    private:

        winrt::com_ptr<ID3D11Device> mDevice;
        winrt::com_ptr<IDXGIAdapter1> mAdapter;

        std::wstring mAdapterName;
    };

public:

    class ScreenDuplicator
    {
    public:

        class Frame
        {
        public:
            Frame(ScreenDuplicator& duplicator);
            ~Frame();

            winrt::com_ptr<ID3D11Texture2D> DesktopImage() const;
            
            std::weak_ptr<class VirtualDesktop> VirtualDesktop() const;

            RECT DesktopMonitorBounds() const;

            int64_t PresentationTime() const;

            bool Captured() const;

            DXGI_MODE_ROTATION Rotation() const;

            DXGI_OUTDUPL_MOVE_RECT* MoveRects() const;

            RECT* DirtyRects() const;

            size_t MoveRectsCount() const;

            size_t DirtyRectsCount() const;

        private:
            RECT mDesktopMonitorBounds;
            winrt::com_ptr<ID3D11Texture2D> mFrameTexture;
            DXGI_OUTDUPL_FRAME_INFO mFrameInfo;
            winrt::com_ptr<IDXGIOutputDuplication> mDupl;
            std::weak_ptr<class VirtualDesktop> mVirtualDesktop;
            std::shared_ptr<std::vector<byte>> mRectBuffer;

            bool mCaptured;

            // move/dirty rects data
            DXGI_OUTDUPL_MOVE_RECT* mMoveRects;
            size_t mNumMoveRects;

            RECT* mDirtyRects;
            size_t mNumDirtyRects;

            DXGI_MODE_ROTATION mRotation;
        };

        ScreenDuplicator(DesktopMonitor const& monitor);

        winrt::com_ptr<ID3D11Device> Device() const { return mDevice; }

        std::weak_ptr<class VirtualDesktop> VirtualDesktop() const;

        std::shared_ptr<class DesktopPointer> DesktopPointer();

        void ResetDuplication();

        ~ScreenDuplicator();

    private:

        void DuplicateOutput();

        winrt::com_ptr<ID3D11Device> mDevice;
        std::weak_ptr<DisplayAdapter> mDisplayAdapter;
        winrt::com_ptr<IDXGIOutput1> mOutput;
        winrt::com_ptr<IDXGIOutputDuplication> mDupl;
        int mOutputIndex;
        std::wstring mOutputName;
        std::weak_ptr<class VirtualDesktop> mVirtualDesktop;

        // Holds move and dirty rects
        std::shared_ptr<std::vector<byte>> mRectBuffer;

        std::shared_ptr<class DesktopPointer> mDesktopPointer;
    };

    std::wstring OutputName() const { return mOutputName; }

    RECT DesktopMonitorBounds() const { return mDesktopMonitorBounds; }

    DXGI_MODE_ROTATION Rotation() const { return mRotation; }

    DisplayAdapter const& Adapter() const { return *mDisplayAdapter; }

private:

    DesktopMonitor(
        std::shared_ptr<class VirtualDesktop> virtualDesktop,
        std::shared_ptr<DisplayAdapter> displayAdapter,
        winrt::com_ptr<IDXGIOutput1> output,
        int outputIndex);

    winrt::com_ptr<IDXGIOutput1> mOutput;
    RECT mDesktopMonitorBounds;
    std::wstring mOutputName;
    int mOutputIndex;
    DXGI_MODE_ROTATION mRotation;

    std::shared_ptr<DisplayAdapter> mDisplayAdapter;
    std::weak_ptr<class VirtualDesktop> mVirtualDesktop;
};
