/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <app/clusters/ota-requestor/BDXDownloader.h>
#include <app/clusters/ota-requestor/DefaultOTARequestor.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorDriver.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorStorage.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/RT58x/OTAImageProcessorImpl.h>

class OTAConfig
{
public:
    OTAConfig(){};

    // Defers the OTA requestor initialization until DNS-SD is ready, so that the
    // NotifyUpdateApplied sent on the first boot of a new image can reach the provider.
    static void Init();
    static constexpr uint32_t kInitOTARequestorDelaySec = 3;

private:
    static void OnPlatformEvent(const chip::DeviceLayer::ChipDeviceEvent * event, intptr_t arg);
    static void InitOTARequestorHandler(chip::System::Layer * systemLayer, void * appState);
    static void InitOTARequestor();
};
