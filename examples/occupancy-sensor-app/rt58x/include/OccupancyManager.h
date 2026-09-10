/*
 *
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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "AppEvent.h"

#include "FreeRTOS.h"
#include "timers.h" // provides FreeRTOS timer support
#include <app/clusters/occupancy-sensor-server/CodegenIntegration.h>

#include <lib/core/CHIPError.h>

class OccupancyManager
{
public:
    CHIP_ERROR Init();
    void AttributeChangeHandler(chip::EndpointId endpointId, chip::AttributeId attributeId, uint8_t * value, uint16_t size);
    // Reports the raw sensor state to the cluster: true while occupancy is detected, false once
    // detection ends. The cluster clears Occupancy HoldTime seconds after the false.
    CHIP_ERROR SetOccupancy(bool detected);

private:
    friend OccupancyManager & OccuMgr();

    static void LedTimerEventHandler(TimerHandle_t xTimer);

    // The cluster owns the HoldTime countdown that clears Occupancy; this timer only mirrors it
    // so that the indicator LED goes out at the same moment.
    TimerHandle_t mLedTimer;
    StaticTimer_t mStaticLedTimerStruct;

    static OccupancyManager sOccuMgr;
};

inline OccupancyManager & OccuMgr()
{
    return OccupancyManager::sOccuMgr;
}
