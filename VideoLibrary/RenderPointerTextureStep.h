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

#include "RecordingStep.h"
#include "VirtualDesktop.h"
#include "ShaderCache.h"
#include "TexturePool.h"

class RenderPointerTextureStep : public RecordingStep
{
public:
    RenderPointerTextureStep(
        std::shared_ptr<DesktopMonitor::ScreenDuplicator> screenDuplicator,
        std::shared_ptr<ShaderCache> shaderCache,
        winrt::com_ptr<TexturePool> texturePool,
        winrt::com_ptr<ID3D11Texture2D> sharedSurface);

    virtual ~RenderPointerTextureStep();

    virtual void Perform() override;

    winrt::com_ptr<ID3D11Texture2D> Result();

private:
    winrt::com_ptr<ID3D11Texture2D> MakePointerTexture();
    winrt::com_ptr<ID3D11Texture2D> MakeColorPointerTexture();
    winrt::com_ptr<ID3D11Texture2D> MakeMaskedPointerTexture();

    void MakeMonochromePointerBuffer(UINT* dest, const UINT * colorData, UINT colorPitch, byte* maskData, UINT maskPitch, UINT width, UINT height, UINT maskX, UINT maskY, UINT maskHeight);
    void MakeMaskedColorPointerBuffer(UINT * dest, const UINT * colorData, UINT colorPitch, UINT * maskData, UINT maskPitch, UINT width, UINT height, UINT maskX, UINT maskY);
    winrt::com_ptr<ID3D11Texture2D> MakeColorPointer(byte* data, int width, int height);

    std::shared_ptr<DesktopMonitor::ScreenDuplicator> mScreenDuplicator;
    std::shared_ptr<ShaderCache> mShaderCache;
    winrt::com_ptr<TexturePool> mTexturePool;
    winrt::com_ptr<ID3D11Texture2D> mSharedSurface;

    winrt::com_ptr<ID3D11Texture2D> mResult;
};
