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

#include "DesktopMonitor.h"

class TexturePool : public IMFAsyncCallback
{
public:

    TexturePool(winrt::com_ptr<ID3D11Device> device, D3D11_TEXTURE2D_DESC desc);

    winrt::com_ptr<ID3D11Texture2D> Acquire();

    virtual HRESULT STDMETHODCALLTYPE GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) override;

    virtual HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* pAsyncResult) override;

    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release()
    {
        assert(m_refCount > 0);
        ULONG uCount = InterlockedDecrement(&m_refCount);
        if (uCount == 0)
        {
            delete this;
        }
        return uCount;
    }
    virtual HRESULT QueryInterface(REFIID riid, void** ppv) noexcept override;

    virtual ~TexturePool();

private:

    winrt::com_ptr<ID3D11Texture2D> CreateTexture();

    winrt::com_ptr<ID3D11Device> mDevice;
    const D3D11_TEXTURE2D_DESC mTextureDesc;
    std::queue<winrt::com_ptr<ID3D11Texture2D>> mTexturePool;
    std::mutex mMutex;
    volatile long   m_refCount;

};
