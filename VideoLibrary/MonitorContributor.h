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

#include "Frame.h"
#include "ScreenDuplicator.h"
#include "ShaderCache.h"
#include "SharedSurface.h"
#include "Vertex.h"

// One monitor's contribution to the virtual-desktop shared surface.
//
// Holds the per-device state that previously lived inline on Pipeline:
// a ShaderCache (per-device, shaders are device-affine), an opened
// per-device view of the master shared surface (same texture memory
// via the keyed-mutex shared handle when the duplicator lives on a
// different GPU adapter; same instance otherwise), a render-target
// view onto it, a staging texture sized to the duplicator's desktop
// image, and a dynamic vertex buffer for dirty-rect uploads.
//
// `Contribute()` runs one frame: take the keyed-mutex lock on the
// shared surface, capture a frame from the duplicator, render the
// move + dirty rects into the monitor's region of the virtual desktop,
// release the lock. The returned `Frame` carries the QPC presentation
// time and pointer info; the caller (Pipeline) uses those to drive the
// sample timeline and pointer composite step.
class MonitorContributor
{
public:
    MonitorContributor(
        std::shared_ptr<ScreenDuplicator> duplicator,
        std::shared_ptr<SharedSurface> masterSharedSurface,
        RECT virtualDesktopBounds);

    ~MonitorContributor();

    MonitorContributor(MonitorContributor&&) = default;
    MonitorContributor(MonitorContributor const&) = delete;
    MonitorContributor& operator=(MonitorContributor const&) = delete;

    // Capture + render this monitor's contribution for one frame.
    // Returns the Frame (so the caller can read its PresentationTime
    // and observe DesktopPointer side-effects), or null if the keyed-
    // mutex acquire timed out or no frame was delivered.
    std::shared_ptr<Frame> Contribute();

    winrt::com_ptr<ID3D11Device> Device() const;

private:
    std::shared_ptr<ScreenDuplicator> mDuplicator;
    // The shared surface as opened on *this* duplicator's device. For
    // a same-device contributor this is the master shared surface
    // unchanged; for a cross-GPU contributor it's a new SharedSurface
    // instance wrapping the same texture memory (via the keyed-mutex
    // shared handle).
    std::shared_ptr<SharedSurface> mSharedSurface;
    std::shared_ptr<ShaderCache> mShaderCache;
    std::shared_ptr<std::vector<Vertex>> mVertexBuffer;
    winrt::com_ptr<ID3D11Buffer> mGpuVertexBuffer;
    UINT mGpuVertexBufferCapacity;
    winrt::com_ptr<ID3D11Texture2D> mStagingTexture;
    winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
    RECT mVirtualDesktopBounds;
};
