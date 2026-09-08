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

#include "LightSensorManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace ::chip::DeviceLayer;

constexpr chip::EndpointId kIlluminanceMeasurementEndpoint = 1;
constexpr uint16_t kSensorTimerPeriodMs                    = 60000; // 60s timer period

namespace IlluminanceAttr = chip::app::Clusters::IlluminanceMeasurement::Attributes;

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sSensorTimer;
StaticTimer_t sStaticSensorTimerStruct;

LightSensorManager LightSensorManager::sLightSensorMgr;

// Simulated Illuminance Measurement MeasuredValue samples.
// Per the Matter spec, MeasuredValue = 10000 * log10(illuminance in lux) + 1,
// so e.g. ~30000 corresponds to roughly 1000 lux.
static uint16_t mSimulatedLux[] = { 20000, 25000, 30000, 35000, 40000, 45000, 38000, 28000, 22000, 33000 };

CHIP_ERROR LightSensorManager::Init()
{
    // Create FreeRTOS sw timer for the light sensor sampling period.
    sSensorTimer = xTimerCreateStatic("sensorTmr", pdMS_TO_TICKS(kSensorTimerPeriodMs), true, nullptr, SensorTimerEventHandler,
                                      &sStaticSensorTimerStruct);

    if (sSensorTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sSensorTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }

    // Trigger periodic update
    xTimerStart(sSensorTimer, 10);

    ChipLogProgress(NotSpecified, "LightSensorManager::Init");

    return CHIP_NO_ERROR;
}

void LightSensorManager::SensorTimerEventHandler(TimerHandle_t xTimer)
{
    static uint8_t simulatedIndex = 0;
    if (simulatedIndex >= 9)
    {
        simulatedIndex = 0;
    }
    else
    {
        simulatedIndex++;
    }

    uint16_t illuminance = mSimulatedLux[simulatedIndex];
    ChipLogProgress(NotSpecified, "Illuminance MeasuredValue is : %u", illuminance);

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::Clusters::IlluminanceMeasurement::Attributes::MeasuredValue::Set(kIlluminanceMeasurementEndpoint, illuminance);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
