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
#include "DxMultithread.h"
#include "Pipeline.h"
#include "RenderPointerTextureStep.h"
#include "TextureToMediaSampleStep.h"

Pipeline::Pipeline(
    std::vector<std::shared_ptr<ScreenDuplicator>> duplicators,
    std::shared_ptr<DesktopPointer> desktopPointer,
    std::shared_ptr<SharedSurface> sharedSurface,
    RECT virtualDesktopBounds)
    : mDuplicators{ std::move(duplicators) }
    , mSharedSurface{ sharedSurface }
    , mDesktopPointer{ desktopPointer }
    , mVirtualDesktopBounds{ virtualDesktopBounds }
    , mPresentationTimeBaselineQpc{ 0 }
    , mPresentationTimeBaselineSet{ false }
{
    QueryPerformanceFrequency(&mQpcFrequency);

    if (mDuplicators.empty())
    {
        throw std::exception("Pipeline requires at least one duplicator");
    }
    for (auto const& dup : mDuplicators)
    {
        winrt::check_pointer(dup.get());
    }
    winrt::check_pointer(mSharedSurface.get());
    winrt::check_pointer(mDesktopPointer.get());

    // Master device drives the pointer step + sample step. The shared
    // surface anchors the master device by definition.
    auto masterDevice = mSharedSurface->Device();
    mShaderCache = std::make_shared<ShaderCache>(masterDevice);

    // TexturePool is per-device; pointer composite reads from the
    // shared surface and writes into a pool texture on the master.
    {
        D3D11_TEXTURE2D_DESC desc = mSharedSurface->Desc();
        mTexturePool.attach(new TexturePool(masterDevice, desc));
        winrt::check_pointer(mTexturePool.get());
    }

    // Build a contributor per duplicator. Each one opens the shared
    // surface on its device (or reuses the master instance for same-
    // device monitors) and stands up its own ShaderCache + RTV.
    mContributors.reserve(mDuplicators.size());
    for (auto const& dup : mDuplicators)
    {
        mContributors.emplace_back(dup, mSharedSurface, mVirtualDesktopBounds);
    }
}

Pipeline::~Pipeline() = default;

void Pipeline::Perform()
{
    mSample = nullptr;

    // Run each contributor in turn. Each acquires the keyed-mutex lock
    // on the shared surface, captures + renders its monitor, releases.
    // Serializing the contributors keeps the rotating-key sequence on
    // the keyed mutex consistent across N devices without needing extra
    // synchronization; if a contributor's frame fails to acquire (no
    // new content), its KeyedMutexLock short-circuits the rotate so
    // subsequent contributors stay in step.
    //
    // Pick the first frame that reports a real GPU present time as the
    // master clock. DDA's LastPresentTime is 0 when the desktop wasn't
    // updated since the last AcquireNextFrame, so we can't use a value
    // of 0 as a "not set" sentinel — track presence explicitly.
    int64_t framePresentationTimeQpc = 0;
    bool framePresentationTimeSet = false;
    for (auto& contributor : mContributors)
    {
        auto frame = contributor.Contribute();
        if (frame && frame->Captured() && !framePresentationTimeSet)
        {
            int64_t presentTime = frame->PresentationTime();
            if (presentTime > 0)
            {
                framePresentationTimeQpc = presentTime;
                framePresentationTimeSet = true;
            }
        }
    }

    // If no contributor reported a real GPU present time, fall back to
    // the current QPC so every emitted sample still carries a tagged
    // timestamp on a single clock. Without this, early frames would
    // reach the sink writer un-stamped and pick up wall-clock times,
    // and later frames would switch to the GPU clock once a non-zero
    // present time arrived — a clock cross that can break the
    // monotonicity the encoder relies on for A/V sync.
    if (!framePresentationTimeSet)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        framePresentationTimeQpc = now.QuadPart;
        framePresentationTimeSet = true;
    }

    // Master-device steps: pointer composite + sample wrap. Both run
    // on the master device's context; the pointer step takes its own
    // lock on the shared surface internally to copy the composited
    // virtual desktop into a pool texture before drawing the cursor.
    auto masterDevice = mSharedSurface->Device();
    DxMultithread multithread{ masterDevice.as<ID3D10Multithread>() };

    RenderPointerTextureStep renderPointer{
        mDesktopPointer,
        mSharedSurface,
        masterDevice,
        mShaderCache,
        mTexturePool,
        mVirtualDesktopBounds
    };
    renderPointer.Perform();

    if (renderPointer.Result() == nullptr)
    {
        return;
    }

    TextureToMediaSampleStep convertTexture{
        renderPointer.Result(),
        mTexturePool
    };
    convertTexture.Perform();

    mSample = convertTexture.Result();

    // Tag the sample with the GPU present time of the captured frame
    // so the sink writer's encoded timeline is frame-accurate (and so
    // dropped frames don't compress the timeline).
    if (mSample && framePresentationTimeSet && mQpcFrequency.QuadPart > 0)
    {
        if (!mPresentationTimeBaselineSet)
        {
            mPresentationTimeBaselineQpc = framePresentationTimeQpc;
            mPresentationTimeBaselineSet = true;
        }
        int64_t elapsedQpc = framePresentationTimeQpc - mPresentationTimeBaselineQpc;
        int64_t qpf = mQpcFrequency.QuadPart;
        // Split-multiply to avoid overflowing int64 on long recordings.
        int64_t sampleTime100ns = (elapsedQpc / qpf) * 10'000'000
            + (elapsedQpc % qpf) * 10'000'000 / qpf;
        winrt::check_hresult(mSample->SetSampleTime(sampleTime100ns));
    }
}

winrt::com_ptr<IMFSample> Pipeline::Sample() const
{
    return mSample;
}
