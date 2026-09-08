/*
 *    On/Off Light Switch (device type 0x0103) binding handler implementation.
 */

#include "BindingHandler.h"

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/bindings/BindingManager.h>
#include <app/clusters/bindings/binding-table.h>
#include <app/server/Server.h>
#include <controller/InvokeInteraction.h>
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;

namespace {

void OnOffProcessUnicast(CommandId commandId, const Binding::TableEntry & binding, OperationalDeviceProxy * peer_device)
{
    VerifyOrReturn(peer_device != nullptr && peer_device->ConnectionReady(),
                   ChipLogError(NotSpecified, "OnOff binding: peer device not ready"));

    auto onSuccess = [](const ConcreteCommandPath & commandPath, const StatusIB & status, const auto & dataResponse) {
        ChipLogProgress(NotSpecified, "OnOff command succeeded");
    };
    auto onFailure = [](CHIP_ERROR error) {
        ChipLogError(NotSpecified, "OnOff command failed: %" CHIP_ERROR_FORMAT, error.Format());
    };

    switch (commandId)
    {
    case Clusters::OnOff::Commands::Toggle::Id: {
        Clusters::OnOff::Commands::Toggle::Type cmd;
        Controller::InvokeCommandRequest(peer_device->GetExchangeManager(), peer_device->GetSecureSession().Value(), binding.remote,
                                         cmd, onSuccess, onFailure);
        break;
    }
    case Clusters::OnOff::Commands::On::Id: {
        Clusters::OnOff::Commands::On::Type cmd;
        Controller::InvokeCommandRequest(peer_device->GetExchangeManager(), peer_device->GetSecureSession().Value(), binding.remote,
                                         cmd, onSuccess, onFailure);
        break;
    }
    case Clusters::OnOff::Commands::Off::Id: {
        Clusters::OnOff::Commands::Off::Type cmd;
        Controller::InvokeCommandRequest(peer_device->GetExchangeManager(), peer_device->GetSecureSession().Value(), binding.remote,
                                         cmd, onSuccess, onFailure);
        break;
    }
    default:
        break;
    }
}

void OnOffProcessGroup(CommandId commandId, const Binding::TableEntry & binding)
{
    Messaging::ExchangeManager & exchangeMgr = Server::GetInstance().GetExchangeManager();

    switch (commandId)
    {
    case Clusters::OnOff::Commands::Toggle::Id: {
        Clusters::OnOff::Commands::Toggle::Type cmd;
        Controller::InvokeGroupCommandRequest(&exchangeMgr, binding.fabricIndex, binding.groupId, cmd);
        break;
    }
    case Clusters::OnOff::Commands::On::Id: {
        Clusters::OnOff::Commands::On::Type cmd;
        Controller::InvokeGroupCommandRequest(&exchangeMgr, binding.fabricIndex, binding.groupId, cmd);
        break;
    }
    case Clusters::OnOff::Commands::Off::Id: {
        Clusters::OnOff::Commands::Off::Type cmd;
        Controller::InvokeGroupCommandRequest(&exchangeMgr, binding.fabricIndex, binding.groupId, cmd);
        break;
    }
    default:
        break;
    }
}

void LevelControlProcessUnicast(const BindingCommandData * data, const Binding::TableEntry & binding,
                                OperationalDeviceProxy * peer_device)
{
    VerifyOrReturn(peer_device != nullptr && peer_device->ConnectionReady(),
                   ChipLogError(NotSpecified, "LevelControl binding: peer device not ready"));

    auto onSuccess = [](const ConcreteCommandPath & commandPath, const StatusIB & status, const auto & dataResponse) {
        ChipLogProgress(NotSpecified, "LevelControl command succeeded");
    };
    auto onFailure = [](CHIP_ERROR error) {
        ChipLogError(NotSpecified, "LevelControl command failed: %" CHIP_ERROR_FORMAT, error.Format());
    };

    if (data->commandId == Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id)
    {
        Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Type cmd;
        cmd.level = data->level;
        cmd.transitionTime.SetNonNull(static_cast<uint16_t>(0));
        Controller::InvokeCommandRequest(peer_device->GetExchangeManager(), peer_device->GetSecureSession().Value(), binding.remote,
                                         cmd, onSuccess, onFailure);
    }
}

void LevelControlProcessGroup(const BindingCommandData * data, const Binding::TableEntry & binding)
{
    Messaging::ExchangeManager & exchangeMgr = Server::GetInstance().GetExchangeManager();

    if (data->commandId == Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Id)
    {
        Clusters::LevelControl::Commands::MoveToLevelWithOnOff::Type cmd;
        cmd.level = data->level;
        cmd.transitionTime.SetNonNull(static_cast<uint16_t>(0));
        Controller::InvokeGroupCommandRequest(&exchangeMgr, binding.fabricIndex, binding.groupId, cmd);
    }
}

// Invoked by the BindingManager for each binding entry on the notified endpoint/cluster.
void LightSwitchChangedHandler(const Binding::TableEntry & binding, OperationalDeviceProxy * peer_device, void * context)
{
    VerifyOrReturn(context != nullptr, ChipLogError(NotSpecified, "LightSwitchChangedHandler: null context"));
    BindingCommandData * data = static_cast<BindingCommandData *>(context);

    const bool isGroupBinding   = (binding.type == Binding::MATTER_MULTICAST_BINDING && data->isGroup);
    const bool isUnicastBinding = (binding.type == Binding::MATTER_UNICAST_BINDING && !data->isGroup);

    switch (data->clusterId)
    {
    case Clusters::OnOff::Id:
        if (isGroupBinding)
            OnOffProcessGroup(data->commandId, binding);
        else if (isUnicastBinding)
            OnOffProcessUnicast(data->commandId, binding, peer_device);
        break;
    case Clusters::LevelControl::Id:
        if (isGroupBinding)
            LevelControlProcessGroup(data, binding);
        else if (isUnicastBinding)
            LevelControlProcessUnicast(data, binding, peer_device);
        break;
    default:
        break;
    }
}

void LightSwitchContextReleaseHandler(void * context)
{
    VerifyOrReturn(context != nullptr, ChipLogError(NotSpecified, "LightSwitchContextReleaseHandler: null context"));
    Platform::Delete(static_cast<BindingCommandData *>(context));
}

void InitBindingHandlerInternal(intptr_t arg)
{
    auto & server = chip::Server::GetInstance();
    Binding::Manager::GetInstance().Init(
        { &server.GetFabricTable(), server.GetCASESessionManager(), &server.GetPersistentStorage() });
    Binding::Manager::GetInstance().RegisterBoundDeviceChangedHandler(LightSwitchChangedHandler);
    Binding::Manager::GetInstance().RegisterBoundDeviceContextReleaseHandler(LightSwitchContextReleaseHandler);
}

} // namespace

void SwitchWorkerFunction(intptr_t context)
{
    VerifyOrReturn(context != 0, ChipLogError(NotSpecified, "SwitchWorkerFunction: null context"));
    BindingCommandData * data = reinterpret_cast<BindingCommandData *>(context);
    Binding::Manager::GetInstance().NotifyBoundClusterChanged(data->localEndpointId, data->clusterId, static_cast<void *>(data));
}

CHIP_ERROR InitBindingHandler()
{
    // BindingManager must be initialized on the Matter task once the server is up.
    chip::DeviceLayer::PlatformMgr().ScheduleWork(InitBindingHandlerInternal, 0);
    return CHIP_NO_ERROR;
}
