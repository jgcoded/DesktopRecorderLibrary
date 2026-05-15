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

#include <cstdint>
#include <vector>

struct DenoiseState;

// DNN-based noise suppression on top of the Communications-mode capture.
//
// CommunicationsAudioCapture already engages Windows' system voice DSP
// (AEC/NS/AGC). On built-in laptop mics that gives only a mild
// improvement; layering RNNoise on top is what bridges to Discord/Teams
// quality on the static-noise side (fans, HVAC, keyboard rumble).
//
// RNNoise operates on 480-sample frames at 48 kHz (10 ms). It takes
// floats but expects them in int16 scale (-32768..32767), not the
// usual normalized -1..1 range — so we rescale on the way in/out.
//
// This wrapper is mono-only because that's what
// CommunicationsAudioCapture delivers after the engine's mix down.
// If multi-channel is ever needed, instantiate one DenoiseState per
// channel and de-interleave/re-interleave around the call.
class RnnoiseFilter
{
public:
    // 10 ms frame at 48 kHz — RNNoise's hard-coded model granularity.
    static constexpr size_t kFrameSize = 480;

    RnnoiseFilter();
    ~RnnoiseFilter();

    RnnoiseFilter(const RnnoiseFilter&) = delete;
    RnnoiseFilter& operator=(const RnnoiseFilter&) = delete;

    // Worst-case output count for a Process call given `frameCount`
    // input samples. The filter accumulates partial frames internally,
    // so if the accumulator already holds kFrameSize-1 samples and the
    // caller adds even one more, a full 480-sample frame is emitted.
    // Callers MUST size their output buffer for this bound — passing
    // a smaller capacity is a contract violation that Process will
    // assert against.
    static constexpr size_t MaxOutputFor(size_t frameCount)
    {
        return frameCount + kFrameSize - 1;
    }

    // Feed an arbitrary count of mono FP32 samples in -1..1 range.
    // Emits whatever full 480-sample frames are ready, in the same
    // -1..1 range, written to `out`. Returns the count of samples
    // written. Anything not yet a full frame is buffered for the
    // next call.
    //
    // `outCapacity` is the number of floats `out` can hold; it must
    // be at least `MaxOutputFor(frameCount)`. Output count can exceed
    // input count when the internal accumulator already held a partial
    // frame from a previous call.
    size_t Process(const float* in, size_t frameCount, float* out, size_t outCapacity);

    // Drain any leftover samples padded with silence. Useful at
    // shutdown if the caller wants flush-out tail samples. Returns
    // count written (0 or kFrameSize). `out` must hold at least
    // kFrameSize floats.
    size_t Flush(float* out);

private:
    DenoiseState* mState;

    // Accumulator for partial frames between Process() calls.
    std::vector<float> mInputAccum;     // up to kFrameSize samples in int16-scale
    std::vector<float> mFrameScratch;   // kFrameSize samples, processed output
};
