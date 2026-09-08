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

#include "SpeakerManager.h"

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include "AppConfig.h"
#include "AppTask.h"
#include <FreeRTOS.h>

#include <lib/support/TypeTraits.h>

#include "log.h"

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters::LevelControl;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;

SpeakerManager SpeakerManager::sSpeaker;

CHIP_ERROR SpeakerManager::Init()
{
    bool currentLedState;
    app::DataModel::Nullable<uint8_t> nullableCurrentLevel;

    Status status;

    OnOffServer::Instance().getOnOffValue(1, &currentLedState);
    status = Attributes::CurrentLevel::Get(1, nullableCurrentLevel);
    if (status == Status::Success)
    {
        mLevel = nullableCurrentLevel.Value();
    }
    else
    {
        mLevel = 0;
    }
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

bool SpeakerManager::IsTurnedOn()
{
    return mState == kState_On;
}

uint8_t SpeakerManager::GetLevel()
{
    return mLevel;
}

bool SpeakerManager::InitiateAction(Action_t aAction, uint8_t * value)
{
    bool action_initiated = false;
    State_t new_state     = mState;

    switch (aAction)
    {
    case ON_ACTION:
        ChipLogProgress(NotSpecified, "SpeakerMgr:ON: %s->ON", mState == kState_On ? "ON" : "OFF");
        break;
    case OFF_ACTION:
        ChipLogProgress(NotSpecified, "SpeakerMgr:OFF: %s->OFF", mState == kState_On ? "ON" : "OFF");
        break;
    case LEVEL_ACTION:
        ChipLogProgress(NotSpecified, "SpeakerMgr:LEVEL: lev:%u->%u", mLevel, *value);
        break;
    default:
        ChipLogProgress(NotSpecified, "SpeakerMgr:Unknown");
        break;
    }

    // Initiate Turn On/Off Action only when the previous one is complete.
    if (mState == kState_Off && aAction == ON_ACTION)
    {
        action_initiated = true;

        new_state = kState_On;
    }
    else if (mState == kState_On && aAction == OFF_ACTION)
    {
        action_initiated = true;

        new_state = kState_Off;
    }
    else if (aAction == LEVEL_ACTION && *value != mLevel)
    {
        action_initiated = true;
        if (*value == 0)
        {
            new_state = kState_Off;
        }
        else
        {
            new_state = kState_On;
        }
    }
    if (action_initiated)
    {
        if (aAction == LEVEL_ACTION)
        {
            SetLevel(*value);
        }
        else
        {
            Set(new_state == kState_On);
        }
        UpdateLight();
    }

    return action_initiated;
}

void SpeakerManager::SetLevel(uint8_t aLevel)
{
    mLevel = aLevel;
}

void SpeakerManager::Set(bool aOn)
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

void SpeakerManager::UpdateLight()
{
    if (mTimer)
    {
        xTimerChangePeriod(mTimer, pdMS_TO_TICKS(500), 0);
    }
}

void SpeakerManager::TimerEventHandler(TimerHandle_t xTimer)
{
    ChipLogProgress(NotSpecified, "Device state: %s, level: %u", sSpeaker.mState == kState_On ? "ON" : "OFF", sSpeaker.mLevel);

    if (sSpeaker.mState == kState_On && sSpeaker.mLevel > 1)
    {
        // Dimmable light: drive a white LED at the current level.
        pwm_set_color(sSpeaker.mLevel, sSpeaker.mLevel, sSpeaker.mLevel);
    }
    else
    {
        pwm_set_color(0, 0, 0);
    }
}
