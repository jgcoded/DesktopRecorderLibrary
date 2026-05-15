# VideoLibrary Pipeline Audit

## What works well

- **Step-based composition is a clean abstraction.** Each step has one `Perform()` and one optional `Result()`; `Pipeline::Perform` reads top-to-bottom like a recipe. Easy to insert/remove a step (e.g., to swap in a multi-monitor compositor).
- **GPU-resident path.** `MFCreateDXGISurfaceBuffer` + the DXGI device manager hands the duplicated texture to the H.264 MFT without any CPU round-trip — this is the right shape for a screen recorder.
- **`MFTrackedSample` + `TexturePool::Invoke`.** Returning the texture to a pool when MF releases the sample is the correct way to avoid per-frame `CreateTexture2D` calls along the hot path. The wiring is small and works.
- **Move/dirty rects respected.** Most pipelines just copy the whole desktop image every frame; this one actually consumes the partial-update info Desktop Duplication gives you. That's a real perf win.
- **Recoverable-error model.** `RecoverableVideoException` + `ThrowExceptionCheckRecoverable` distinguishes "rebuild the pipeline" (TDR / monitor rotation / access-lost) from fatal errors; `SampleApp` then restarts cleanly. The error vectors in `Errors.h` are taken from the Microsoft reference sample, which is the right source of truth.
- **`DxMultithread` RAII guard** correctly enforces `ID3D10Multithread::SetMultithreadProtected` whenever MF and D3D share a context.

## Real bugs

1. **`Pipeline.cpp:55` — `mSample == nullptr;` is a no-op comparison, not an assignment.** Every early-return path below it (`!lock->Locked()` on line 64, `renderPointer.Result() == nullptr` on line 130) leaves `mSample` holding the previous frame's `IMFSample`. `SampleApp` then re-writes that stale sample. Should be `mSample = nullptr;`. This is the single most impactful bug in the pipeline.

2. **`KeyedMutexLock.h:91-97` — destructor throws on timeout.** If `AcquireSync` returns `WAIT_TIMEOUT`, `mLocked` is false but the destructor still calls `mMutex->ReleaseSync(...)` (which will fail with `E_INVALIDARG` — no matching acquire) and the failure flows through `winrt::check_hresult` *inside the destructor*. That throws during stack unwinding → `std::terminate`. The destructor must short-circuit when `!mLocked` (and **not** rotate the keys either, otherwise the next acquire is permanently desynced).

