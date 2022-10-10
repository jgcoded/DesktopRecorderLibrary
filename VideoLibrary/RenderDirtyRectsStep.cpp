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
#include "VirtualDesktop.h"
#include "RenderPointerTextureStep.h"
#include "RenderDirtyRectsStep.h"

RenderDirtyRectsStep::RenderDirtyRectsStep(
    std::shared_ptr<Frame> frame,
    RECT virtualDesktopBounds,
    std::shared_ptr<std::vector<Vertex>> vertexBuffer,
    std::shared_ptr<ShaderCache> shaderCache,
    ID3D11Texture2D* sharedSurfacePtr,
    winrt::com_ptr<ID3D11RenderTargetView> renderTargetView)
    : mFrame{ frame }
    , mVirtualDesktopBounds{ virtualDesktopBounds }
    , mVertexBuffer{ vertexBuffer }
    , mShaderCache{ shaderCache }
    , mSharedSurfacePtr{ sharedSurfacePtr }
    , mRenderTargetView{ renderTargetView }
{
    if (mFrame == nullptr)
    {
        throw std::exception("Null frame");
    }

    if (mVertexBuffer == nullptr)
    {
        throw std::exception("null vertex buffer");
    }

    if (mShaderCache == nullptr)
    {
        throw std::exception("null shader cache");
    }

    winrt::check_pointer(mSharedSurfacePtr);
    winrt::check_pointer(mRenderTargetView.get());
}

RenderDirtyRectsStep::~RenderDirtyRectsStep()
{
}

void RenderDirtyRectsStep::Perform()
{
    if (mFrame->DirtyRectsCount() == 0) {
        return;
    }

    UpdateDirtyRects();

    RenderDirtyRects();
}

