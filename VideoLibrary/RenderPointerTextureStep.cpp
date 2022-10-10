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
#include "Errors.h"
#include "RenderPointerTextureStep.h"
#include "ShaderCache.h"
#include "TextureToMediaSampleStep.h"
#include "TexturePool.h"
#include "Vertex.h"

RenderPointerTextureStep::RenderPointerTextureStep(
    std::shared_ptr<DesktopPointer> desktopPointer,
    std::shared_ptr<SharedSurface> sharedSurface,
    winrt::com_ptr<ID3D11Device> device,
    std::shared_ptr<ShaderCache> shaderCache,
    winrt::com_ptr<TexturePool> texturePool,
    RECT virtualDesktopBounds,
    RECT desktopMonitorBounds)
    : mDevice{ device }
    , mSharedSurface{ sharedSurface }
    , mDesktopPointer{ desktopPointer }
    , mShaderCache{ shaderCache }
    , mTexturePool{ texturePool }
    , mVirtualDesktopBounds{ virtualDesktopBounds }
    , mDesktopMonitorBounds{ desktopMonitorBounds }
    , mResult{ nullptr }
{
    if (mDesktopPointer == nullptr)
    {
        throw std::exception("Desktop pointer null");
    }

    if (mShaderCache == nullptr)
    {
        throw std::exception("render pointer shader cache is null");
    }

    winrt::check_pointer(mDevice.get());
    winrt::check_pointer(mTexturePool.get());
    winrt::check_pointer(mSharedSurface.get());
}

RenderPointerTextureStep::~RenderPointerTextureStep()
{
}

