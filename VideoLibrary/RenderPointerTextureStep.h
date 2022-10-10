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
#include "ShaderCache.h"
#include "TexturePool.h"
#include "SharedSurface.h"

class RenderPointerTextureStep : public RecordingStep
{
public:
    RenderPointerTextureStep(
        std::shared_ptr<DesktopPointer> desktopPointer,
        std::shared_ptr<SharedSurface> sharedSurface,
        winrt::com_ptr<ID3D11Device> device,
        std::shared_ptr<ShaderCache> shaderCache,
        winrt::com_ptr<TexturePool> texturePool,
        RECT virtualDesktopBounds,
        RECT desktopMonitorBounds);

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

    winrt::com_ptr<ID3D11Device> mDevice;
    std::shared_ptr<DesktopPointer> mDesktopPointer;
    std::shared_ptr<ShaderCache> mShaderCache;
    std::shared_ptr<SharedSurface> mSharedSurface;
    winrt::com_ptr<TexturePool> mTexturePool;

    ID3D11Texture2D* mSharedSurfacePtr;

    winrt::com_ptr<ID3D11Texture2D> mResult;

    RECT mVirtualDesktopBounds;
    RECT mDesktopMonitorBounds;
};