void RenderDirtyRectsStep::UpdateDirtyRects()
{
    // create dirty vertex buffer
    mVertexBuffer->resize(mFrame->DirtyRectsCount() * g_VerticesPerRect);

    D3D11_TEXTURE2D_DESC sharedSurfaceDesc;
    mSharedSurfacePtr->GetDesc(&sharedSurfaceDesc);

    D3D11_TEXTURE2D_DESC desktopImageDesc;
    mFrame->DesktopImage()->GetDesc(&desktopImageDesc);

    // be careful with the types of these integers - they should be signed ints
    // or else the below vertices position calculations will overflow
    const LONG centerX = (LONG)sharedSurfaceDesc.Width / 2;
    const LONG centerY = (LONG)sharedSurfaceDesc.Height / 2;
    const LONG offsetX = mVirtualDesktopBounds.left;
    const LONG offsetY = mVirtualDesktopBounds.top;

    const RECT desktopBounds = mFrame->DesktopMonitorBounds();
    const LONG desktopWidth = desktopBounds.right - desktopBounds.left;
    const LONG desktopHeight = desktopBounds.bottom - desktopBounds.top;

    RECT* dirtyRects = mFrame->DirtyRects();

    for (size_t i = 0; i < mFrame->DirtyRectsCount(); ++i) {
    /*
        Identity and unspecified:
        2                4
        +---------------+
        |               |
        |               |
        +---------------+
        1               3

        90 CCW:
        2        4
        +--------+
        |        |
        |        |
        |        |
        |        |
        +--------+
        1        3


        180 CCW:
        4               1
        +---------------+
        |               |
        |               |
        +---------------+
        2               3

        270 CCW:
        1        3
        +--------+
        |        |
        |        |
        |        |
        |        |
        +--------+
        4        2

    */
        RECT dirtyRect = dirtyRects[i];
        RECT rotatedRect = dirtyRects[i];

        switch (mFrame->Rotation())
        {
        case DXGI_MODE_ROTATION_ROTATE90:
            rotatedRect.left = desktopWidth - dirtyRect.bottom;
            rotatedRect.top = dirtyRect.left;
            rotatedRect.right = desktopWidth - dirtyRect.top;
            rotatedRect.bottom = dirtyRect.right;
            break;

        case DXGI_MODE_ROTATION_ROTATE180:
            rotatedRect.left = desktopWidth - dirtyRect.right;
            rotatedRect.top = desktopHeight - dirtyRect.bottom;
            rotatedRect.right = desktopWidth - dirtyRect.left;
            rotatedRect.bottom = desktopHeight - dirtyRect.top;
            break;

        case DXGI_MODE_ROTATION_ROTATE270:
            rotatedRect.left = dirtyRect.top;
            rotatedRect.top = desktopHeight - dirtyRect.right;
            rotatedRect.right = dirtyRect.bottom;
            rotatedRect.bottom = desktopHeight - dirtyRect.left;
            break;

        case DXGI_MODE_ROTATION_IDENTITY:
        case DXGI_MODE_ROTATION_UNSPECIFIED:
        default:
            break;
        }

        TexCoord bottomLeft = { dirtyRect.left / static_cast<FLOAT>(desktopImageDesc.Width), dirtyRect.bottom / static_cast<FLOAT>(desktopImageDesc.Height) };
        TexCoord topLeft = { dirtyRect.left / static_cast<FLOAT>(desktopImageDesc.Width), dirtyRect.top / static_cast<FLOAT>(desktopImageDesc.Height) };
        TexCoord bottomRight = { dirtyRect.right / static_cast<FLOAT>(desktopImageDesc.Width), dirtyRect.bottom / static_cast<FLOAT>(desktopImageDesc.Height) };
        TexCoord topRight = { dirtyRect.right / static_cast<FLOAT>(desktopImageDesc.Width), dirtyRect.top / static_cast<FLOAT>(desktopImageDesc.Height) };

        // set vertex buffer at [i]
        auto vertices = mVertexBuffer->data() + i * g_VerticesPerRect;

        switch (mFrame->Rotation()) {
        case DXGI_MODE_ROTATION_IDENTITY:
        case DXGI_MODE_ROTATION_UNSPECIFIED:

            vertices[0].texCoord = bottomLeft;
            vertices[1].texCoord = topLeft;
            vertices[2].texCoord = bottomRight;
            vertices[3].texCoord = vertices[2].texCoord;
            vertices[4].texCoord = vertices[1].texCoord;
            vertices[5].texCoord = topRight;
            break;

        case DXGI_MODE_ROTATION_ROTATE90:
            vertices[0].texCoord = bottomRight;
            vertices[1].texCoord = bottomLeft;
            vertices[2].texCoord = topRight;
            vertices[3].texCoord = vertices[2].texCoord;
            vertices[4].texCoord = vertices[1].texCoord;
            vertices[5].texCoord = topLeft;
            break;

        case DXGI_MODE_ROTATION_ROTATE180:
            vertices[0].texCoord = topRight;
            vertices[1].texCoord = bottomRight;
            vertices[2].texCoord = topLeft;
            vertices[3].texCoord = vertices[2].texCoord;
            vertices[4].texCoord = vertices[1].texCoord;
            vertices[5].texCoord = bottomLeft;
            break;

        case DXGI_MODE_ROTATION_ROTATE270:
            vertices[0].texCoord = topLeft;
            vertices[1].texCoord = topRight;
            vertices[2].texCoord = bottomLeft;
            vertices[3].texCoord = vertices[2].texCoord;
            vertices[4].texCoord = vertices[1].texCoord;
            vertices[5].texCoord = bottomRight;
            break;

        default:
            throw std::exception("render dirty rects with unimplemented rotation");
        }

        // Set the vertices of the two triangles that make up the dirty rectangle
        // The second triangle shares a side with the first triangle
        // vertices proceed clockwise

        // bottomLeft -> topLeft -> bottomRight
        vertices[0].pos = { (rotatedRect.left + desktopBounds.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
            -1 * (rotatedRect.bottom + desktopBounds.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
            0.0f };
        vertices[1].pos = { (rotatedRect.left + desktopBounds.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
            -1 * (rotatedRect.top + desktopBounds.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
            0.0f };
        vertices[2].pos = { (rotatedRect.right + desktopBounds.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
            -1 * (rotatedRect.bottom + desktopBounds.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
            0.0f };

        // bottomRight -> topLeft -> topRight
        vertices[3].pos = vertices[2].pos;
        vertices[4].pos = vertices[1].pos;
        vertices[5].pos = { (rotatedRect.right + desktopBounds.left - offsetX - centerX) / static_cast<FLOAT>(centerX),
            -1 * (rotatedRect.top + desktopBounds.top - offsetY - centerY) / static_cast<FLOAT>(centerY),
            0.0f };
    }
}

void RenderDirtyRectsStep::RenderDirtyRects()
{
    winrt::com_ptr<ID3D11Device> device;
    mSharedSurfacePtr->GetDevice(device.put());

    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());

    D3D11_TEXTURE2D_DESC srcTextureDesc;
    mFrame->DesktopImage()->GetDesc(&srcTextureDesc);

    // setup vertex buffer, vertex shader view, etc
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = srcTextureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = srcTextureDesc.MipLevels - 1;
    srvDesc.Texture2D.MipLevels = srcTextureDesc.MipLevels;
    winrt::com_ptr<ID3D11ShaderResourceView> srView;
    winrt::check_hresult(device->CreateShaderResourceView(
        mFrame->DesktopImage().get(),
        &srvDesc,
        srView.put()
    ));
    auto srTemp = srView.get();
    ID3D11ShaderResourceView** srvPtr = &srTemp;

    auto rtv = mRenderTargetView.get();
    ID3D11RenderTargetView** rtvPtr = &rtv;

    auto sampler = mShaderCache->LinearSampler().get();
    auto samplerPtr = &sampler;

    // set device context properties
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context->OMSetRenderTargets(1, rtvPtr, nullptr);
    context->VSSetShader(mShaderCache->VertexShader().get(), nullptr, 0);
    context->PSSetShader(mShaderCache->PixelShader().get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, srvPtr);
    context->PSSetSamplers(0, 1, samplerPtr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = static_cast<UINT>(mVertexBuffer->size() * sizeof(Vertex));
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA bufferData;
    bufferData.pSysMem = reinterpret_cast<const void*>(mVertexBuffer->data());
    bufferData.SysMemPitch = 0;
    bufferData.SysMemSlicePitch = 0;

    winrt::com_ptr<ID3D11Buffer> buffer;
    winrt::check_hresult(device->CreateBuffer(&bufferDesc, &bufferData, buffer.put()));

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    ID3D11Buffer* buf = buffer.get();
    ID3D11Buffer** bufAddr = &buf;
    context->IASetVertexBuffers(0, 1, bufAddr, &stride, &offset);

    D3D11_TEXTURE2D_DESC sharedSurfaceDesc;
    mSharedSurfacePtr->GetDesc(&sharedSurfaceDesc);

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(sharedSurfaceDesc.Width);
    VP.Height = static_cast<FLOAT>(sharedSurfaceDesc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    context->RSSetViewports(1, &VP);

    context->Draw(static_cast<UINT>(mVertexBuffer->size()), 0);
}
