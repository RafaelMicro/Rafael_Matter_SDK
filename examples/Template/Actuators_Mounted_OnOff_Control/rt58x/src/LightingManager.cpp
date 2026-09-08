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

#include "LightingManager.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include "AppConfig.h"
#include "AppTask.h"
#include <FreeRTOS.h>

#include <lib/support/TypeTraits.h>

#include "log.h"

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;

LightingManager LightingManager::sLight;

CHIP_ERROR LightingManager::Init()
{
    bool currentLedState;

    OnOffServer::Instance().getOnOffValue(1, &currentLedState);
    mState = currentLedState ? kState_On : kState_Off;

    mTimer = xTimerCreate("LightTimer", pdMS_TO_TICKS(100), pdFALSE, nullptr, TimerEventHandler);
    if (mTimer == nullptr)
    {
        ChipLogError(NotSpecified, "Failed to create timer");
        return CHIP_ERROR_NO_MEMORY;
    }
    UpdateLight();

    return CHIP_NO_ERROR;
}

bool LightingManager::IsTurnedOn()
{
    return mState == kState_On;
}

bool LightingManager::InitiateAction(Action_t aAction, uint8_t * value)
{
    bool action_initiated = false;
    State_t new_state     = mState;

    switch (aAction)
    {
    case ON_ACTION:
        ChipLogProgress(NotSpecified, "LightMgr:ON: %s->ON", mState == kState_On ? "ON" : "OFF");
        break;
    case OFF_ACTION:
        ChipLogProgress(NotSpecified, "LightMgr:OFF: %s->OFF", mState == kState_On ? "ON" : "OFF");
        break;
    default:
        ChipLogProgress(NotSpecified, "LightMgr:Unknown");
        break;
    }

    // Initiate Turn On/Off Action only when the previous one is complete.
    if (mState == kState_Off && aAction == ON_ACTION)
    {
        action_initiated = true;
        new_state        = kState_On;
    }
    else if (mState == kState_On && aAction == OFF_ACTION)
    {
        action_initiated = true;
        new_state        = kState_Off;
    }

    if (action_initiated)
    {
        Set(new_state == kState_On);
        UpdateLight();
    }

    return action_initiated;
}

void LightingManager::Set(bool aOn)
{
    if (aOn)
    {
        mState = kState_On;
    }
    else
    {
        mState = kState_Off;
    }
}

void LightingManager::UpdateLight()
{
    if (mTimer)
    {
        xTimerChangePeriod(mTimer, pdMS_TO_TICKS(500), 0);
    }
}

void LightingManager::TimerEventHandler(TimerHandle_t xTimer)
{
    ChipLogProgress(NotSpecified, "Device state: %s", sLight.mState == kState_On ? "ON" : "OFF");

    if (sLight.mState == kState_On)
    {
        pwm_set_color(255, 255, 255);
    }
    else
    {
        pwm_set_color(0, 0, 0);
    }
}
