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

#include "RainManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;

constexpr EndpointId kRainEndpoint = 1;

RainManager RainManager::sRainMgr;

CHIP_ERROR RainManager::Init()
{
    auto booleanState = BooleanState::FindClusterOnEndpoint(kRainEndpoint);
    // Default resting state is "no rain" (StateValue = false); pressing pin1 reports rain.
    ChipLogProgress(NotSpecified, "Rain State: No rain");
    booleanState->SetStateValue(false);
    hosal_gpio_pin_clear(21);
    return CHIP_NO_ERROR;
}

void RainManager::AttributeChangeHandler(EndpointId endpointId, AttributeId attributeId, uint8_t * value, uint16_t size) {}

void RainManager::ToggleStateValue(bool val)
{
    auto booleanState = BooleanState::FindClusterOnEndpoint(kRainEndpoint);
    if (booleanState != nullptr)
    {
        if (val)
        {
            ChipLogProgress(NotSpecified, "Rain State: Rain");
            hosal_gpio_pin_set(21);
        }
        else
        {
            ChipLogProgress(NotSpecified, "Rain State: No rain");
            hosal_gpio_pin_clear(21);
        }
        PlatformMgr().LockChipStack();
        booleanState->SetStateValue(val);
        PlatformMgr().UnlockChipStack();
    }
}
