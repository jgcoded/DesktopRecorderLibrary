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

#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "..\VideoLibrary\CaptureFrameStep.h"
#include "..\VideoLibrary\RenderPointerTextureStep.h"

#include <ScreenGrab.h>
#include <wincodec.h>

namespace VideoLibraryTests
{
    TEST_CLASS(RecordingStepsTests)
    {
    public:
        TEST_METHOD(RecordDesktop)
        {
            auto virtualDesktop = std::make_shared<VirtualDesktop>();

            auto monitors = virtualDesktop->GetAllDesktopMonitors();
            RotatingKeys keys{ 0, 1 };
            auto duplicators = virtualDesktop->RecordMonitors(monitors);
            std::unique_ptr<RecordingStep> recordingStep;
            int i = 0;
            for (auto& duplicator : duplicators) {

                for (int j = 0; j < 100; ++i, ++j) {
                    recordingStep.reset(new CaptureFrameStep{ *duplicator });
                    recordingStep->Perform();
                }
            }

            auto step = dynamic_cast<RenderPointerTextureStep*>(recordingStep.get());
            Assert::IsNotNull(step);
            step->Perform();
            auto texture = step->Result();
            winrt::com_ptr<ID3D11Device> device;
            winrt::com_ptr<ID3D11DeviceContext> context;
            texture->GetDevice(device.put());
            device->GetImmediateContext(context.put());

            //DirectX::SaveWICTextureToFile(context.get(), virtualDesktop->DesktopPointer().Texture().get(), GUID_ContainerFormatPng, L"pointer.jpg");
            DirectX::SaveWICTextureToFile(context.get(), texture.get(), GUID_ContainerFormatJpeg, L"test.jpg");

        }


    };
}
