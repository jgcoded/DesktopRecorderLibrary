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
#include "ShaderCache.h"
// pch.h MUST INCLUDE THE PIXEL SHADER AND VERTEX SHADER

void ShaderCache::Initialize(winrt::com_ptr<ID3D11Device> device)
{
    // create vertex shader
    const auto vertexShaderArraySize = ARRAYSIZE(g_VertexShaderRawData);
    winrt::check_hresult(device->CreateVertexShader(
        g_VertexShaderRawData,
        vertexShaderArraySize,
        nullptr,
        mVertexShader.put())
    );

    // Create the input layout for the vertex shader
    // Name                 Index   Mask Register SysValue  Format   Used
    // -------------------- ----- ------ -------- -------- ------- ------
    // POSITION                 0   xyzw        0     NONE   float   xyzw
    // TEXCOORD                 0   xy          1     NONE   float   xy  
    //
    const std::vector<D3D11_INPUT_ELEMENT_DESC> inputDesc = {
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    winrt::com_ptr<ID3D11InputLayout> inputLayout;
    winrt::check_hresult(device->CreateInputLayout(
        inputDesc.data(),
        static_cast<UINT>(inputDesc.size()),
        g_VertexShaderRawData,
        vertexShaderArraySize,
        mVertexShaderInputLayout.put()
    ));
    winrt::com_ptr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.put());
    context->IASetInputLayout(mVertexShaderInputLayout.get());

    // create pixel shader
    auto pixelShaderArraySize = ARRAYSIZE(g_PixelShaderRawData);
    winrt::check_hresult(device->CreatePixelShader(
        g_PixelShaderRawData,
        pixelShaderArraySize,
        nullptr,
        mPixelShader.put())
    );

    // Set up sampler
    D3D11_SAMPLER_DESC SampDesc;
    RtlZeroMemory(&SampDesc, sizeof(SampDesc));
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampDesc.MinLOD = 0;
    SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    winrt::check_hresult(device->CreateSamplerState(&SampDesc, mLinearSampler.put()));

    // Blend state
    // Create the blend state
    D3D11_BLEND_DESC BlendStateDesc;
    BlendStateDesc.AlphaToCoverageEnable = FALSE;
    BlendStateDesc.IndependentBlendEnable = FALSE;
    BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    winrt::check_hresult(device->CreateBlendState(&BlendStateDesc, mBlendState.put()));
}

ShaderCache::ShaderCache(winrt::com_ptr<ID3D11Device> device)
{
    Initialize(device);
}

ShaderCache::~ShaderCache()
{
}

winrt::com_ptr<ID3D11VertexShader> ShaderCache::VertexShader()
{
    return mVertexShader;
}

winrt::com_ptr<ID3D11InputLayout> ShaderCache::VertexShaderInputLayout()
{
    return mVertexShaderInputLayout;
}

winrt::com_ptr<ID3D11PixelShader> ShaderCache::PixelShader()
{
    return mPixelShader;
}

winrt::com_ptr<ID3D11SamplerState> ShaderCache::LinearSampler()
{
    return mLinearSampler;
}

winrt::com_ptr<ID3D11BlendState> ShaderCache::BlendState()
{
    return mBlendState;
}
