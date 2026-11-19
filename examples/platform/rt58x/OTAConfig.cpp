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

#include "OTAConfig.h"
#include <app/clusters/ota-requestor/CodegenIntegration.h>
#include <app/server/Server.h>
#include <lib/core/ErrorStr.h>

// Global OTA objects
chip::DefaultOTARequestor gRequestorCore;
chip::DefaultOTARequestorStorage gRequestorStorage;
chip::DeviceLayer::DefaultOTARequestorDriver gRequestorUser;
chip::BDXDownloader gDownloader;
chip::OTAImageProcessorImpl gImageProcessor;

void OTAConfig::Init()
{
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().AddEventHandler(OnPlatformEvent, 0);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "Failed to register OTA event handler: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void OTAConfig::OnPlatformEvent(const chip::DeviceLayer::ChipDeviceEvent * event, intptr_t arg)
{
    // On OpenThread, kDnssdInitialized is posted once the SRP server has answered, so the
    // provider address can be resolved by the time the requestor connects to it.
    if (event->Type != chip::DeviceLayer::DeviceEventType::kDnssdInitialized || chip::GetRequestorInstance() != nullptr)
    {
        return;
    }

    ChipLogProgress(SoftwareUpdate, "DNS-SD initialized, scheduling OTA Requestor initialization");
    CHIP_ERROR err = chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds32(kInitOTARequestorDelaySec),
                                                                 InitOTARequestorHandler, nullptr);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "Failed to schedule OTA Requestor initialization: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void OTAConfig::InitOTARequestorHandler(chip::System::Layer * systemLayer, void * appState)
{
    InitOTARequestor();
}

void OTAConfig::InitOTARequestor()
{
    VerifyOrReturn(chip::GetRequestorInstance() == nullptr);

    // Initialize and interconnect the Requestor and Image Processor objects -- START
    SetRequestorInstance(&gRequestorCore);

    gRequestorStorage.Init(chip::Server::GetInstance().GetPersistentStorage());

    CHIP_ERROR err = gRequestorCore.Init(chip::Server::GetInstance(), gRequestorStorage, gRequestorUser, gDownloader,
                                         chip::GetOTARequestorAttributes(), chip::GetDefaultOTARequestorEventGenerator());
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "OTA requestor init failed: %s", chip::ErrorStr(err));
        return;
    }

    // Periodic query timeout must be set prior to requestor being initialized
    gRequestorUser.SetPeriodicQueryTimeout(OTA_PERIODIC_TIMEOUT);
    gRequestorUser.Init(&gRequestorCore, &gImageProcessor);

    gImageProcessor.SetOTADownloader(&gDownloader);

    // Connect the Downloader and Image Processor objects
    gDownloader.SetImageProcessorDelegate(&gImageProcessor);
    // Initialize and interconnect the Requestor and Image Processor objects -- END
}
