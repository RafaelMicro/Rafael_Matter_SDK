/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include "AppConfig.h"
#include "OTAConfig.h"
#include "hosal_gpio.h"
#include <matter_config.h>

#include "task.h"
#include <FreeRTOS.h>

#include <mbedtls/platform.h>

#ifdef ENABLE_CHIP_SHELL
#include "matter_shell.h"
#endif

#include "flashctl.h"
#include "fota_define.h"
#include "log.h"
#include "mcu.h"
#include <EnhancedFlashDataset.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/InteractionModelEngine.h>
#include <app/reporting/reporting.h>
#include <app/util/attribute-storage.h>
#include <assert.h>
#include <lib/core/CHIPError.h>
#include <lib/core/ErrorStr.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/ZclString.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>

#include <app/clusters/network-commissioning/CodegenInstance.h>

using namespace chip;
using namespace chip::app;
using namespace chip::DeviceLayer;
using namespace chip::Protocols::InteractionModel;

// ================================================================================
// NetworkCommissioning Cluster Driver
// ================================================================================
static NetworkCommissioning::GenericThreadDriver sRafaelThreadDriver;
static Clusters::NetworkCommissioning::Instance sNetworkCommissioningInstance(0, &sRafaelThreadDriver);

// ================================================================================
// FreeRTOS Callbacks
// ================================================================================
extern "C" void vApplicationIdleHook(void)
{
    otTaskletsSignalPending(0);
    // FreeRTOS Idle callback
    // Check CHIP Config nvm3 and repack flash if necessary.
    // Internal::RT58xConfig::RepackNvm3Flash();
}
void MatterFotaInit(void)
{
    fota_information_t * p_fota_info = (fota_information_t *) (FOTA_UPDATE_BANK_INFO_ADDRESS);

    if (p_fota_info->fotabank_ready == FOTA_IMAGE_READY)
    {
        if (p_fota_info->fota_result == FOTA_RESULT_SUCCESS)
        {
            ChipLogProgress(NotSpecified, "fota success");
        }
        else
        {
            ChipLogProgress(NotSpecified, "fota failed 0x%02x", p_fota_info->fota_result);
        }
        while (flash_check_busy())
            ;
        taskENTER_CRITICAL();
        flash_erase(FLASH_ERASE_SECTOR, FOTA_UPDATE_BANK_INFO_ADDRESS);
        taskEXIT_CRITICAL();
    }
    else
    {
        uint32_t bootreason = 0;
        ConfigurationMgr().GetBootReason(bootreason);
        if (bootreason == 5)
        {
            ConfigurationMgr().StoreBootReason(1); // kPowerOnReboot
        }
    }
}
void MatterNetworkInit(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    // Initialize Network Commissioning Cluster
    err = sNetworkCommissioningInstance.Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "Network Commissioning Cluster Init failed: %s", ErrorStr(err));
        return;
    }
}
void DoFactoryReset(intptr_t arg)
{
    auto & server      = chip::Server::GetInstance();
    auto & fabricTable = server.GetFabricTable();
    auto & sessionMgr  = server.GetSecureSessionManager();

    // Expire all CASE sessions for current fabrics, then delete fabrics.
    for (const auto & fabricInfo : fabricTable)
    {
        sessionMgr.ExpireAllSessionsForFabric(fabricInfo.GetFabricIndex());
    }
    fabricTable.DeleteAllFabrics();

#if CHIP_ENABLE_OPENTHREAD
    // Leave Thread network and clear persisted dataset.
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT
    if (ThreadStackMgr().IsThreadAttached())
    {
        CHIP_ERROR srpErr = ThreadStackMgr().ClearAllSrpHostAndServices();
        if (srpErr != CHIP_NO_ERROR)
        {
            ChipLogError(NotSpecified, "ClearAllSrpHostAndServices failed: %s", ErrorStr(srpErr));
        }
        else
        {
            ChipLogProgress(NotSpecified, "SRP host/services cleared");
        }
    }
#endif
    // ConnectivityMgr().ErasePersistentInfo();
#endif
#if 1
    vTaskSuspendAll();
    efd_env_set_default();
    sys_software_reset();
    vTaskSuspendAll();
#else
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    // Ensure previous window state does not block a new one (e.g. after multiple presses).
    server.GetCommissioningWindowManager().CloseCommissioningWindow();

    CHIP_ERROR bleErr = ConnectivityMgr().SetBLEAdvertisingEnabled(true);
    if (bleErr != CHIP_NO_ERROR && bleErr != CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
    {
        ChipLogError(NotSpecified, "SetBLEAdvertisingEnabled(true) failed: %s", ErrorStr(bleErr));
    }

    CHIP_ERROR err = server.GetCommissioningWindowManager().OpenBasicCommissioningWindow(
        chip::System::Clock::Seconds32(CHIP_DEVICE_CONFIG_DISCOVERY_TIMEOUT_SECS),
        chip::CommissioningWindowAdvertisement::kAllSupported);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "OpenBasicCommissioningWindow failed: %s", ErrorStr(err));
    }
    else
    {
        ChipLogProgress(NotSpecified, "Commissioning window opened (BLE advertising should be enabled)");
    }
#endif
}
Status emberAfExternalAttributeReadCallback(EndpointId endpoint, ClusterId clusterId,
                                            const EmberAfAttributeMetadata * attributeMetadata, uint8_t * buffer,
                                            uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "External Attribute Read Callback: ClusterId=0x%04lX, AttributeId=0x%04lX", clusterId,
                    attributeMetadata->attributeId);

    return Status::Failure;
}
