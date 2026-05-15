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
    RnnoiseFilter();
    ~RnnoiseFilter();

    RnnoiseFilter(const RnnoiseFilter&) = delete;
    RnnoiseFilter& operator=(const RnnoiseFilter&) = delete;

    // Feed an arbitrary count of mono FP32 samples in -1..1 range.
    // Emits whatever full 480-sample frames are ready, in the same
    // -1..1 range, written to `out`. Returns the count of samples
    // written. Anything not yet a full frame is buffered for the
    // next call.
    //
    // Caller-owned buffers: `out` must hold at least `frameCount`
    // samples (output count is always <= input count for a given
    // call, since we only emit completed frames).
    size_t Process(const float* in, size_t frameCount, float* out);

    // Drain any leftover samples padded with silence. Useful at
    // shutdown if the caller wants flush-out tail samples. Returns
    // count written (0 or 480).
    size_t Flush(float* out);

private:
    DenoiseState* mState;

    // Accumulator for partial frames between Process() calls.
    std::vector<float> mInputAccum;     // up to 480 samples in int16-scale
    std::vector<float> mFrameScratch;   // 480 samples, processed output
};
