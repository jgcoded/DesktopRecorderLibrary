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
#include "Errors.h"

HRESULT TranslateHresultFailureWithDevice(winrt::com_ptr<ID3D11Device> device, HRESULT hr)
{
    HRESULT translatedHr = hr;
    HRESULT deviceRemovedReason = device->GetDeviceRemovedReason();

    if (device)
    {
        switch (deviceRemovedReason)
        {
        case DXGI_ERROR_DEVICE_REMOVED:
        case DXGI_ERROR_DEVICE_RESET:
            case static_cast<HRESULT>(E_OUTOFMEMORY) :
            {
                // Our device has been stopped due to an external event on the GPU so map them all to
                // device removed and continue processing the condition
                translatedHr = DXGI_ERROR_DEVICE_REMOVED;
                break;
            }

            case S_OK:
                // Device is not removed
                break;

            default:
                // Device is removed, but we don't want to lose this removal reason
                translatedHr = deviceRemovedReason;
                break;
        }
    }

    return translatedHr;
}

void ThrowExceptionCheckRecoverable(winrt::com_ptr<ID3D11Device> device, const std::vector<HRESULT> expectedErrors, HRESULT error)
{
    HRESULT translatedHr = TranslateHresultFailureWithDevice(device, error);

    if (std::find(expectedErrors.begin(), expectedErrors.end(), translatedHr) != expectedErrors.end())
    {
        throw RecoverableVideoException(translatedHr);
    }

    winrt::throw_hresult(error);
}

inline RecoverableVideoException::RecoverableVideoException(HRESULT hr)
{
    mHresult = hr;
}

inline HRESULT RecoverableVideoException::Hresult() const
{
    return mHresult;
}
