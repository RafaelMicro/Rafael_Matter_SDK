/*
 *    Microwave Oven Control cluster (0x005F) delegate for the Microwave Oven (0x0079).
 *    Minimal application implementation: power-as-number with limits (PWRNUM | PWRLMTS).
 */

#pragma once

#include <app/clusters/microwave-oven-control-server/microwave-oven-control-server.h>
#include <protocols/interaction_model/StatusCode.h>

namespace chip {
namespace app {
namespace Clusters {
namespace MicrowaveOvenControl {

class ExampleMicrowaveOvenControlDelegate : public Delegate
{
public:
    Protocols::InteractionModel::Status HandleSetCookingParametersCallback(uint8_t cookMode, uint32_t cookTimeSec,
                                                                           bool startAfterSetting,
                                                                           Optional<uint8_t> powerSettingNum,
                                                                           Optional<uint8_t> wattSettingIndex) override;

    Protocols::InteractionModel::Status HandleModifyCookTimeSecondsCallback(uint32_t finalCookTimeSec) override;

    CHIP_ERROR GetWattSettingByIndex(uint8_t index, uint16_t & wattSetting) override;

    uint32_t GetMaxCookTimeSec() const override { return kMaxCookTimeSec; }
    uint8_t GetPowerSettingNum() const override { return mPowerSettingNum; }
    uint8_t GetMinPowerNum() const override { return kMinPowerNum; }
    uint8_t GetMaxPowerNum() const override { return kMaxPowerNum; }
    uint8_t GetPowerStepNum() const override { return kPowerStepNum; }
    uint8_t GetCurrentWattIndex() const override { return 0; }
    uint16_t GetWattRating() const override { return kWattRating; }

private:
    static constexpr uint32_t kMaxCookTimeSec = 86400u;
    static constexpr uint8_t kMinPowerNum     = 10u;
    static constexpr uint8_t kMaxPowerNum     = 100u;
    static constexpr uint8_t kPowerStepNum    = 10u;
    static constexpr uint16_t kWattRating     = 1000u;

    uint8_t mPowerSettingNum = kMaxPowerNum;
};

} // namespace MicrowaveOvenControl
} // namespace Clusters
} // namespace app
} // namespace chip

/**
 * Instantiate and initialise the Microwave Oven Control cluster instance on endpoint 1.
 * Must be called after chip::Server::Init() so that the Operational State and
 * Microwave Oven Mode instances (which self-register during endpoint init) already exist.
 */
void MicrowaveOvenControlInit();
