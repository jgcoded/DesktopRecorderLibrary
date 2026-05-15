# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build / Test

This is a Visual Studio C++ solution targeting Windows 10 (min SDK 10.0.17134.0), built with `v143` toolset on x64 (Win32/x86 also configured). It is **not buildable from this WSL/Linux environment** — point users at Visual Studio 2022 or `MSBuild.exe` from a Windows host.

- Solution file: `DesktopRecorderLibrary.sln`
- Projects: `VideoLibrary` (static lib), `SampleApp` (Win32 exe), `VideoLibraryTests` (MSTest native unit test DLL using `Microsoft::VisualStudio::CppUnitTestFramework`)
- Configurations: `Debug|x64`, `Release|x64`, `Debug|Win32`, `Release|Win32`
- NuGet must be restored first (`Microsoft.Windows.CppWinRT.2.0.220608.4` is required) — `nuget restore DesktopRecorderLibrary.sln` or via VS.
- Build all: `msbuild DesktopRecorderLibrary.sln /p:Configuration=Debug /p:Platform=x64`
- Run tests: `vstest.console.exe x64\Debug\VideoLibraryTests.dll`
- Run a single test: `vstest.console.exe x64\Debug\VideoLibraryTests.dll /Tests:RecordDesktop`
- Tests exercise real Direct3D / Desktop Duplication APIs, so they require an interactive Windows desktop session with a GPU — they will fail in headless CI.

Each subproject uses a precompiled header (`pch.h` for VideoLibrary/SampleApp, `stdafx.h` for tests). New `.cpp` files must `#include "pch.h"` (or `stdafx.h`) as the very first include.

## Architecture

The library wraps three Windows stacks — **Desktop Duplication API**, **Direct3D 11**, and **Media Foundation** — and glues them together with a step-based pipeline that runs once per frame.

### Frame-flow ownership

A consumer typically holds these long-lived objects (see `SampleApp/main.cpp:PipelineThread` for the canonical wiring):

1. `VirtualDesktop` — enumerates `DesktopMonitor`s and computes the bounding rect across all monitors. The library distinguishes per-monitor bounds from the *virtual desktop* bounds; the latter is the canvas pointer rendering composes against.
2. `DesktopPointer` — shared cursor state across all duplicators on the virtual desktop. One instance is shared even when multiple monitors are duplicated, because the Desktop Duplication API reports pointer info on whichever output the cursor most recently moved over.
3. `ScreenDuplicator` (one per recorded monitor) — owns its own `ID3D11Device`, the `IDXGIOutputDuplication`, and the byte buffer used to read move/dirty rects.
4. `SharedSurface` — a D3D11 texture created with `D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX` so it can be opened on another device. The duplicator device writes into it; the sink writer's device reads from it. Access is gated by `KeyedMutexLock` + `RotatingKeys` (two keys that swap on every release — required because keyed mutexes use distinct acquire/release keys).
5. `Pipeline` — orchestrates the per-frame steps below.
6. `ScreenMediaSinkWriter` — wraps `IMFSinkWriter`; configured via an `EncodingContext` struct.

Lifetime ordering matters: `ScreenMediaSinkWriter::End()` must run before the duplicator's `ID3D11Device` is released, and `MFShutdown()` only after every MF object is gone. `SampleApp/main.cpp` releases the duplicator after the writer, then calls `ID3D11Debug::ReportLiveDeviceObjects` in debug builds — preserve this ordering when refactoring.

### The recording pipeline (`Pipeline::Perform`)

All concrete frame work is implemented as subclasses of `RecordingStep` (a `Perform()`-only abstract base). `Pipeline` composes them per frame:

1. `CaptureFrameStep` — calls `AcquireNextFrame`, returns a `Frame` carrying desktop image + move/dirty rects + monitor bounds.
2. `RenderMoveRectsStep` — copies regions of the previous frame to a staging texture to apply scroll-style move rects.
3. `RenderDirtyRectsStep` — uploads `Vertex` quads for each dirty rect and runs `VertexShader.hlsl` / `PixelShader.hlsl` via `ShaderCache` to blit them into the `SharedSurface`.
4. `RenderPointerTextureStep` — composites the cursor (color or monochrome / masked) onto a pool-allocated texture sized to the virtual desktop, accounting for monitor rotation.
5. `TextureToMediaSampleStep` — wraps the composed texture as an `IMFSample` for the sink writer.

`TexturePool` recycles intermediate textures; `ShaderCache` lazily compiles/binds shaders so the per-frame path stays allocation-light.

### Cross-device safety

The duplicator's device and the sink writer's device are distinct. Anywhere D3D resources cross between them — primarily the `SharedSurface` — go through `KeyedMutexLock`. Any code touching MF + D3D from the same thread also constructs a stack-scoped `DxMultithread` (`ID3D10Multithread::SetMultithreadProtected`), because the MF DXGI device manager requires it (see comment in `Pipeline.cpp:58`).

### Audio path

Audio is independent of the video pipeline: `AudioMedia::GetAudioMediaSourceFromEndpoint` produces an `IMFMediaSource`, and `AsyncMediaSourceReader` drives it asynchronously. Samples arrive via a callback that calls `ScreenMediaSinkWriter::WriteSample` with `MF_MT_MAJOR_TYPE = MFMediaType_Audio`. Video samples take the same path but with `MFMediaType_Video` — the sink writer dispatches by major type, so callers must set it before calling `WriteSample`.

### Recoverable errors

`Errors.h` defines `RecoverableVideoException` plus three vectors of expected `HRESULT`s for system transitions (mode change, TDR, session disconnect, monitor rotation). `ThrowExceptionCheckRecoverable` is the canonical way to convert a failed DXGI/duplication call into either a recoverable exception (caller should rebuild the pipeline) or a fatal one. The `SampleApp` message loop restarts recording when the pipeline thread sets `stopThread` with a non-`S_OK` `stopThreadResult` — preserve this restart-on-recoverable-error pattern in any new recording host.

### Desktop-attach for secure-desktop transitions

The recording thread calls `OpenInputDesktop` + `SetThreadDesktop` before doing any work (`SampleApp/main.cpp:SetupPipelineThread`). Without this, recording breaks whenever the user hits a UAC prompt or locks the screen. Any new thread that drives the pipeline needs the same dance.

## Conventions

- C++17, `/permissive-`, warning level 4, `/bigobj`, `NOMINMAX` set globally.
- Uses C++/WinRT (`winrt::com_ptr`, `winrt::check_hresult`, `winrt::check_pointer`, `winrt::to_hresult`) — prefer these over raw COM and `HRESULT` checks.
- Shaders are compiled to headers (`VertexShader.h`, `PixelShader.h`) via FxCompile and consumed as byte arrays named `g_<filename>RawData`.
- Wide strings throughout (`std::wstring`, `hstring`).
- Source files carry a GPLv3 header — keep it on new files.