void RenderPointerTextureStep::Perform()
{

    // todo in multi monitor scenario see if the pointer is visible for currently drawn monitor
    auto lock = mSharedSurface->Lock();
    if (!lock->Locked())
    {
        return;
    }
    mSharedSurfacePtr = lock->TexturePtr();
    // copy shared surface
    winrt::com_ptr<ID3D11DeviceContext> context;
    mDevice->GetImmediateContext(context.put());

    winrt::com_ptr<ID3D11Texture2D> virtualDesktopCopy = mTexturePool->Acquire();

    context->CopyResource(virtualDesktopCopy.get(), lock->TexturePtr());

    if (!mDesktopPointer->Visible())
    {
        mResult = virtualDesktopCopy;
        return;
    }
    
    POINT pos = mDesktopPointer->Position().Position;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shape = mDesktopPointer->ShapeInfo();

    // render pointer to copy
    D3D11_TEXTURE2D_DESC desc;
    virtualDesktopCopy->GetDesc(&desc);

    if (pos.x < 0) {
        shape.Width += pos.x;
        pos.x = 0;
    }
    else if (pos.x + shape.Width > desc.Width) {
        shape.Width = desc.Width - pos.x;
    }
    if (shape.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        shape.Height /= 2;
    }
    if (pos.y < 0) {
        shape.Height += pos.y;
        pos.y = 0;
    }
    else if (pos.y + shape.Height > desc.Height) {
        shape.Height = desc.Height - pos.y;
    }
    float centerX = (float)desc.Width / 2;
    float centerY = (float)desc.Height / 2;
    float left = ((float)pos.x - centerX) / centerX;
    float right = ((float)pos.x + (float)shape.Width - centerX) / centerX;
    float top = -1.0f*((float)pos.y - centerY) / centerY;
    float bottom = -1.0f*((float)pos.y + (float)shape.Height - centerY) / centerY;

    // Vertices for drawing whole texture
    // vertex coords are clock wise per triangle, texture coords are ccw
    const std::vector<Vertex> vertices = {
        { { left, bottom, 0 },{ 0.0f, 1.0f } },
        { { left, top, 0 },{ 0.0f, 0.0f } },
        { { right, bottom, 0 },{ 1.0f, 1.0f } },
        { { right, bottom, 0 },{ 1.0f, 1.0f } },
        { { left, top, 0 },{ 0.0f, 0.0f } },
        { { right, top, 0 },{ 1.0f, 0.0f } },
    };

    winrt::com_ptr<ID3D11Texture2D> mouseTexture = this->MakePointerTexture();

    // Set shader resource properties
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = desc.MipLevels - 1;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    winrt::com_ptr<ID3D11ShaderResourceView> srView;
    winrt::check_hresult(mDevice->CreateShaderResourceView(
        mouseTexture.get(),
        &srvDesc,
        srView.put()
    ));
    auto srv = srView.get();
    ID3D11ShaderResourceView** srvPtr = &srv;

    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(Vertex) * g_VerticesPerRect;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA bufferData;
    bufferData.pSysMem = reinterpret_cast<const void*>(vertices.data());
    bufferData.SysMemPitch = 0;
    bufferData.SysMemSlicePitch = 0;
    // Create vertex buffer
    winrt::com_ptr<ID3D11Buffer> mouseVertexBuffer;
    winrt::check_hresult(mDevice->CreateBuffer(
        &bufferDesc,
        &bufferData,
        mouseVertexBuffer.put()
    ));

    // TODO why create RTV per texture?? for now just do it but
    // need to store RTV per texture in TexturePool

    winrt::com_ptr<ID3D11RenderTargetView> rtv;
    winrt::check_hresult(mDevice->CreateRenderTargetView(
        virtualDesktopCopy.get(),
        nullptr,
        rtv.put()
    ));
    auto render = rtv.get();
    ID3D11RenderTargetView** rtvAddr = &render;

    auto sampler = mShaderCache->LinearSampler().get();
    ID3D11SamplerState** samplerPtr = &sampler;

    // Set resources
    FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    auto buf = mouseVertexBuffer.get();
    auto bufAddr = &buf;
    context->IASetVertexBuffers(0, 1, bufAddr, &stride, &offset);
    context->OMSetBlendState(mShaderCache->BlendState().get(), BlendFactor, 0xFFFFFFFF);
    context->OMSetRenderTargets(1, rtvAddr, nullptr);
    context->VSSetShader(mShaderCache->VertexShader().get(), nullptr, 0);
    context->PSSetShader(mShaderCache->PixelShader().get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, srvPtr);
    context->PSSetSamplers(0, 1, samplerPtr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_VIEWPORT VP;
    VP.Width = static_cast<FLOAT>(desc.Width);
    VP.Height = static_cast<FLOAT>(desc.Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    context->RSSetViewports(1, &VP);

    // Draw
    context->Draw(g_VerticesPerRect, 0);

    mResult = virtualDesktopCopy;
}

winrt::com_ptr<ID3D11Texture2D> RenderPointerTextureStep::Result()
{
    return mResult;
}

winrt::com_ptr<ID3D11Texture2D> RenderPointerTextureStep::MakePointerTexture()
{
    auto shapeInfo = mDesktopPointer->ShapeInfo();

    switch (shapeInfo.Type) {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        return MakeColorPointerTexture();
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        return MakeMaskedPointerTexture();
    default:
        break;
    }

    return nullptr;
}

winrt::com_ptr<ID3D11Texture2D> RenderPointerTextureStep::MakeColorPointerTexture()
{
    auto shapeInfo = mDesktopPointer->ShapeInfo();
    return MakeColorPointer(mDesktopPointer->PutBuffer(), shapeInfo.Width, shapeInfo.Height);
}

winrt::com_ptr<ID3D11Texture2D> RenderPointerTextureStep::MakeMaskedPointerTexture()
{
    winrt::com_ptr<ID3D11Device> device = mDevice;

    // TODO in multi monitor recording, will need to FIRST merge all desktop images and then draw the mouse
    auto pointerPos = mDesktopPointer->Position().Position;
    auto shapeInfo = mDesktopPointer->ShapeInfo();

    const auto& desc = mSharedSurface->Desc();

    int left = pointerPos.x;
    int top = pointerPos.y;

    int width = static_cast<int>(shapeInfo.Width);
    int height = static_cast<int>(shapeInfo.Height);

    UINT maskX = 0, maskY = 0;

    if (left < 0) {
        left = 0;
        maskX = -left;
        width += left;
    }
    else if (left + width >(int)desc.Width) {
        width = desc.Width - left;
    }

    if (shapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        height /= 2;
    }

    if (top < 0) {
        top = 0;
        maskY = -top;
        height += top;
    }
    else if (top + height >(int)desc.Height) {
        height = desc.Height - top;
    }

    // create a new texture to copy the current desktop image under the cursor
    D3D11_TEXTURE2D_DESC newDesc;
    newDesc.Format = desc.Format;
    newDesc.Width = width;
    newDesc.Height = height;
    newDesc.BindFlags = 0;
    newDesc.Usage = D3D11_USAGE_STAGING;
    newDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    newDesc.MiscFlags = 0;
    newDesc.ArraySize = desc.ArraySize;
    newDesc.SampleDesc = desc.SampleDesc;
    newDesc.MipLevels = desc.MipLevels;

    winrt::com_ptr<ID3D11Texture2D> desktopCopy;
    winrt::check_hresult(device->CreateTexture2D(&newDesc, nullptr, desktopCopy.put()));

    D3D11_BOX box;
    box.left = left;
    box.right = left + width;
    box.top = top;
    box.bottom = top + height;
    box.front = 0;
    box.back = 1;

    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    context->CopySubresourceRegion(
        desktopCopy.get(),
        0, 0, 0, 0,
        mSharedSurfacePtr, 0, &box);

    auto desktopSurface = desktopCopy.as<IDXGISurface>();
    std::vector<UINT> dest;
    dest.assign(width * height, 0);

    DXGI_MAPPED_RECT mapped;
    winrt::check_hresult(desktopSurface->Map(&mapped, DXGI_MAP_READ));

    auto textureData = reinterpret_cast<UINT*>(mapped.pBits);
    if (textureData == nullptr) {
        return nullptr;
    }

    auto maskData = mDesktopPointer->PutBuffer();

    if (shapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        MakeMonochromePointerBuffer(dest.data(), textureData, mapped.Pitch, maskData, shapeInfo.Pitch, width, height, maskX, maskY, shapeInfo.Height / 2);
    } else if (shapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) {
        MakeMaskedColorPointerBuffer(dest.data(), textureData, mapped.Pitch, (UINT*)maskData, shapeInfo.Pitch, width, height, maskX, maskY);
    }

    winrt::check_hresult(desktopSurface->Unmap());

    return MakeColorPointer((BYTE*)dest.data(), width, height);
}

void RenderPointerTextureStep::MakeMonochromePointerBuffer(UINT * dest, const UINT * colorData, UINT colorPitch, byte * maskData, UINT maskPitch, UINT width, UINT height, UINT maskX, UINT maskY, UINT maskHeight)
{
    /*
    The pointer type is a monochrome mouse pointer, which is a monochrome bitmap.
    The bitmap's size is specified by width and height in a
    1 bits per pixel (bpp) device independent bitmap (DIB) format AND maskBit that is
    followed by another 1 bpp DIB format XOR maskBit of the same size.

    if is mono must divide height by 2 to get to the xor maskBit
    */
    for (UINT j = 0; j < height; ++j) {

        BYTE maskBit = 0x80 >> (maskX % 8);
        for (UINT i = 0; i < width; ++i) {
            // maskData contains two 1bpp images
            auto dstIndex = j * width + i;
            auto colorIndex = j * colorPitch / sizeof(UINT) + i;

            auto maskAndIndex = ((maskY + j) * maskPitch) + ((maskX + i) / 8);
            auto maskXorIndex = ((maskY + j + maskHeight) * maskPitch) + ((maskX + i) / 8);

            auto andMaskBit = maskData[maskAndIndex] & maskBit;
            auto xorMaskBit = maskData[maskXorIndex] & maskBit;

            auto andMask = andMaskBit ? 0xFFFFFFFF : 0xFF000000;
            auto xorMask = xorMaskBit ? 0x00FFFFFF : 0x00000000;

            auto color = colorData[colorIndex];

            dest[dstIndex] = (color & andMask) ^ xorMask;

            if (maskBit == 1) {
                maskBit = 0x80;
            }
            else {
                maskBit = maskBit >> 1;
            }
        }
    }
}

void RenderPointerTextureStep::MakeMaskedColorPointerBuffer(UINT * dest, const UINT * colorData, UINT colorPitch, UINT * maskData, UINT maskPitch, UINT width, UINT height, UINT maskX, UINT maskY)
{
    /*
    The pointer type is a masked color mouse pointer.
    A masked color mouse pointer is a 32 bpp ARGB format bitmap with the maskBit value in the alpha bits.
    The only allowed maskBit values are 0 and 0xFF. When the maskBit value is 0,
    the RGB value should replace the screen pixel. When the maskBit value is 0xFF,
    an XOR operation is performed on the RGB value and the screen pixel;
    the result replaces the screen pixel.

    */
    for (UINT j = 0; j < height; ++j) {
        for (UINT i = 0; i < width; ++i) {

            auto destIndex = (j*width) + i;
            auto colorIndex = (j*colorPitch / sizeof(UINT)) + i;
            auto maskIndex = ((j + maskY)*maskPitch / sizeof(UINT)) + (i + maskX);

            auto color = colorData[colorIndex];
            auto mask = maskData[maskIndex];

            if (mask & 0xFF000000) {
                auto newColor = 0xFF000000 | (color ^ mask);
                dest[destIndex] = newColor;
            }
            else {
                dest[destIndex] = 0xFF000000 | mask;
            }
        }
    }
}

winrt::com_ptr<ID3D11Texture2D> RenderPointerTextureStep::MakeColorPointer(byte * data, int width, int height)
{
    winrt::com_ptr<ID3D11Device> device = mDevice;

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.CPUAccessFlags = 0;
    desc.SampleDesc.Quality = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

    D3D11_SUBRESOURCE_DATA initialData;
    initialData.pSysMem = (void*)data;
    initialData.SysMemPitch = width * 4;
    initialData.SysMemSlicePitch = 0;

    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(device->CreateTexture2D(&desc, &initialData, texture.put()));
    return texture;
}
