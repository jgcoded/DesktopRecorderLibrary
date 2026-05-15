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
#include "DesktopPointer.h"
#include "MonitorContributor.h"
#include "ScreenDuplicator.h"
#include "ShaderCache.h"
#include "SharedSurface.h"
#include "TexturePool.h"
#include "Vertex.h"

// Top-level multi-monitor recording pipeline.
//
// Takes one ScreenDuplicator per monitor we're recording. Each
// duplicator may live on a different GPU adapter — the master shared
// surface gets opened on each contributor's device via the keyed-mutex
// shared handle so all contributors render into the same texture
// memory.
//
// Per frame: each MonitorContributor runs (acquire lock, capture,
// render into its region of the virtual desktop, release lock). When
// all contributors are done, the pointer composite step and the
// MF-sample wrap step run on the master device.
class Pipeline
{
public:
    Pipeline(
        std::vector<std::shared_ptr<ScreenDuplicator>> duplicators,
        std::shared_ptr<DesktopPointer> desktopPointer,
        std::shared_ptr<SharedSurface> sharedSurface,
        RECT virtualDesktopBounds);

    ~Pipeline();

    void Perform();

    winrt::com_ptr<IMFSample> Sample() const;

private:
    std::vector<std::shared_ptr<ScreenDuplicator>> mDuplicators;
    std::vector<MonitorContributor> mContributors;

    // Master-device state. The master device is whichever device the
    // sharedSurface lives on — it's where the pointer step composites,
    // where the TexturePool allocates, and where the output IMFSample
    // is wrapped. Contributors that live on other devices feed into
    // the same shared surface via OpenSharedSurfaceWithDevice.
    std::shared_ptr<SharedSurface> mSharedSurface;
    std::shared_ptr<DesktopPointer> mDesktopPointer;
    std::shared_ptr<ShaderCache> mShaderCache;
    winrt::com_ptr<TexturePool> mTexturePool;
    winrt::com_ptr<IMFSample> mSample;

    RECT mVirtualDesktopBounds;

    // GPU present-time tracking. QPF is queried once; the baseline is
    // captured from the first frame any contributor delivers with a
    // real present time so the emitted sample timeline starts at 0.
    LARGE_INTEGER mQpcFrequency;
    int64_t mPresentationTimeBaselineQpc;
    bool mPresentationTimeBaselineSet;
};