3. **`RenderMoveRectsStep.cpp:113-121` — `ROTATE270` source-rect math is wrong.** Compare it to the `ROTATE90` branch (lines 88-92): the symmetric mapping for 270 should use `SourcePoint.y` and `height` where the current code uses `SourcePoint.x` and `width`. Specifically `srcRect.left` should be `SourcePoint.y` (mirror of 90's `srcRect.top = SourcePoint.x`). The current code references `SourcePoint.x` three times and `SourcePoint.y` once, which can't be right for a 90-degree CCW rotation. Scrolls / move-rects on portrait-flipped monitors will smear.

4. **`RenderPointerTextureStep.cpp:258-260` — masked-pointer left-edge clip is wrong.**
   ```cpp
   if (left < 0) {
       left = 0;
       maskX = -left;   // left was just set to 0, so maskX is always 0
       width += left;   // and `left` is 0, so width is unchanged
   }
   ```
   The reads happen *after* `left = 0`. As a result `maskX` is always 0 and `width` isn't reduced — the masked cursor renders garbage source pixels when it hangs off the left edge.

5. **`RenderPointerTextureStep.cpp:95-101` — color pointer left/top clip has the same flavor of bug.** `pos.x = 0` is set but the source offset into the pointer color data isn't shifted; the cursor still uploads `shape.Width` pixels starting at source offset 0, so the visible cursor doesn't match what the OS draws.

6. **`Errors.cpp:26-28` — null-check on the wrong side.**
   ```cpp
   HRESULT deviceRemovedReason = device->GetDeviceRemovedReason();
   if (device) { ... }
   ```
   Dereferences `device` before checking it. In the recoverable path that's usually fine (every caller passes a real device), but the safety net is illusory.

7. **`ScreenMediaSinkWriter.cpp:203-208` — audio timestamp normalization is incorrect.** The video branch uses elapsed time (`now() - mWriteStartTime`). The audio branch does `sampleTime - mWriteStartTime.time_since_epoch().count() / 100`, which mixes "epoch-relative ticks of the high_resolution_clock start" with "MF source-reader sample time" (a separate clock domain). On Windows these are not the same clock; the offset is nonsense. Audio drifts against video over long recordings.

8. **`Frame::PresentationTime()` is dead and dishonest.** Returns raw `LastPresentTime.QuadPart` (QPC ticks) with the conversion to nanoseconds commented out and a name claiming nanoseconds. Either delete it or finish it.

9. **`Pipeline.cpp:55` (related to #1) — `mSample` is never proactively cleared on an early return.** Even after fixing the `==`/`=` typo, the contract "Sample() returns null after a missed frame" only holds because of that one line. Document it or use `std::optional<com_ptr>` so the type system enforces it.

10. **`TexturePool::Acquire` doesn't check `CreateTexture2D` for failure** (line 82). Returns a null `com_ptr`. Downstream `winrt::check_pointer` catches it eventually, but each step does its own check inconsistently — `RenderPointerTextureStep::Perform` uses `virtualDesktopCopy` without a null check.

## Architectural weaknesses

### A. ~~The cross-device sharing path is dead, but pays its cost every frame~~ — RETRACTED

> The original audit recommended deleting `KeyedMutexLock` / `RotatingKeys` / `OpenSharedSurfaceWithDevice` / the `SHARED_KEYEDMUTEX` flag on the grounds that no caller exercises them today. That recommendation was wrong on premise.
>
> Multi-monitor recording where monitors hang off different GPUs is the next planned feature. That case *requires* a keyed-mutex shared texture so a secondary adapter's duplicator (or the sink writer running on a different device) can open the surface produced on the primary duplicator's device. The infrastructure exists precisely to make that possible.
>
> The per-frame keyed-mutex acquire/release pair is the cost of supporting cross-GPU compositing — it's intended, not dead weight. The destructor short-circuit (item #2 in *Real bugs*) still needed fixing on its own.

### B. Per-frame allocation churn

These objects are created from scratch every frame even though their descriptors never change:

| Object | Where | Could be cached on |
|---|---|---|
| Dirty-rect vertex buffer | `RenderDirtyRectsStep.cpp:299-300` | `Pipeline` (grow as needed; `D3D11_USAGE_DYNAMIC` + Map/Unmap) |
| SRV for the desktop image | `RenderDirtyRectsStep.cpp:262-266` | per-`ScreenDuplicator` |
| RTV for the pointer-step target | `RenderPointerTextureStep.cpp:170-175` (TODO comment in code) | per-`TexturePool` entry |
| SRV for the mouse texture | `RenderPointerTextureStep.cpp:138-143` | per-shape ID; invalidate on `ShapeInfo()` change |
| Mouse pointer `ID3D11Texture2D` | `RenderPointerTextureStep.cpp:432-433` | `DesktopPointer::UpdateTexture` *already exists* but is never called |

`DesktopPointer` already has the right shape (`mIsPointerTextureStale`, `UpdateTexture`, `Texture`) for caching the pointer texture; the producer side just isn't wired up. That's a low-effort, high-yield fix.

### C. `Frame` constructor has side effects

`Frame::Frame()` (a) calls `AcquireNextFrame`, (b) mutates the shared `DesktopPointer`'s buffer/shape/position, (c) parses move/dirty rects, and (d) translates errors. That's four responsibilities, and the pointer-state mutation in particular makes `Frame` impossible to reason about as a value type. Split into:
- `Frame::Acquire(duplicator)` — only the AcquireNextFrame + rects + RAII release.
- `DesktopPointer::UpdateFromFrameInfo(...)` — takes the frame info + buffer access + duplicator output; the pointer owns its own state mutation.

This also gets `Errors.h` out of the value type's translation unit.

### D. Timestamps come from wall clock, not the GPU

`ScreenMediaSinkWriter::WriteSample` builds video PTS from `high_resolution_clock::now() - mWriteStartTime`. `Frame` already carries `mFrameInfo.LastPresentTime` (QPC ticks) — that's the actual GPU present time and is far more accurate than wall-clock-on-sample-write. The sink writer should accept a presentation time per sample and the pipeline should pass `Frame::PresentationTime()` through (after fixing the conversion).

Knock-on benefit: dropped frames (timeouts, lock contention) won't compress the timeline, and `SignalGap` becomes meaningful again.

### E. State-bind layering is fragile

`ShaderCache::Initialize` calls `IASetInputLayout` once on construction, and no step ever rebinds it. Steps mix:
- `OMSetBlendState(nullptr, ...)` in `RenderDirtyRectsStep::RenderDirtyRects` (disables blend, line 278)
- `OMSetBlendState(mShaderCache->BlendState(), ...)` in `RenderPointerTextureStep::Perform` (alpha-blend on, line 189)

There's no convention for what state a step expects on entry vs. what it leaves behind. Today the order is fixed so it works; the moment a step is reordered or duplicated it breaks subtly. Either:
- Each step is required to fully set its render state (input layout, blend, rasterizer, viewport, RTV) on entry, or
- The Pipeline maintains a state object and pushes/pops a snapshot per step.

I'd recommend the first — it's more code per step but the steps become self-contained and you can test them in isolation (which the existing `RecordingStepsTests` is gesturing at but can't realize today).

### F. `Pipeline` knows too much about lifecycle

`Pipeline::Perform` allocates the texture pool, staging texture, and render target view lazily inside the per-frame method, but they live for the whole recording. That coupling makes it hard to:
- React to monitor rotation / desktop-bounds change (would need to invalidate the pool but `Pipeline` has no signal).
- Reuse the pipeline across recordings.

Move resource allocation to a separate `Initialize()` method (or the ctor, once `Frame` no longer side-effects mid-call), and add a `Reconfigure(RECT newBounds)` that the host thread can call when a recoverable error fires.

### G. `RecordingStep` as a base class buys very little

`RecordingStep` is just `virtual void Perform() = 0;`. There's no dynamic dispatch happening — `Pipeline` constructs each step by concrete type, calls `Perform()` once, then reads a step-specific `Result()` typed differently in each subclass. There's no list of steps and no iteration over them. The polymorphism is paying for itself nowhere.

Two reasonable directions:
- **Delete it.** Make each step a free function or a small struct with `Perform()` and a typed `Result()`. Honesty about the data flow.
- **Make it real.** If you want to make steps composable (e.g., for multi-monitor compositing — see the TODO at `RenderPointerTextureStep.cpp:67`), give `RecordingStep` a typed-input/typed-output contract (templates) and let `Pipeline` actually iterate a `vector<unique_ptr<RecordingStep>>` so steps can be added/reordered at runtime.

The current shape is the worst of both: virtual-call overhead with no polymorphic benefit.

### H. Multi-monitor support is gated by `RenderPointerTextureStep`

The library exposes `VirtualDesktop::VirtualDesktopBounds()` and the sample writer is configured to record the entire virtual desktop, but `Pipeline` only takes **one** `ScreenDuplicator`. The TODO at `RenderPointerTextureStep.cpp:244` ("in multi monitor recording, will need to FIRST merge all desktop images and then draw the mouse") is the central missing piece. The current architecture would need:

- `Pipeline` to accept `vector<shared_ptr<ScreenDuplicator>>` and produce a single shared surface from N duplicator outputs.
- A new compositing step between `RenderDirtyRectsStep` and `RenderPointerTextureStep` that iterates duplicators and renders each into its own region of the virtual desktop.
- Pointer rendering after compositing, in virtual-desktop coordinates (which `DesktopPointer::UpdatePosition` is already translating to — good).

Doing this cleanly is the main reason to address (G) above.

### I. Audio capture has no voice DSP

`AudioMedia::GetAudioMediaSourceFromEndpoint` uses `MFCreateDeviceSource` with `MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID`. This is the *raw* mic capture path — no acoustic echo cancellation, no noise suppression, no automatic gain control. Teams/Zoom/Discord all engage Windows Communications-mode capture, which routes the mic through the system's Audio Processing Objects (APOs) for AEC/NS/AGC. The current path bypasses all of that, so HVAC noise, room reverb, plosives, and AGC swings come through unfiltered.

A higher AAC bitrate / source-matching sample rate (already done) helps the codec side but doesn't fix the underlying raw-mic signal.

Two routes for a proper fix:

- **`Windows.Media.Capture` with `AudioProcessing::Default`** — the UWP API. Engages the system's voice DSP automatically. Cleanest option for "conference-call quality"; interops with MF via `MediaCapture::StartRecordToCustomSinkAsync` or by surfacing the underlying `IMFMediaSource`.
- **Custom WASAPI capture in Communications role + wrap as `IMFMediaSource`** — more code, lower-level control over which APOs are enabled. Worth it only if you need fine-grained DSP toggles.

The first option is what most modern Windows desktop recorders do.

## Suggested priority

1. Fix the `==`/`=` typo (P0 — corrupts every recording with a missed frame).
2. Fix `KeyedMutexLock` destructor (P0 — `std::terminate` on timeout). Keep the class; multi-GPU recording will rely on it.
3. Fix `ROTATE270` move-rect math + the two pointer left-edge clip bugs (P1 — visible artifacts on rotated displays / edge cursor).
4. Cache pointer texture + per-pool-texture RTV + persistent vertex buffer (P1 — measurable per-frame perf).
5. ~~Decide and delete (or properly use) the cross-device SharedSurface path~~ — retracted, see section A.
6. Move timestamps to GPU present time, fix audio normalization (P2 — A/V drift).
7. Engage Communications-mode capture for voice DSP (P2 — see section I).
8. Split `Frame`'s responsibilities; resource init out of `Perform`; either kill `RecordingStep` or commit to it (P3 — pays off when multi-monitor work starts).
