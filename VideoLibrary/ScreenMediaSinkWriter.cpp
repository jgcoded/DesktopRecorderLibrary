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
#include "DxResource.h"
#include "ScreenMediaSinkWriter.h"

using namespace  winrt::Windows::Media::MediaProperties;

ScreenMediaSinkWriter::ScreenMediaSinkWriter(const EncodingContext& encodingContext)
    : mVideoInputMediaType{ encodingContext.videoInputMediaType }
    , mAudioInputMediaType{ encodingContext.audioInputMediaType }
    , mIsWriting{ false }
    , mWriteStartTime{ std::chrono::nanoseconds{ MAXLONGLONG } }
    , mDevice{ encodingContext.device }
    , mAudioStreamIndex { 0 }
    , mAudioBaselineTime{ 0 }
    , mAudioBaselineSet{ false }
{
    auto mediaEncodingProfile = MediaEncodingProfile::CreateMp4(encodingContext.resolutionOption);

    auto videoProps = mediaEncodingProfile.Video();
    auto bitRate = videoProps.Bitrate();
    auto frameRate = videoProps.FrameRate();
    auto width = videoProps.Width();
    auto height = videoProps.Height();
    auto profile = videoProps.ProfileId();

    if (encodingContext.resolutionOption == ResolutionOption::Auto)
    {
        winrt::check_hresult(MFGetAttributeSize(mVideoInputMediaType.get(), MF_MT_FRAME_SIZE, &width, &height));
    }

    if (encodingContext.frameRate != 0)
    {
        frameRate.Numerator(encodingContext.frameRate);
        frameRate.Denominator(1);
    }

    if (encodingContext.bitRate != 0)
    {
        bitRate = encodingContext.bitRate;
    }

    mVideoFrameDuration = (10 * 1000 * 1000) / (frameRate.Numerator() / frameRate.Denominator());

    // init
    winrt::check_hresult(MFCreateDXGIDeviceManager(&mManagerResetToken, mDeviceManager.put()));

    winrt::check_hresult(mDeviceManager->ResetDevice(mDevice.get(), mManagerResetToken));

    // create attributes
    winrt::check_hresult(MFCreateAttributes(mSinkWriterAttributes.put(), 4));
    winrt::check_hresult(mSinkWriterAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));
    winrt::check_hresult(mSinkWriterAttributes->SetUINT32(MF_LOW_LATENCY, true));
    winrt::check_hresult(mSinkWriterAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, true));
    winrt::check_hresult(mSinkWriterAttributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, mDeviceManager.get()));

    // create sink writer;
    winrt::check_hresult(MFCreateSinkWriterFromURL(
        encodingContext.fileName.c_str(),
        nullptr,
        mSinkWriterAttributes.get(),
        mSinkWriter.put()));

    // Create video output media type
    winrt::check_hresult(MFCreateMediaType(mVideoOutputMediaType.put()));
    winrt::check_hresult(mVideoOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_MPEG2_PROFILE, profile));
    winrt::check_hresult(mVideoOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));

    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_AVG_BITRATE, bitRate));
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    // The encoder emits *limited-range* BT.709 YUV regardless of our
    // earlier full-range hints (verified empirically via ffprobe pixel
    // sampling — stored values clip at [16, 235]). Tag the output to
    // match that reality so Windows Media Player / Movies & TV doesn't
    // try to "expand" what's already supposed to be RGB-space, which
    // shows up as oversaturated chroma and lifted blacks.
    //
    // All four of NOMINAL_RANGE, YUV_MATRIX, VIDEO_PRIMARIES, and
    // TRANSFER_FUNCTION must be set together for the MP4 sink to emit
    // a `colr` atom in the container — that's the only signal these
    // players read when the H.264 SPS VUI is absent.
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235));
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709));
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709));
    winrt::check_hresult(mVideoOutputMediaType->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709));
    winrt::check_hresult(MFSetAttributeSize(mVideoOutputMediaType.get(), MF_MT_FRAME_SIZE, width, height));
    winrt::check_hresult(MFSetAttributeRatio(mVideoOutputMediaType.get(), MF_MT_FRAME_RATE, frameRate.Numerator(), frameRate.Denominator()));
    winrt::check_hresult(mSinkWriter->AddStream(mVideoOutputMediaType.get(), &mVideoStreamIndex));

    // set video input media type
    winrt::check_hresult(mSinkWriter->SetInputMediaType(mVideoStreamIndex, mVideoInputMediaType.get(), nullptr));

    // Best-effort: ask the encoder via ICodecAPI to use 0-255 RGB input
    // and 16-235 YUV output for its internal color conversion. Microsoft's
    // built-in H.264 MFT returns E_NOTIMPL on both properties (verified
    // empirically), so this is a no-op there; left in place because
    // third-party encoders sometimes honor it. The MP4 container's colr
    // atom is what actually carries the tags downstream.
    {
        winrt::com_ptr<ICodecAPI> codec;
        if (SUCCEEDED(mSinkWriter->GetServiceForStream(
                mVideoStreamIndex, GUID_NULL, IID_PPV_ARGS(codec.put())))
            && codec)
        {
            VARIANT range;
            VariantInit(&range);
            range.vt = VT_UI4;
            range.ulVal = eAVEncVideoColorNominalRange_0_255;
            (void)codec->SetValue(&CODECAPI_AVEncVideoInputColorNominalRange, &range);
            range.ulVal = eAVEncVideoColorNominalRange_16_235;
            (void)codec->SetValue(&CODECAPI_AVEncVideoOutputColorNominalRange, &range);
            VariantClear(&range);
        }
    }

    if (!mAudioInputMediaType)
    {
        return;
    }

    auto audioQuality = encodingContext.audioQuality;

    if (audioQuality == AudioQuality::Auto)
    {
        // Medium (96 kbps stereo) is too aggressive for voice — produces
        // the warbly AAC artifacts users associate with bad recordings.
        // High (~192 kbps stereo) is a meaningful audible improvement.
        audioQuality = AudioQuality::High;
    }

    auto audioProps = MediaEncodingProfile::CreateM4a(audioQuality).Audio();

    UINT32 audioBitsPerSample = audioProps.BitsPerSample();
    UINT32 audioSampleRate = audioProps.SampleRate();
    UINT32 audioNumChannels = audioProps.ChannelCount();
    UINT32 audioBitrate = audioProps.Bitrate() / 8;

    // Match the encoder output to the source's actual sample rate and
    // channel layout. Otherwise MF has to resample (e.g. 48 kHz mic ->
    // 44.1 kHz output) or up/downmix, which costs CPU and can introduce
    // audible artifacts on top of the AAC compression.
    UINT32 sourceSampleRate = 0;
    if (SUCCEEDED(mAudioInputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sourceSampleRate))
        && sourceSampleRate > 0)
    {
        audioSampleRate = sourceSampleRate;
    }

    UINT32 sourceChannels = 0;
    if (SUCCEEDED(mAudioInputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &sourceChannels))
        && sourceChannels > 0)
    {
        // Only scale UP when the source has more channels than the
        // profile (very unusual). Don't scale DOWN for mono: a "High"
        // quality choice should mean High whether the source is mono
        // or stereo. Mono at the profile's nominal stereo bitrate is
        // unambiguously high-quality voice (192 kbps AAC mono is far
        // beyond conference-call needs).
        if (audioNumChannels > 0 && sourceChannels > audioNumChannels)
        {
            audioBitrate = static_cast<UINT32>(
                (static_cast<uint64_t>(audioBitrate) * sourceChannels) / audioNumChannels);
        }
        audioNumChannels = sourceChannels;
    }

    // AAC output media type https://msdn.microsoft.com/en-us/library/dd742785(v=vs.85).aspx
    winrt::check_hresult(MFCreateMediaType(mAudioOutputMediaType.put()));
    winrt::check_hresult(mAudioOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    winrt::check_hresult(mAudioOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, audioBitsPerSample));
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, audioSampleRate));
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, audioNumChannels));
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, audioBitrate));

    constexpr UINT32 aacPayloadType = 0; // stream contains raw_data_block elements only
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, aacPayloadType));

    // value of the audioProfileLevelIndication field, as defined by ISO/IEC 14496-3
    constexpr UINT32 aacProfileLevel = 0x29;
    winrt::check_hresult(mAudioOutputMediaType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, aacProfileLevel));

    winrt::check_hresult(mSinkWriter->AddStream(mAudioOutputMediaType.get(), &mAudioStreamIndex));

    // set audio input media type
    winrt::check_hresult(mSinkWriter->SetInputMediaType(mAudioStreamIndex, mAudioInputMediaType.get(), nullptr));
}

