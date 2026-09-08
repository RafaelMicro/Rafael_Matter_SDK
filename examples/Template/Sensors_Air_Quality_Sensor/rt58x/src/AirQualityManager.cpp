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

/**********************************************************
 * Includes
 *********************************************************/

#include "AirQualityManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

#include <app/clusters/air-quality-server/air-quality-server.h>

/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace ::chip::DeviceLayer;
using namespace chip::app::Clusters;
using chip::app::Clusters::AirQuality::AirQualityEnum;
using chip::app::Clusters::AirQuality::Feature;

constexpr chip::EndpointId kAirQualityEndpoint = 1;
constexpr uint16_t kSensorTimerPeriodMs        = 60000; // 60s timer period

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sSensorTimer;
StaticTimer_t sStaticSensorTimerStruct;

AirQualityManager AirQualityManager::sAirQualityMgr;

// Air Quality cluster server instance on endpoint 1. Enable the extended enum
// values (Fair / Moderate / VeryPoor / ExtremelyPoor) via the feature map.
static AirQuality::Instance sAirQualityInstance(kAirQualityEndpoint,
                                                BitMask<Feature>(Feature::kFair, Feature::kModerate, Feature::kVeryPoor,
                                                                 Feature::kExtremelyPoor));

// Simulated air quality states cycled by the sampling timer.
static AirQualityEnum mSimulatedAirQuality[] = {
    AirQualityEnum::kGood, AirQualityEnum::kFair, AirQualityEnum::kModerate, AirQualityEnum::kPoor, AirQualityEnum::kVeryPoor,
};

CHIP_ERROR AirQualityManager::Init()
{
    // Register the Air Quality cluster instance (must run after the server is up).
    CHIP_ERROR err = sAirQualityInstance.Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "AirQuality::Instance::Init failed");
        return err;
    }

    // Create FreeRTOS sw timer for the air quality sampling period.
    sSensorTimer = xTimerCreateStatic("sensorTmr", pdMS_TO_TICKS(kSensorTimerPeriodMs), true, nullptr, SensorTimerEventHandler,
                                      &sStaticSensorTimerStruct);

    if (sSensorTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sSensorTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }

    // Trigger periodic update
    xTimerStart(sSensorTimer, 10);

    ChipLogProgress(NotSpecified, "AirQualityManager::Init");

    return CHIP_NO_ERROR;
}

void AirQualityManager::SensorTimerEventHandler(TimerHandle_t xTimer)
{
    static uint8_t simulatedIndex = 0;
    simulatedIndex                = (simulatedIndex + 1) % (sizeof(mSimulatedAirQuality) / sizeof(mSimulatedAirQuality[0]));

    AirQualityEnum airQuality = mSimulatedAirQuality[simulatedIndex];
    ChipLogProgress(NotSpecified, "Air Quality is : %u", static_cast<uint8_t>(airQuality));

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    sAirQualityInstance.UpdateAirQuality(airQuality);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
