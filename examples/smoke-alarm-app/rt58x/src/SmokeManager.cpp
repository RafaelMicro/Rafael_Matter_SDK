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

#include "SmokeManager.h"
#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

#include <app/clusters/smoke-co-alarm-server/SmokeCOTestEventTriggerHandler.h>
#include <lib/support/TypeTraits.h>
/**********************************************************
 * Defines and Constants
 *********************************************************/

using namespace chip;
using namespace ::chip::app::Clusters;
using namespace ::chip::app::Clusters::SmokeCoAlarm;
using namespace ::chip::DeviceLayer;
constexpr EndpointId kAlarmEndpoint = 1;

TimerHandle_t sSelfTestTimer;
StaticTimer_t sStaticSelfTestTimerStruct;
TimerHandle_t sLedTimer;
StaticTimer_t sStaticLedTimerStruct;

static std::array<ExpressedStateEnum, SmokeCoAlarmServer::kPriorityOrderLength> sPriorityOrder = {
    ExpressedStateEnum::kSmokeAlarm,     ExpressedStateEnum::kInterconnectSmoke, ExpressedStateEnum::kCOAlarm,
    ExpressedStateEnum::kInterconnectCO, ExpressedStateEnum::kHardwareFault,     ExpressedStateEnum::kTesting,
    ExpressedStateEnum::kEndOfService,   ExpressedStateEnum::kBatteryAlert
};

/**********************************************************
 * Variable declarations
 *********************************************************/

SmokeManager SmokeManager::sSmokeManager;

