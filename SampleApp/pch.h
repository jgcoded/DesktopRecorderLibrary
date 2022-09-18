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

#include <iostream>
#include <queue>
#include <atomic>
#include <mutex>
#include <functional>
#include <sstream>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <Mferror.h>
#include <mfapi.h>
#include <mfidl.h>
#include <codecapi.h>
#include <mfreadwrite.h>
#include <evr.h>
#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.h>

#include "VideoLibrary\VirtualDesktop.h"
#include "VideoLibrary\Pipeline.h"
#include "VideoLibrary\AsyncMediaSourceReader.h"
#include "VideoLibrary\ScreenMediaSinkWriter.h"
#include "VideoLibrary\AudioMedia.h"
#include "VideoLibrary\Errors.h"

#include "WindowFactory.h"
