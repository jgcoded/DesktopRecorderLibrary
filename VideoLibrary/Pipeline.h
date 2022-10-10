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
#include "TexturePool.h"
#include "DesktopMonitor.h"
#include "DesktopPointer.h"
#include "ScreenDuplicator.h"
#include "Vertex.h"
#include "ShaderCache.h"

class Pipeline : public RecordingStep
{
public:
    Pipeline(
        std::shared_ptr<ScreenDuplicator> duplicator,
        std::shared_ptr<SharedSurface> sharedSurface,
        RECT virtualDesktopBounds
    );

    virtual ~Pipeline();

    // Inherited via RecordingStep
    virtual void Perform() override;

    winrt::com_ptr<IMFSample> Sample() const;

private:

    void AllocateTexturePool();
    void AllocateStagingTexture(winrt::com_ptr<ID3D11Device> device, const D3D11_TEXTURE2D_DESC& desc);

    std::shared_ptr<ScreenDuplicator> mDuplicator;
    std::shared_ptr<SharedSurface> mSharedSurface;
    std::shared_ptr<ShaderCache> mShaderCache;
    std::shared_ptr<std::vector<Vertex>> mVertexBuffer;
    winrt::com_ptr<TexturePool> mTexturePool;
    winrt::com_ptr<ID3D11Texture2D> mStagingTexture;
    winrt::com_ptr<IMFSample> mSample;
    winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
    RECT mVirtualDesktopBounds;
};
