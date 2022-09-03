# Desktop Recorder Library

This is the C++ library for a desktop recorder app I worked on for a couple of
years as a hobby project. The app reached 10K monthly active users on the Windows Store.
Here is a demo of what the app looked like, powered by this library:

[![Watch the demo](https://img.youtube.com/vi/GGDT2mmUgYg/maxresdefault.jpg)](https://youtu.be/GGDT2mmUgYg)

## Features

* GPU Accelerated video encoding to MP4
* On-the-fly, GPU-based texture resizing
* Multiple monitor support
* Monitor rotation support
* Microphone Audio Capture
* Mouse capture and rendering

## Dependencies

* Windows Desktop Duplication
* Windows Media Foundation
* DirectX
* Win32

## Minimal example

```cpp
winrt::check_hresult(MFStartup(MF_VERSION));
winrt::init_apartment();

// VirtualDesktop is an abstraction for multimonitor support
auto virtualDesktop = std::make_shared<VirtualDesktop>();

// Pick a monitor to record
std::vector<DesktopMonitor> desktopMonitors = virtualDesktop->GetAllDesktopMonitors();
DesktopMonitor monitor = desktopMonitors[0];

// Create a duplicator for the monitor
std::unique_ptr<DesktopMonitor::ScreenDuplicator> duplicator =
  virtualDesktop->RecordMonitor(monitor);

// The pipeline encapsulates texture rendering
std::unique_ptr<Pipeline> duplicationPipeline = std::make_unique<Pipeline>(duplicator);
duplicationPipeline->Perform();

// The IMFSample that can be used with the ScreenMediaSinkWriter class
winrt::com_ptr<IMFSample> = duplicationPipeline->Sample();

// Get the media type - redacted for brevity
winrt::com_ptr<IMFMediaType> videoMediaType = GetMediaTypeFromDuplicator(*duplicator);

// Encoding options for the video
EncodingContext encodingContext{};
encodingContext.fileName = L"test.mp4";
encodingContext.resolutionOption = ResolutionOption::Auto;
encodingContext.audioQuality = AudioQuality::Auto;
encodingContext.frameRate = 30;
encodingContext.bitRate = 9000000;
encodingContext.videoInputMediaType = videoMediaType;
// See SampleApp for audio recording
encodingContext.audioInputMediaType = nullptr;
encodingContext.device = duplicator->Device();

// Encapsulates the Media Foundation sink writer
auto writer = std::make_unique<ScreenMediaSinkWriter>(encodingContext);
writer->Begin();

// Write the sample
sample->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
writer->WriteSample(sample);

// End finishes recording
writer->End();
writer.reset(nullptr);

winrt::check_hresult(MFShutdown());
```

