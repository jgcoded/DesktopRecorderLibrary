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

//
// pch.h
// Precompiled header for commonly included header files
//

#pragma once

#define NOMINMAX // Don't let windows headers define min and max macros

#include <Shlwapi.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <Mferror.h>
#include <mfapi.h>
#include <mfidl.h>
#include <codecapi.h>
#include <mfreadwrite.h>
#include <evr.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <functional>
#include <mutex>

#include "VertexShader.h"
#include "PixelShader.h"
#include "Vertex.h"
