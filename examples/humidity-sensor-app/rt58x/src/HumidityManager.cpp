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

#include "HumidityManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;

constexpr EndpointId kHumidityMeasurementEndpoint = 1;

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sHumiTimer;
StaticTimer_t sStaticHumiTimerStruct;

HumidityManager HumidityManager::sHumiMgr;

static int16_t mSimulatedHumi[] = { 5500, 6800, 7200, 6500, 5200, 4000, 5000, 3900, 2700, 4200 };

CHIP_ERROR HumidityManager::Init()
{
    /* Simulate Humidity changes every minutes*/
    app::DataModel::Nullable<uint16_t> measuredValue;
    measuredValue.SetNonNull(5800);
    sHumiTimer = xTimerCreateStatic("HumiTmr", pdMS_TO_TICKS(60000), true, nullptr, HumiTimerEventHandler, &sStaticHumiTimerStruct);
    if (sHumiTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sHumiTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }
    xTimerStart(sHumiTimer, 10);
    CHIP_ERROR err = RelativeHumidityMeasurement::SetMeasuredValue(kHumidityMeasurementEndpoint, measuredValue);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(NotSpecified, "Failed to set measured value");
    }

    ChipLogProgress(NotSpecified, "HumiManager::Init");

    return CHIP_NO_ERROR;
}

void HumidityManager::HumiTimerEventHandler(TimerHandle_t xTimer)
{
    app::DataModel::Nullable<uint16_t> measuredValue;
    static uint8_t simulatedIndex = 0;
    if (simulatedIndex >= 9)
    {
        simulatedIndex = 0;
    }
    measuredValue.SetNonNull(mSimulatedHumi[simulatedIndex]);
    simulatedIndex++;
    ChipLogProgress(NotSpecified, "Humidity is : %d", measuredValue.Value());

    PlatformMgr().LockChipStack();
    CHIP_ERROR err = RelativeHumidityMeasurement::SetMeasuredValue(kHumidityMeasurementEndpoint, measuredValue);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogProgress(NotSpecified, "Failed to set measured value");
    }
    PlatformMgr().UnlockChipStack();
}
