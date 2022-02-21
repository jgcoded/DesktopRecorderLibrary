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

#include <winrt/Windows.Media.MediaProperties.h>
#include <string>

using ResolutionOption = winrt::Windows::Media::MediaProperties::VideoEncodingQuality;
using AudioQuality = winrt::Windows::Media::MediaProperties::AudioEncodingQuality;

struct EncodingContext
{
    std::wstring fileName;
    ResolutionOption resolutionOption;
    AudioQuality audioQuality;
    int frameRate;
    int bitRate;
    winrt::com_ptr<IMFMediaType> videoInputMediaType;
    winrt::com_ptr<IMFMediaType> audioInputMediaType;
    winrt::com_ptr<ID3D11Device> device;
};