CHIP_ERROR SmokeManager::Init()
{
    ChipLogProgress(NotSpecified, "SmokeManager::Init");
    mStartSelfTesting = false;
    mSmokeAlarmState  = AlarmStateEnum::kNormal;
    mCOAlarmState     = AlarmStateEnum::kNormal;

    // Smoke CO Alarm is code-driven, but its CodegenIntegration only provides weak, empty
    // MatterSmokeCoAlarm*Callback() stubs and never creates the cluster from ZAP, so the
    // application owns the cluster instance and its registration with the data model provider.
    SmokeCoAlarmCluster::Config config;
    config.featureMap.Set(Feature::kSmokeAlarm).Set(Feature::kCoAlarm);
    ReturnErrorOnFailure(SmokeCoAlarmServer::Instance().Init(kAlarmEndpoint, config, this));

    SmokeCoAlarmServer::Instance().SetSmokeState(kAlarmEndpoint, AlarmStateEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetCOState(kAlarmEndpoint, AlarmStateEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetTestInProgress(kAlarmEndpoint, false);
    SmokeCoAlarmServer::Instance().SetHardwareFaultAlert(kAlarmEndpoint, false);
    SmokeCoAlarmServer::Instance().SetBatteryAlert(kAlarmEndpoint, AlarmStateEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetEndOfServiceAlert(kAlarmEndpoint, EndOfServiceEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetInterconnectSmokeAlarm(kAlarmEndpoint, AlarmStateEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetInterconnectCOAlarm(kAlarmEndpoint, AlarmStateEnum::kNormal);
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kAlarmEndpoint, sPriorityOrder);

    sSelfTestTimer = xTimerCreateStatic("selfTestTmr", pdMS_TO_TICKS(10000), false, nullptr, SelfTestTimerEventHandler,
                                        &sStaticSelfTestTimerStruct);
    if (sSelfTestTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sSelfTestTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }
    sLedTimer = xTimerCreateStatic("ledTmr", pdMS_TO_TICKS(100), true, nullptr, AlarmLedTimerHandler, &sStaticLedTimerStruct);
    if (sLedTimer == NULL)
    {
        ChipLogProgress(NotSpecified, "sLedTimer timer create failed");
        return CHIP_ERROR_NO_MEMORY;
    }

    return CHIP_NO_ERROR;
}

void SmokeManager::StartSelfTesting()
{
    ChipLogProgress(Zcl, "Start self-testing");
    if (sSelfTestTimer && SmokeMgr().mStartSelfTesting == false)
    {
        SmokeMgr().mStartSelfTesting = true;
        SmokeCoAlarmServer::Instance().SetTestInProgress(kAlarmEndpoint, true);
        SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kAlarmEndpoint, sPriorityOrder);
        xTimerStart(sSelfTestTimer, 0);
    }
}

void SmokeManager::OnSelfTestRequested()
{
    StartSelfTesting();
}

void SmokeManager::SelfTestTimerEventHandler(TimerHandle_t xTimer)
{
    SmokeMgr().mStartSelfTesting = false;

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    SmokeCoAlarmServer::Instance().SetTestInProgress(kAlarmEndpoint, false);
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kAlarmEndpoint, sPriorityOrder);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    ChipLogProgress(Zcl, "End self-testing");
}
void SmokeManager::ToggleSmokeState(AlarmStateEnum newAlarmState)
{
    switch (newAlarmState)
    {
    case AlarmStateEnum::kNormal: {
        mSmokeAlarmState = AlarmStateEnum::kNormal;
        ChipLogProgress(NotSpecified, "Smoke State: Normal");
    }
    break;
    case AlarmStateEnum::kWarning: {
        mSmokeAlarmState = AlarmStateEnum::kWarning;
        ChipLogProgress(NotSpecified, "Smoke State: Warning");
    }
    break;
    case AlarmStateEnum::kCritical: {
        mSmokeAlarmState = AlarmStateEnum::kCritical;
        ChipLogProgress(NotSpecified, "Smoke State: Critical");
    }
    break;
    default:
        break;
    }
    PlatformMgr().LockChipStack();
    SmokeCoAlarmServer::Instance().SetSmokeState(kAlarmEndpoint, mSmokeAlarmState);
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kAlarmEndpoint, sPriorityOrder);
    PlatformMgr().UnlockChipStack();
}
void SmokeManager::AlarmLedTimerHandler(TimerHandle_t xTimer)
{
    hosal_gpio_pin_toggle(21);
}
void SmokeManager::OnExpressedStateChanged(ExpressedStateEnum newExpressedState)
{
    ChipLogProgress(NotSpecified, "Expressed State: %u", to_underlying(newExpressedState));
    VerifyOrReturn(sLedTimer != nullptr);

    if (newExpressedState == ExpressedStateEnum::kNormal)
    {
        xTimerStop(sLedTimer, 0);
        hosal_gpio_pin_set(21);
    }
    else if (!xTimerIsTimerActive(sLedTimer))
    {
        xTimerStart(sLedTimer, 0);
    }
}
void SmokeManager::OnSmokeSensitivityLevelChanged(SensitivityEnum newSmokeSensitivityLevel)
{
    ChipLogProgress(NotSpecified, "Smoke Sensitivity Level: %u", to_underlying(newSmokeSensitivityLevel));
}

bool HandleSmokeCOTestEventTrigger(uint64_t eventTrigger)
{
    // Normalize upper 16 bits to match SmokeCOTrigger enum prefix (0x005c = SmokeCoAlarm cluster ID).
    // The test harness may send 0xffff as a wildcard prefix; we replace it unconditionally.
    eventTrigger           = (eventTrigger & 0x0000FFFFFFFFFFFFULL) | 0x005c000000000000ULL;
    SmokeCOTrigger trigger = static_cast<SmokeCOTrigger>(eventTrigger);
    switch (trigger)
    {
    case SmokeCOTrigger::kForceSmokeWarning:
        SmokeCoAlarmServer::Instance().SetSmokeState(kAlarmEndpoint, AlarmStateEnum::kWarning);
        break;
    case SmokeCOTrigger::kForceSmokeCritical:
        SmokeCoAlarmServer::Instance().SetSmokeState(kAlarmEndpoint, AlarmStateEnum::kCritical);
        break;
    case SmokeCOTrigger::kClearSmoke:
        SmokeCoAlarmServer::Instance().SetSmokeState(kAlarmEndpoint, AlarmStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceCOWarning:
        SmokeCoAlarmServer::Instance().SetCOState(kAlarmEndpoint, AlarmStateEnum::kWarning);
        break;
    case SmokeCOTrigger::kForceCOCritical:
        SmokeCoAlarmServer::Instance().SetCOState(kAlarmEndpoint, AlarmStateEnum::kCritical);
        break;
    case SmokeCOTrigger::kClearCO:
        SmokeCoAlarmServer::Instance().SetCOState(kAlarmEndpoint, AlarmStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceLowBatteryWarning:
        SmokeCoAlarmServer::Instance().SetBatteryAlert(kAlarmEndpoint, AlarmStateEnum::kWarning);
        break;
    case SmokeCOTrigger::kForceLowBatteryCritical:
        SmokeCoAlarmServer::Instance().SetBatteryAlert(kAlarmEndpoint, AlarmStateEnum::kCritical);
        break;
    case SmokeCOTrigger::kClearBatteryLevelLow:
        SmokeCoAlarmServer::Instance().SetBatteryAlert(kAlarmEndpoint, AlarmStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceMalfunction:
        SmokeCoAlarmServer::Instance().SetHardwareFaultAlert(kAlarmEndpoint, true);
        break;
    case SmokeCOTrigger::kClearMalfunction:
        SmokeCoAlarmServer::Instance().SetHardwareFaultAlert(kAlarmEndpoint, false);
        break;
    case SmokeCOTrigger::kForceEndOfLife:
        SmokeCoAlarmServer::Instance().SetEndOfServiceAlert(kAlarmEndpoint, EndOfServiceEnum::kExpired);
        break;
    case SmokeCOTrigger::kClearEndOfLife:
        SmokeCoAlarmServer::Instance().SetEndOfServiceAlert(kAlarmEndpoint, EndOfServiceEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceSmokeInterconnect:
        SmokeCoAlarmServer::Instance().SetInterconnectSmokeAlarm(kAlarmEndpoint, AlarmStateEnum::kWarning);
        break;
    case SmokeCOTrigger::kClearSmokeInterconnect:
        SmokeCoAlarmServer::Instance().SetInterconnectSmokeAlarm(kAlarmEndpoint, AlarmStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceCOInterconnect:
        SmokeCoAlarmServer::Instance().SetInterconnectCOAlarm(kAlarmEndpoint, AlarmStateEnum::kWarning);
        break;
    case SmokeCOTrigger::kClearCOInterconnect:
        SmokeCoAlarmServer::Instance().SetInterconnectCOAlarm(kAlarmEndpoint, AlarmStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceSmokeContaminationHigh:
        SmokeCoAlarmServer::Instance().SetContaminationState(kAlarmEndpoint, ContaminationStateEnum::kCritical);
        break;
    case SmokeCOTrigger::kForceSmokeContaminationLow:
        SmokeCoAlarmServer::Instance().SetContaminationState(kAlarmEndpoint, ContaminationStateEnum::kLow);
        break;
    case SmokeCOTrigger::kClearContamination:
        SmokeCoAlarmServer::Instance().SetContaminationState(kAlarmEndpoint, ContaminationStateEnum::kNormal);
        break;
    case SmokeCOTrigger::kForceSmokeSensitivityHigh:
        SmokeCoAlarmServer::Instance().SetSmokeSensitivityLevel(kAlarmEndpoint, SensitivityEnum::kHigh);
        break;
    case SmokeCOTrigger::kForceSmokeSensitivityLow:
        SmokeCoAlarmServer::Instance().SetSmokeSensitivityLevel(kAlarmEndpoint, SensitivityEnum::kLow);
        break;
    case SmokeCOTrigger::kClearSensitivity:
        SmokeCoAlarmServer::Instance().SetSmokeSensitivityLevel(kAlarmEndpoint, SensitivityEnum::kStandard);
        break;
    case SmokeCOTrigger::kForceSilence:
        SmokeCoAlarmServer::Instance().SetDeviceMuted(kAlarmEndpoint, MuteStateEnum::kMuted);
        break;
    case SmokeCOTrigger::kClearSilence:
        SmokeCoAlarmServer::Instance().SetDeviceMuted(kAlarmEndpoint, MuteStateEnum::kNotMuted);
        break;
    default:
        return false;
    }
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kAlarmEndpoint, sPriorityOrder);
    return true;
}
