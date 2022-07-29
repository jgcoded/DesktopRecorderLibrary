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
#include "DesktopMonitor.h"
#include "RecordingStep.h"
#include "CaptureFrameStep.h"
#include "VirtualDesktop.h"
#include "TexturePool.h"

winrt::com_ptr<TexturePool> TexturePool::CreateFromFrame(DesktopMonitor::ScreenDuplicator::Frame& frame)
{
    winrt::com_ptr<ID3D11Device> device;
    frame.DesktopImage()->GetDevice(device.put());
    D3D11_TEXTURE2D_DESC desc;
    frame.DesktopImage()->GetDesc(&desc);

    winrt::com_ptr<TexturePool> pool;
    pool.attach(new TexturePool{ device, desc });

    return pool;
}

TexturePool::TexturePool(winrt::com_ptr<ID3D11Device> device, const D3D11_TEXTURE2D_DESC desc)
    : mDevice{ device }
    , mTextureDesc{ desc }
{
}

winrt::com_ptr<ID3D11Texture2D> TexturePool::Acquire()
{
    std::lock_guard<std::mutex> lock{ mMutex };
    if (mTexturePool.empty()) {
        return CreateTexture();
    }

    auto texture = mTexturePool.front();
    mTexturePool.pop();
    return texture;
}

HRESULT __stdcall TexturePool::GetParameters(DWORD * pdwFlags, DWORD * pdwQueue)
{
    UNREFERENCED_PARAMETER(pdwFlags);
    UNREFERENCED_PARAMETER(pdwQueue);
    return E_NOTIMPL;
}

HRESULT __stdcall TexturePool::Invoke(IMFAsyncResult * pAsyncResult)
{
    winrt::com_ptr<IUnknown> unknown;
    winrt::check_hresult(pAsyncResult->GetObjectW(unknown.put()));

    auto sample = unknown.as<IMFSample>();

    winrt::com_ptr<IMFMediaBuffer> mediaBuffer;
    winrt::check_hresult(sample->GetBufferByIndex(0, mediaBuffer.put()));

    auto dxgiBuffer = mediaBuffer.as<IMFDXGIBuffer>();

    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(dxgiBuffer->GetResource(IID_PPV_ARGS(texture.put())));

    {
        std::lock_guard<std::mutex> lock{ mMutex };
        mTexturePool.push(texture);
    }

    return S_OK;
}

winrt::com_ptr<ID3D11Texture2D> TexturePool::CreateTexture()
{
    D3D11_TEXTURE2D_DESC moveDesc = mTextureDesc;
    moveDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    moveDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    winrt::com_ptr<ID3D11Texture2D> texture;
    mDevice->CreateTexture2D(&moveDesc, nullptr, texture.put());
    return texture;
}

HRESULT TexturePool::QueryInterface(REFIID riid, void** ppv) noexcept
{
    static const QITAB qit[] =
    {
        QITABENT(TexturePool, IMFAsyncCallback),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}