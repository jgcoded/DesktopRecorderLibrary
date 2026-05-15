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

Texture2D tx : register(t0);
SamplerState samLinear : register(s0);

// Source-color-space conversion. Set by the pipeline when the desktop is
// in HDR mode so the encoded MP4 is correctly sRGB. Keep in sync with the
// ColorSpaceCBData struct on the C++ side.
//
// conversionMode values:
//   0 = no conversion (source is already sRGB; pointer step always uses this)
//   1 = scRGB linear Rec.709 -> sRGB gamma Rec.709 (Win10 HDR default)
//   2 = HDR10 PQ Rec.2020 -> sRGB gamma Rec.709 (Dolby Vision capable displays)
cbuffer ColorSpaceCB : register(b0)
{
    uint conversionMode;
    float sdrWhiteNits;
    float pad0;
    float pad1;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

// SMPTE ST.2084 (PQ) EOTF: PQ code [0,1] -> absolute luminance in nits
// (peak 10,000).
float3 PQToLumaNits(float3 pq)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float3 p = pow(max(pq, 0.0), 1.0 / m2);
    float3 num = max(p - c1, 0.0);
    float3 den = max(c2 - c3 * p, 1e-6);
    return 10000.0 * pow(num / den, 1.0 / m1);
}

// Rec.2020 -> Rec.709 gamut matrix (linear-light), D65 white point.
float3 Rec2020ToRec709(float3 c)
{
    return float3(
         1.66049 * c.r - 0.58764 * c.g - 0.07285 * c.b,
        -0.12455 * c.r + 1.13289 * c.g - 0.00833 * c.b,
        -0.01815 * c.r - 0.10059 * c.g + 1.11874 * c.b
    );
}

// Linear -> sRGB piecewise gamma encoding.
float3 LinearToSrgb(float3 c)
{
    c = max(c, 0.0);
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}

// Lightweight Reinhard tone-map. Compresses HDR highlights toward 1.0
// while preserving most of the SDR range. Not perceptually ideal but
// cheap and self-stable.
float3 ReinhardToneMap(float3 luma)
{
    return luma / (luma + 1.0);
}

float4 main(PS_INPUT input) : SV_Target
{
    float4 c = tx.Sample(samLinear, input.Tex);

    if (conversionMode == 1)
    {
        // scRGB linear Rec.709. By definition (1.0,1.0,1.0) = 80 nits =
        // SDR white. With Windows HDR enabled, the user's "SDR content
        // brightness" slider scales SDR rendering up by (sdrWhiteNits/80),
        // so we have to scale back DOWN by that factor or else SDR white
        // ends up as a scRGB value well above 1.0 and gets clamped to
        // sRGB white at maximum brightness (washed out / over-bright).
        // Highlights legitimately above the user's SDR-white level are
        // hard-clipped after normalization — fine for a screen recorder.
        float scale = 80.0 / max(sdrWhiteNits, 80.0);
        float3 normalized = saturate(max(c.rgb, 0.0) * scale);
        c.rgb = LinearToSrgb(normalized);
    }
    else if (conversionMode == 2)
    {
        // HDR10: PQ-encoded Rec.2020. Decode PQ to absolute luminance,
        // normalize against the configured SDR-white level, tone-map,
        // re-gamut to Rec.709, then sRGB gamma.
        float3 nits = PQToLumaNits(c.rgb);
        float3 scaled = nits / max(sdrWhiteNits, 1.0);
        float3 mapped = ReinhardToneMap(scaled);
        float3 rec709 = Rec2020ToRec709(mapped);
        c.rgb = LinearToSrgb(saturate(rec709));
    }
    // else: conversionMode == 0, source is already sRGB, passthrough.

    return c;
}
