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
#include <string>
#include "AudioMedia.h"

std::vector<AudioDevice> AudioMedia::GetAudioRecordingDevices()
{
    std::vector<AudioDevice> result;
    winrt::com_ptr<IMFAttributes> attributes;
    winrt::check_hresult(MFCreateAttributes(attributes.put(), 1));

    winrt::check_hresult(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID));

    IMFActivate** activationObjects = nullptr;
    UINT32 numObjects;
    winrt::check_hresult(MFEnumDeviceSources(attributes.get(), &activationObjects, &numObjects));

    for (UINT32 i = 0; i < numObjects; ++i) {

        auto activationObject = activationObjects[i];

        auto device = AudioMedia::GetAudioRecordingDeviceFromActivator(activationObject);

        if (!device.endpoint.empty()) {
            result.push_back(device);
        }

        activationObject->Release();
        activationObject = nullptr;
    }

    CoTaskMemFree(activationObjects);

    return result;
}

winrt::com_ptr<IMFMediaSource> AudioMedia::GetAudioMediaSourceFromEndpoint(std::wstring endpoint)
{
    winrt::com_ptr<IMFAttributes> attributes;
    winrt::check_hresult(MFCreateAttributes(attributes.put(), 2));
    winrt::check_hresult(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID));
    winrt::check_hresult(attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, endpoint.c_str()));

    winrt::com_ptr<IMFMediaSource> source;
    winrt::check_hresult(MFCreateDeviceSource(attributes.get(), source.put()));

    return source;
}

bool AudioMedia::IsAudioRecordingDeviceAvailable(std::wstring endpoint)
{
    auto devices = AudioMedia::GetAudioRecordingDevices();
    for (const auto& device : devices) {
        if (device.endpoint == endpoint) {
            return true;
        }
    }
    return false;
}

AudioDevice AudioMedia::GetAudioRecordingDeviceFromActivator(IMFActivate* activationObject)
{
    AudioDevice device;

    // get friendly name
    UINT32 strLength = 0;
    winrt::check_hresult(activationObject->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &strLength));

    device.friendlyName = std::wstring(strLength, '\0');
    winrt::check_hresult(activationObject->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, (LPWSTR)device.friendlyName.data(), (UINT32)device.friendlyName.size() + 1, nullptr));

    // get endpoint
    winrt::check_hresult(activationObject->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, &strLength));

    device.endpoint = std::wstring(strLength, '\0');
    winrt::check_hresult(activationObject->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_ENDPOINT_ID, (LPWSTR)device.endpoint.data(), (UINT32)device.endpoint.size() + 1, nullptr));

    return device;
}
