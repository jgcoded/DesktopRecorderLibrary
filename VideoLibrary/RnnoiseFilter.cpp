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
#include "RnnoiseFilter.h"

#include <cassert>

extern "C" {
#include "third_party/rnnoise/include/rnnoise.h"
}

namespace
{
    // RNNoise expects int16-scale floats (-32768..32767), not the
    // -1..1 range WASAPI delivers. The CELT_SIG_SCALE constant is
    // 32768 in upstream; mirroring it here so the audio path is
    // explicit.
    constexpr float kInt16Scale = 32768.0f;
}

RnnoiseFilter::RnnoiseFilter()
    : mState{ rnnoise_create(nullptr) }
{
    if (!mState)
    {
        throw std::bad_alloc{};
    }
    mInputAccum.reserve(kFrameSize);
    mFrameScratch.resize(kFrameSize);
}

RnnoiseFilter::~RnnoiseFilter()
{
    if (mState)
    {
        rnnoise_destroy(mState);
        mState = nullptr;
    }
}

size_t RnnoiseFilter::Process(const float* in, size_t frameCount, float* out, size_t outCapacity)
{
    assert(outCapacity >= MaxOutputFor(frameCount));
    (void)outCapacity;

    size_t outWritten = 0;
    size_t inIndex = 0;

    while (inIndex < frameCount)
    {
        const size_t need = kFrameSize - mInputAccum.size();
        const size_t take = (frameCount - inIndex) < need ? (frameCount - inIndex) : need;

        for (size_t i = 0; i < take; ++i)
        {
            mInputAccum.push_back(in[inIndex + i] * kInt16Scale);
        }
        inIndex += take;

        if (mInputAccum.size() < kFrameSize)
        {
            break;
        }

        // rnnoise_process_frame can read and write the same buffer,
        // but we hold scratch separate so the caller's output buffer
        // can be smaller than kFrameSize mid-call without aliasing concerns.
        rnnoise_process_frame(mState, mFrameScratch.data(), mInputAccum.data());
        mInputAccum.clear();

        for (size_t i = 0; i < kFrameSize; ++i)
        {
            out[outWritten + i] = mFrameScratch[i] / kInt16Scale;
        }
        outWritten += kFrameSize;
    }

    return outWritten;
}

size_t RnnoiseFilter::Flush(float* out)
{
    if (mInputAccum.empty())
    {
        return 0;
    }
    // Pad with zeros to a full frame so the model sees the same shape
    // it always does. The tail is short enough (<10 ms) that the model
    // attack on the zero-pad is inaudible.
    mInputAccum.resize(kFrameSize, 0.0f);
    rnnoise_process_frame(mState, mFrameScratch.data(), mInputAccum.data());
    mInputAccum.clear();
    for (size_t i = 0; i < kFrameSize; ++i)
    {
        out[i] = mFrameScratch[i] / kInt16Scale;
    }
    return kFrameSize;
}
