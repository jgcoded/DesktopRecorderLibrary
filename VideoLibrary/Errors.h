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

#include <dxgi.h>

// The below is from the DesktopDuplication sample from Microsoft in the DesktopDuplication.cpp file

// Below are lists of errors expect from Dxgi API calls when a transition event like mode change, PnpStop, PnpStart
// desktop switch, TDR or session disconnect/reconnect. In all these cases we want the application to clean up the threads that process
// the desktop updates and attempt to recreate them.
// If we get an error that is not on the appropriate list then we exit the application

// These are the errors we expect from general Dxgi API due to a transition
const std::vector<HRESULT> SystemTransitionsExpectedErrors = {
    DXGI_ERROR_DEVICE_REMOVED,
    DXGI_ERROR_ACCESS_LOST,
    static_cast<HRESULT>(WAIT_ABANDONED),
};

// These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
const std::vector<HRESULT> CreateDuplicationExpectedErrors = {
    DXGI_ERROR_DEVICE_REMOVED,
    static_cast<HRESULT>(E_ACCESSDENIED),
    DXGI_ERROR_UNSUPPORTED,
    DXGI_ERROR_SESSION_DISCONNECTED
};

// These are the errors we expect from IDXGIOutputDuplication methods due to a transition
const std::vector<HRESULT> FrameInfoExpectedErrors = {
    DXGI_ERROR_DEVICE_REMOVED,
    DXGI_ERROR_ACCESS_LOST,
    DXGI_ERROR_INVALID_CALL
};

class RecoverableVideoException : public std::exception
{
public:

    RecoverableVideoException(HRESULT hr);

    HRESULT Hresult() const;

private:
    HRESULT mHresult;
};

HRESULT TranslateHresultFailureWithDevice(winrt::com_ptr<ID3D11Device> device, HRESULT hr);

void ThrowExceptionCheckRecoverable(winrt::com_ptr<ID3D11Device> device, const std::vector<HRESULT> expectedErrors, HRESULT error);
