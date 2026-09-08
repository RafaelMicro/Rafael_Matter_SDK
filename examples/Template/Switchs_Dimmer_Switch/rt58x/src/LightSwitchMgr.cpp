/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2018 Nest Labs, Inc.
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

#include "LightSwitchMgr.h"

#include "AppConfig.h"
#include "AppEvent.h"
#include "BindingHandler.h"

#include <app-common/zap-generated/cluster-objects.h>
#include <platform/CHIPDeviceLayer.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

LightSwitchMgr LightSwitchMgr::sSwitch;

/**
 * @brief Configures LightSwitchMgr and starts the binding handler.
 *        Must be called once before using the LightSwitchMgr.
 *
 * @param switchEndpoint endpoint hosting the On/Off client + Binding clusters (endpoint 1)
 */
CHIP_ERROR LightSwitchMgr::Init(chip::EndpointId switchEndpoint)
{
    VerifyOrReturnError(switchEndpoint != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);

    mSwitchEndpoint = switchEndpoint;
    return InitBindingHandler();
}

/**
 * @brief Send an On/Off command (On / Off / Toggle) to all devices bound to this
 *        switch's On/Off client cluster.
 */
void LightSwitchMgr::TriggerLightSwitchAction(LightSwitchAction action)
{
    BindingCommandData * data = Platform::New<BindingCommandData>();
    VerifyOrReturn(data != nullptr, ChipLogError(NotSpecified, "LightSwitchMgr: failed to allocate command data"));

    data->localEndpointId = mSwitchEndpoint;
    data->clusterId       = OnOff::Id;
    data->isGroup         = false;

    switch (action)
    {
    case LightSwitchAction::On:
        data->commandId = OnOff::Commands::On::Id;
        break;
    case LightSwitchAction::Off:
        data->commandId = OnOff::Commands::Off::Id;
        break;
    case LightSwitchAction::Toggle:
    default:
        data->commandId = OnOff::Commands::Toggle::Id;
        break;
    }

    DeviceLayer::PlatformMgr().ScheduleWork(SwitchWorkerFunction, reinterpret_cast<intptr_t>(data));
}

/**
 * @brief Send a LevelControl MoveToLevelWithOnOff command (target level 0..254) to all
 *        devices bound to this switch's Level Control client cluster.
 */
void LightSwitchMgr::TriggerLevelAction(uint8_t level)
{
    BindingCommandData * data = Platform::New<BindingCommandData>();
    VerifyOrReturn(data != nullptr, ChipLogError(NotSpecified, "LightSwitchMgr: failed to allocate command data"));

    data->localEndpointId = mSwitchEndpoint;
    data->clusterId       = LevelControl::Id;
    data->commandId       = LevelControl::Commands::MoveToLevelWithOnOff::Id;
    data->level           = level;
    data->isGroup         = false;

    DeviceLayer::PlatformMgr().ScheduleWork(SwitchWorkerFunction, reinterpret_cast<intptr_t>(data));
}
