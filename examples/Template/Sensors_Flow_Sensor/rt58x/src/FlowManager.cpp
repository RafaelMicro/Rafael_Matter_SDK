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

#include "FlowManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace ::chip::DeviceLayer;

constexpr chip::EndpointId kFlowMeasurementEndpoint = 1;
constexpr uint16_t kSensorTimerPeriodMs             = 60000; // 60s timer period

namespace FlowAttr = chip::app::Clusters::FlowMeasurement::Attributes;

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sSensorTimer;
StaticTimer_t sStaticSensorTimerStruct;

FlowManager FlowManager::sFlowMgr;

// Simulated Flow Measurement MeasuredValue samples (unit: 0.1 m3/h, uint16).
// e.g. 250 => 25.0 m3/h.
static uint16_t mSimulatedFlow[] = { 100, 150, 200, 250, 300, 220, 180, 260, 120, 320 };

CHIP_ERROR FlowManager::Init()
{
    // Create FreeRTOS sw timer for the flow sensor sampling period.
    sSensorTimer = xTimerCreateStatic("sensorTmr", pdMS_TO_TICKS(kSensorTimerPeriodMs), true, nullptr, SensorTimerEventHandler,
                                      &sStaticSensorTimerStruct);

    if (sSensorTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sSensorTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }

    // Trigger periodic update
    xTimerStart(sSensorTimer, 10);

    ChipLogProgress(NotSpecified, "FlowManager::Init");

    return CHIP_NO_ERROR;
}

void FlowManager::SensorTimerEventHandler(TimerHandle_t xTimer)
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

    uint16_t flow = mSimulatedFlow[simulatedIndex];
    ChipLogProgress(NotSpecified, "Flow MeasuredValue is : %u", flow);

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    chip::app::Clusters::FlowMeasurement::Attributes::MeasuredValue::Set(kFlowMeasurementEndpoint, flow);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