void ScreenMediaSinkWriter::Begin()
{
    std::lock_guard<std::mutex> lock{ mMutex };
    mIsWriting = true;
    mAudioBaselineSet = false;
    try
    {
        winrt::check_hresult(mSinkWriter->BeginWriting());
        mWriteStartTime = std::chrono::high_resolution_clock::now();
    }
    catch (...)
    {
        mIsWriting = false;
        throw;
    }
}

void ScreenMediaSinkWriter::SignalGap()
{
    std::lock_guard<std::mutex> lock{ mMutex };
    if (!mIsWriting)
    {
        throw std::bad_function_call();
    }

    auto frameCaptureTime = std::chrono::high_resolution_clock::now();
    auto frameTime = (frameCaptureTime - mWriteStartTime).count() / 100;

    mSinkWriter->SendStreamTick(mVideoStreamIndex, frameTime);
}

void ScreenMediaSinkWriter::ResetDevice(winrt::com_ptr<ID3D11Device> device)
{
    std::lock_guard<std::mutex> lock{ mMutex };
    if (!mIsWriting)
    {
        throw std::bad_function_call();
    }

    mDeviceManager->ResetDevice(device.get(), mManagerResetToken);
    mDevice = device;
}

void ScreenMediaSinkWriter::WriteSample(IMFSample* sample)
{
    std::lock_guard<std::mutex> lock{ mMutex };
    if (!mIsWriting)
    {
        throw std::bad_function_call();
    }

    GUID sampleType;
    winrt::check_hresult(sample->GetGUID(MF_MT_MAJOR_TYPE, &sampleType));

    if (sampleType == MFMediaType_Video)
    {
        // If the caller (Pipeline) already attached a presentation time
        // derived from the GPU's frame present timestamp, keep it. That
        // preserves the encoded timeline through dropped/skipped frames.
        // Otherwise fall back to wall-clock relative to Begin().
        LONGLONG existingTime = 0;
        HRESULT timeHr = sample->GetSampleTime(&existingTime);
        if (timeHr == MF_E_NO_SAMPLE_TIMESTAMP)
        {
            auto frameCaptureTime = std::chrono::high_resolution_clock::now();
            auto frameTime = (frameCaptureTime - mWriteStartTime).count() / 100;
            winrt::check_hresult(sample->SetSampleTime(frameTime));
        }
        else
        {
            winrt::check_hresult(timeHr);
        }
        winrt::check_hresult(sample->SetSampleDuration(mVideoFrameDuration));

        mSinkWriter->WriteSample(mVideoStreamIndex, sample);
    }
    else if (sampleType == MFMediaType_Audio)
    {
        // Audio sample times come from the MF source reader in its own
        // 100ns clock domain (epoch depends on the source). Rebase to the
        // first sample we receive so audio starts at 0 in the output;
        // MF aligns the audio and video tracks from there.
        LONGLONG sampleTime;
        winrt::check_hresult(sample->GetSampleTime(&sampleTime));
        if (!mAudioBaselineSet)
        {
            mAudioBaselineTime = sampleTime;
            mAudioBaselineSet = true;
        }
        winrt::check_hresult(sample->SetSampleTime(sampleTime - mAudioBaselineTime));
        mSinkWriter->WriteSample(mAudioStreamIndex, sample);
    }
}

void ScreenMediaSinkWriter::End()
{
    std::lock_guard<std::mutex> lock{ mMutex };

    if (!mIsWriting) {
        throw std::exception("End called when ScreenMediaSinkWriter was not writing");
    }

    winrt::check_hresult(mSinkWriter->Flush(mVideoStreamIndex));
    if (mAudioInputMediaType)
    {
        winrt::check_hresult(mSinkWriter->Flush(mAudioStreamIndex));
    }
    winrt::check_hresult(mSinkWriter->Finalize());
    mIsWriting = false;
}

ScreenMediaSinkWriter::~ScreenMediaSinkWriter()
{
    if (mIsWriting)
    {
        this->End();
    }
}
