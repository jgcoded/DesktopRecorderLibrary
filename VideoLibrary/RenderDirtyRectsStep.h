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
#include "DesktopMonitor.h"
#include "ShaderCache.h"
#include "Frame.h"
#include "Vertex.h"

class RenderDirtyRectsStep : public RecordingStep
{
public:
    RenderDirtyRectsStep(
        std::shared_ptr<Frame> frame,
        RECT virtualDesktopBounds,
        std::shared_ptr<std::vector<Vertex>> vertexBuffer,
        std::shared_ptr<ShaderCache> shaderCache,
        ID3D11Texture2D* sharedSurfacePtr,
        winrt::com_ptr<ID3D11RenderTargetView> renderTargetView
        );
    ~RenderDirtyRectsStep();

    // Inherited via RecordingStep
    virtual void Perform() override;

private:
    
    void UpdateDirtyRects();

    void RenderDirtyRects();

    std::shared_ptr<Frame> mFrame;
    RECT mVirtualDesktopBounds;
    std::shared_ptr<std::vector<Vertex>> mVertexBuffer;
    std::shared_ptr<ShaderCache> mShaderCache;
    ID3D11Texture2D* mSharedSurfacePtr;
    winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};
