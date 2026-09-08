/*
 *    On/Off Light Switch (device type 0x0103) binding handler.
 *
 *    Sends On/Off commands (On / Off / Toggle) to the devices bound to this switch's
 *    On/Off client cluster, via the Binding cluster + BindingManager. Supports both
 *    unicast and group bindings.
 */

#pragma once

#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/util/basic-types.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

struct BindingCommandData
{
    chip::EndpointId localEndpointId = 1;
    chip::CommandId commandId        = chip::app::Clusters::OnOff::Commands::Toggle::Id;
    chip::ClusterId clusterId        = chip::app::Clusters::OnOff::Id;
    bool isGroup                     = false;
};

// Initialize the BindingManager and register the On/Off command handlers.
// Must be called after the Matter server is initialized (on the Matter task).
CHIP_ERROR InitBindingHandler();

// Schedules sending the On/Off command described by `context` (a heap-allocated
// BindingCommandData, ownership transferred) to all bound devices. Runs on the Matter task.
void SwitchWorkerFunction(intptr_t context);
