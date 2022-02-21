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
#include "Vertex.h"

class RenderDirtyRectsStep : public RecordingStep
{
public:
    RenderDirtyRectsStep(
        std::shared_ptr<DesktopMonitor::ScreenDuplicator::Frame> frame,
        std::shared_ptr<std::vector<Vertex>> vertexBuffer,
        std::shared_ptr<ShaderCache> shaderCache,
        winrt::com_ptr<ID3D11Texture2D> sharedSurface,
        winrt::com_ptr<ID3D11RenderTargetView> renderTargetView
        );
    ~RenderDirtyRectsStep();

    // Inherited via RecordingStep
    virtual void Perform() override;

private:
    
    void UpdateDirtyRects();

    void RenderDirtyRects();

    std::shared_ptr<DesktopMonitor::ScreenDuplicator::Frame> mFrame;
    std::shared_ptr<std::vector<Vertex>> mVertexBuffer;
    std::shared_ptr<ShaderCache> mShaderCache;
    winrt::com_ptr<ID3D11Texture2D> mSharedSurface;
    winrt::com_ptr<ID3D11RenderTargetView> mRenderTargetView;
};
