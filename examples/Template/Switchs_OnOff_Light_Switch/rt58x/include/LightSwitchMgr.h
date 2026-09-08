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

#pragma once

#include <app/util/basic-types.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>

// Manager for the On/Off Light Switch (device type 0x0103). On a button action it
// sends an On/Off command (On / Off / Toggle) to the devices bound to the switch's
// On/Off client cluster, via the Binding cluster.
class LightSwitchMgr
{
public:
    enum class LightSwitchAction : uint8_t
    {
        Toggle, // Triggers a Toggle command
        On,     // Triggers an On command
        Off     // Triggers an Off command
    };

    // switchEndpoint is the local endpoint that hosts the On/Off client + Binding clusters (endpoint 1).
    CHIP_ERROR Init(chip::EndpointId switchEndpoint);

    // Send the given On/Off action to all bound devices.
    void TriggerLightSwitchAction(LightSwitchAction action);

    static LightSwitchMgr & GetInstance() { return sSwitch; }

private:
    static LightSwitchMgr sSwitch;

    chip::EndpointId mSwitchEndpoint = chip::kInvalidEndpointId;
};
