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

#include "WaterFreezeManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;

constexpr EndpointId kWaterFreezeEndpoint = 1;

WaterFreezeManager WaterFreezeManager::sWaterFreezeMgr;

CHIP_ERROR WaterFreezeManager::Init()
{
    auto booleanState = BooleanState::FindClusterOnEndpoint(kWaterFreezeEndpoint);
    // Default resting state is "no freeze" (StateValue = false); pressing pin1 reports a freeze.
    ChipLogProgress(NotSpecified, "Water Freeze State: No freeze");
    booleanState->SetStateValue(false);
    hosal_gpio_pin_clear(21);
    return CHIP_NO_ERROR;
}

void WaterFreezeManager::AttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value, uint16_t size) {}

void WaterFreezeManager::ToggleStateValue(bool val)
{
    auto booleanState = BooleanState::FindClusterOnEndpoint(kWaterFreezeEndpoint);
    if (booleanState != nullptr)
    {
        if (val)
        {
            ChipLogProgress(NotSpecified, "Water Freeze State: Freeze");
            hosal_gpio_pin_set(21);
        }
        else
        {
            ChipLogProgress(NotSpecified, "Water Freeze State: No freeze");
            hosal_gpio_pin_clear(21);
        }
        PlatformMgr().LockChipStack();
        booleanState->SetStateValue(val);
        PlatformMgr().UnlockChipStack();
    }
}
