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
#include <math.h>

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include "AppConfig.h"
#include "AppTask.h"
#include <FreeRTOS.h>

#include <lib/support/TypeTraits.h>

#include "log.h"

/*Macro to correct RGB value proportional to current level*/
#define CorrectRGB(v, l) ((uint8_t) ((uint16_t) (v) * (uint16_t) (l) / 255))

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters::LevelControl;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;

LightingManager LightingManager::sLight;

CHIP_ERROR LightingManager::Init()
{
    bool currentLedState;
    uint16_t currentLedColorTemperature;
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

    ColorControl::Attributes::ColorMode::Get(1, &mColorMode);
    status = ColorControl::Attributes::ColorTemperatureMireds::Get(1, &currentLedColorTemperature);
    if (status == Status::Success)
    {
        mCT.ctMireds = currentLedColorTemperature;
    }
    mRGB   = CTToRgb(mCT);
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

uint8_t LightingManager::GetLevel()
{
    return mLevel;
}

RgbColor_t LightingManager::GetRgb()
{
    return mRGB;
}

bool LightingManager::InitiateAction(Action_t aAction, uint8_t * value)
{
    bool action_initiated = false;
    State_t new_state     = mState;
    CtColor_t ct;

    switch (aAction)
    {
    case ON_ACTION:
        ChipLogProgress(NotSpecified, "LightMgr:ON: %s->ON", mState == kState_On ? "ON" : "OFF");
        break;
    case OFF_ACTION:
        ChipLogProgress(NotSpecified, "LightMgr:OFF: %s->OFF", mState == kState_On ? "ON" : "OFF");
        break;
    case LEVEL_ACTION:
        ChipLogProgress(NotSpecified, "LightMgr:LEVEL: lev:%u->%u", mLevel, *value);
        break;
    case COLOR_ACTION_CT:
        ct.ctMireds = *reinterpret_cast<uint16_t *>(value);
        ChipLogProgress(NotSpecified, "LightMgr:COLOR: ct:%u->%u", mCT.ctMireds, ct.ctMireds);
        break;
    default:
        ChipLogProgress(NotSpecified, "LightMgr:Unknown");
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
    else if (aAction == COLOR_ACTION_CT)
    {
        action_initiated = true;
        if (ct.ctMireds == 0)
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
        else if (aAction == COLOR_ACTION_CT)
        {
            SetColorTemperature(ct);
        }
        else
        {
            Set(new_state == kState_On);
        }
        UpdateLight();
    }

    return action_initiated;
}

void LightingManager::SetLevel(uint8_t aLevel)
{
    mLevel = aLevel;
}

void LightingManager::SetColorTemperature(CtColor_t ct)
{
    mCT.ctMireds = ct.ctMireds;
    mRGB         = CTToRgb(mCT);
}

void LightingManager::SetColorMode(ColorControl::ColorModeEnum ColorMode)
{
    mColorMode = ColorMode;
    if (mColorMode == ColorControl::ColorModeEnum::kColorTemperatureMireds)
    {
        mRGB = CTToRgb(mCT);
    }
    UpdateLight();
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
    ChipLogProgress(NotSpecified, "R: %d, G: %d, B: %d\r\n", sLight.mRGB.r, sLight.mRGB.g, sLight.mRGB.b);

    if (sLight.mState == kState_On && sLight.mLevel > 1)
    {
        // Color temperature light: scale the color-temperature-derived RGB by the current level.
        pwm_set_color(CorrectRGB(sLight.mRGB.r, sLight.mLevel), CorrectRGB(sLight.mRGB.g, sLight.mLevel),
                      CorrectRGB(sLight.mRGB.b, sLight.mLevel));
    }
    else
    {
        pwm_set_color(0, 0, 0);
    }
}
