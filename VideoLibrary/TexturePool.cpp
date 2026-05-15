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
#include "CaptureFrameStep.h"
#include "VirtualDesktop.h"
#include "TexturePool.h"

TexturePool::TexturePool(winrt::com_ptr<ID3D11Device> device, const D3D11_TEXTURE2D_DESC desc)
    : mDevice{ device }
    , mTextureDesc{ desc }
    , m_refCount{ 1 }
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

winrt::com_ptr<ID3D11RenderTargetView> TexturePool::RtvFor(ID3D11Texture2D* texture)
{
    winrt::check_pointer(texture);
    std::lock_guard<std::mutex> lock{ mMutex };
    auto it = mRtvCache.find(texture);
    if (it != mRtvCache.end())
    {
        return it->second;
    }
    winrt::com_ptr<ID3D11RenderTargetView> rtv;
    winrt::check_hresult(mDevice->CreateRenderTargetView(texture, nullptr, rtv.put()));
    // The RTV refcounts the texture, so this cache also pins the
    // textures it indexes — which is fine because the pool is meant to
    // own its textures for the life of the recording.
    mRtvCache.emplace(texture, rtv);
    return rtv;
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
    // Check the HRESULT — otherwise a failed allocation hands back a
    // null com_ptr and downstream check_pointer calls only catch it
    // some of the time (RenderPointerTextureStep uses virtualDesktopCopy
    // immediately without a null check).
    winrt::check_hresult(mDevice->CreateTexture2D(&moveDesc, nullptr, texture.put()));
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

TexturePool::~TexturePool()
{
    assert(m_refCount == 0);
}
