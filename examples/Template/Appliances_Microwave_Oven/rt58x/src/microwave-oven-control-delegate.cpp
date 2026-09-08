/*
 *    Microwave Oven Control cluster (0x005F) delegate implementation for the Microwave Oven (0x0079).
 */

#include "microwave-oven-control-delegate.h"

// app/util/config.h defines the MATTER_DM_PLUGIN_*_SERVER macros that gate the
// accessor namespaces in the delegate headers below, so it must come first.
#include <app/util/config.h>

#include <chef-operational-state-delegate-impl.h>
#include <microwave-oven-mode.h>

#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::MicrowaveOvenControl;
using Status = chip::Protocols::InteractionModel::Status;

Status ExampleMicrowaveOvenControlDelegate::HandleSetCookingParametersCallback(uint8_t cookMode, uint32_t cookTimeSec,
                                                                               bool startAfterSetting,
                                                                               Optional<uint8_t> powerSettingNum,
                                                                               Optional<uint8_t> wattSettingIndex)
{
    ChipLogProgress(Zcl, "MicrowaveOvenControl: SetCookingParameters mode=%u time=%us start=%d", cookMode,
                    static_cast<unsigned>(cookTimeSec), startAfterSetting);
    if (powerSettingNum.HasValue())
    {
        mPowerSettingNum = powerSettingNum.Value();
    }
    GetInstance()->SetCookTimeSec(cookTimeSec);
    return Status::Success;
}

Status ExampleMicrowaveOvenControlDelegate::HandleModifyCookTimeSecondsCallback(uint32_t finalCookTimeSec)
{
    ChipLogProgress(Zcl, "MicrowaveOvenControl: ModifyCookTime -> %us", static_cast<unsigned>(finalCookTimeSec));
    GetInstance()->SetCookTimeSec(finalCookTimeSec);
    return Status::Success;
}

CHIP_ERROR ExampleMicrowaveOvenControlDelegate::GetWattSettingByIndex(uint8_t index, uint16_t & wattSetting)
{
    // PowerInWatts feature is not supported by this device.
    return CHIP_ERROR_NOT_FOUND;
}

void MicrowaveOvenControlInit()
{
    using namespace chip::app::Clusters;

    OperationalState::Instance * opStateInstance = OperationalState::GetOperationalStateInstance();
    ModeBase::Instance * modeInstance            = MicrowaveOvenMode::Instance();
    VerifyOrDie(opStateInstance != nullptr && modeInstance != nullptr);

    static MicrowaveOvenControl::ExampleMicrowaveOvenControlDelegate delegate;
    static MicrowaveOvenControl::Instance instance(
        &delegate, /* endpoint */ 1, MicrowaveOvenControl::Id,
        BitMask<MicrowaveOvenControl::Feature>(MicrowaveOvenControl::Feature::kPowerAsNumber,
                                               MicrowaveOvenControl::Feature::kPowerNumberLimits),
        *opStateInstance, *modeInstance);
    instance.Init();
}
