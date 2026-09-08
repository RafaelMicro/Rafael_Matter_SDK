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

#include "PressureManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace ::chip::DeviceLayer;

constexpr chip::EndpointId kPressureMeasurementEndpoint = 1;
constexpr uint16_t kSensorTimerPeriodMs                 = 60000; // 60s timer period

namespace PressureAttr = chip::app::Clusters::PressureMeasurement::Attributes;

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sSensorTimer;
StaticTimer_t sStaticSensorTimerStruct;

PressureManager PressureManager::sPressureMgr;

// Simulated Pressure Measurement MeasuredValue samples (unit: kPa * 10, int16s).
// e.g. 1013 => 101.3 kPa (~1 atm).
static int16_t mSimulatedPressure[] = { 1000, 1005, 1013, 1020, 1008, 995, 1002, 1018, 990, 1010 };

CHIP_ERROR PressureManager::Init()
{
    // Create FreeRTOS sw timer for the pressure sensor sampling period.
    sSensorTimer = xTimerCreateStatic("sensorTmr", pdMS_TO_TICKS(kSensorTimerPeriodMs), true, nullptr, SensorTimerEventHandler,
                                      &sStaticSensorTimerStruct);

    if (sSensorTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sSensorTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }

    // Trigger periodic update
    xTimerStart(sSensorTimer, 10);

    ChipLogProgress(NotSpecified, "PressureManager::Init");

    return CHIP_NO_ERROR;
}

void PressureManager::SensorTimerEventHandler(TimerHandle_t xTimer)
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

    int16_t pressure = mSimulatedPressure[simulatedIndex];
    ChipLogProgress(NotSpecified, "Pressure MeasuredValue is : %d", pressure);

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::Clusters::PressureMeasurement::Attributes::MeasuredValue::Set(kPressureMeasurementEndpoint, pressure);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
