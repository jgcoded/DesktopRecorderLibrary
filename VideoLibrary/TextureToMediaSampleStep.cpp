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
#include "TexturePool.h"
#include "TextureToMediaSampleStep.h"

TextureToMediaSampleStep::TextureToMediaSampleStep(
    winrt::com_ptr<ID3D11Texture2D> sourceTexture,
    winrt::com_ptr<class TexturePool> texturePool)
    : mSourceTexture{ sourceTexture }
    , mTexturePool{ texturePool }
{
    winrt::check_pointer(sourceTexture.get());
    winrt::check_pointer(texturePool.get());
}

TextureToMediaSampleStep::~TextureToMediaSampleStep()
{
}

void TextureToMediaSampleStep::Perform()
{
    winrt::com_ptr<IMFMediaBuffer> mediaBuffer;
    winrt::check_hresult(MFCreateDXGISurfaceBuffer(
        __uuidof(ID3D11Texture2D), mSourceTexture.get(), 0, true, mediaBuffer.put()));

    auto mediaBuffer2D{ mediaBuffer.as<IMF2DBuffer>() };

    DWORD length = 0;

    winrt::check_hresult(mediaBuffer2D->GetContiguousLength(&length));
    winrt::check_hresult(mediaBuffer->SetCurrentLength(length));

    winrt::check_hresult(MFCreateVideoSampleFromSurface(nullptr, mSample.put()));

    auto trackedSample = mSample.as<IMFTrackedSample>();

    winrt::check_hresult(trackedSample->SetAllocator(mTexturePool.get(), trackedSample.get()));

    winrt::check_hresult(mSample->AddBuffer(mediaBuffer.get()));
}

winrt::com_ptr<IMFSample> TextureToMediaSampleStep::Result()
{
    return mSample;
}
