/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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

#include "OccupancyManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"
#include "semphr.h"

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace ::chip::DeviceLayer;

constexpr EndpointId kOccupancyEndpoint = 1;

// Default HoldTime: 10 seconds (valid range 1~300)
constexpr uint16_t kHoldTimeDefault = 10;
constexpr uint16_t kHoldTimeMin     = 1;
constexpr uint16_t kHoldTimeMax     = 300;

OccupancyManager OccupancyManager::sOccuMgr;

CHIP_ERROR OccupancyManager::Init()
{
    // The code-driven cluster is registered by the ZAP integration and owns FeatureMap,
    // OccupancySensorType, OccupancySensorTypeBitmap and the HoldTime countdown. The
    // application only publishes the sensor state and the hold time this device supports.
    auto * cluster = OccupancySensing::FindClusterOnEndpoint(kOccupancyEndpoint);
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    cluster->SetHoldTimeLimits({ .holdTimeMin = kHoldTimeMin, .holdTimeMax = kHoldTimeMax, .holdTimeDefault = kHoldTimeDefault });

    DataModel::ActionReturnStatus status = cluster->SetHoldTime(kHoldTimeDefault);
    if (!status.IsSuccess())
    {
        ChipLogError(NotSpecified, "Failed to set HoldTime");
    }

    // Mirrors the cluster's HoldTime countdown for the indicator LED only (one-shot).
    mLedTimer = xTimerCreateStatic("OccuLedTmr", pdMS_TO_TICKS(kHoldTimeDefault * 1000), pdFALSE, nullptr, LedTimerEventHandler,
                                   &mStaticLedTimerStruct);
    if (mLedTimer == NULL)
    {
        ChipLogError(NotSpecified, "OccuLedTimer create failed");
        return APP_ERROR_CREATE_TIMER_FAILED;
    }

    ChipLogProgress(NotSpecified, "OccupancyManager::Init done");
    return CHIP_NO_ERROR;
}

void OccupancyManager::AttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value, uint16_t size) {}

CHIP_ERROR OccupancyManager::SetOccupancy(bool detected)
{
    auto * cluster = OccupancySensing::FindClusterOnEndpoint(kOccupancyEndpoint);
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    PlatformMgr().LockChipStack();
    // true sets Occupancy right away and cancels any running countdown; false only starts the
    // HoldTime countdown, after which the cluster clears Occupancy itself.
    cluster->SetOccupancy(detected);
    const uint16_t holdTime = cluster->GetHoldTime();
    PlatformMgr().UnlockChipStack();

    if (detected)
    {
        ChipLogProgress(NotSpecified, "Occupancy detected");
        xTimerStop(mLedTimer, 0);
        hosal_gpio_pin_clear(21);
    }
    else
    {
        ChipLogProgress(NotSpecified, "Detection ended - will clear in %u s", holdTime);
        xTimerChangePeriod(mLedTimer, pdMS_TO_TICKS(holdTime * 1000), 0);
        xTimerStart(mLedTimer, 0);
    }

    return CHIP_NO_ERROR;
}

void OccupancyManager::LedTimerEventHandler(TimerHandle_t xTimer)
{
    // HoldTime elapsed: the cluster has cleared Occupancy, follow it with the LED.
    ChipLogProgress(NotSpecified, "HoldTime expired, occupancy cleared");
    hosal_gpio_pin_set(21);
}
