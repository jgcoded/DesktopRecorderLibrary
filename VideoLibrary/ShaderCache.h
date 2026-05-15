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

// CPU-side mirror of the cbuffer in PixelShader.hlsl. Must stay packed
// to 16 bytes (HLSL cbuffer alignment).
struct ColorSpaceCBData
{
    enum Mode : uint32_t
    {
        None = 0,           // source is already sRGB; passthrough
        ScRgbLinearToSrgb = 1,
        Hdr10PqToSrgb = 2,
    };

    uint32_t conversionMode;
    float sdrWhiteNits;
    float pad0;
    float pad1;
};

class ShaderCache
{
public:

    ShaderCache(winrt::com_ptr<ID3D11Device> device);

    virtual ~ShaderCache();

    winrt::com_ptr<ID3D11VertexShader> VertexShader();

    winrt::com_ptr<ID3D11InputLayout> VertexShaderInputLayout();

    winrt::com_ptr<ID3D11PixelShader> PixelShader();

    winrt::com_ptr<ID3D11SamplerState> LinearSampler();

    winrt::com_ptr<ID3D11BlendState> BlendState();

    // Write the conversion params into the PS cbuffer and bind it to
    // slot b0. Call this before every Draw that uses the pixel shader
    // (one call per step is fine — the GPU cost is negligible).
    void BindColorSpaceConversion(ID3D11DeviceContext* context, ColorSpaceCBData const& data);

private:

    void Initialize(winrt::com_ptr<ID3D11Device> device);

    winrt::com_ptr<ID3D11VertexShader> mVertexShader;
    winrt::com_ptr<ID3D11InputLayout> mVertexShaderInputLayout;
    winrt::com_ptr<ID3D11PixelShader> mPixelShader;
    winrt::com_ptr<ID3D11SamplerState> mLinearSampler;
    winrt::com_ptr<ID3D11BlendState> mBlendState;
    winrt::com_ptr<ID3D11Buffer> mColorSpaceCB;
};
