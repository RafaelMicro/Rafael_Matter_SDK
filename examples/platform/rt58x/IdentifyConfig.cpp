#include "IdentifyConfig.h"

// Matter 1.6 moved this from app/ to platform/.
#include "platform/DefaultTimerDelegate.h"

#include <data-model-providers/codegen/CodegenDataModelProvider.h>
#include <lib/core/ErrorStr.h>
#include <platform/CHIPDeviceLayer.h>

using namespace chip;
using namespace chip::app;
using namespace chip::DeviceLayer;

// ================================================================================
// Identify Cluster Delegate
// ================================================================================
RafaelIdentifyDelegate gRafaelIdentifyDelegate(/*EndPointId*/ 1);
DefaultTimerDelegate gTimerDelegate;
RegisteredServerCluster<chip::app::Clusters::IdentifyCluster>
    gIdentifyCluster(Clusters::IdentifyCluster::Config(gRafaelIdentifyDelegate.mEndPointId, gTimerDelegate)
                         .WithIdentifyType(Clusters::Identify::IdentifyTypeEnum::kVisibleIndicator)
                         .WithDelegate(&gRafaelIdentifyDelegate));

// ================================================================================
// Identify Cluster Callbacks
// ================================================================================
CHIP_ERROR IdentifyInit(void (*onToggle)(uint8_t), void (*onStop)(void))
{
    gRafaelIdentifyDelegate.SetIdentifyCallback(onToggle, onStop);
    gRafaelIdentifyDelegate.SetIdentifyTimer();
    CHIP_ERROR err = CodegenDataModelProvider::Instance().Registry().Register(gIdentifyCluster.Registration());
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "IdentifyInit failed: %s", ErrorStr(err));
    }
    return err;
}
void RafaelIdentifyDelegate::SetIdentifyTimer(void)
{
    mIdentifyTimer =
        xTimerCreateStatic("tmr_id", pdMS_TO_TICKS(500), pdTRUE, this, IdentifyTimerEventHandler, &mIdentifyTimerStruct);
}
void RafaelIdentifyDelegate::IdentifyTimerEventHandler(TimerHandle_t xTimer)
{
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(pvTimerGetTimerID(xTimer));
    if (instance == nullptr)
    {
        return;
    }
    if (!instance->mIdentifyFinished)
    {
        instance->mIdentifyOnOff ^= 1;
        if (instance->mIdentifyToggleCallback != nullptr)
        {
            instance->mIdentifyToggleCallback(instance->mIdentifyOnOff);
        }
    }
    else
    {
        instance->mIdentifyOnOff = 0;
        if (instance->mIdentifyStopCallback != nullptr)
        {
            instance->mIdentifyStopCallback();
        }
        xTimerStop(xTimer, 0);
    }
}
void RafaelIdentifyDelegate::OnIdentifyStart(Clusters::IdentifyCluster & cluster)
{
    ChipLogProgress(NotSpecified, "IdentifyStart");
    mIdentifyOnOff    = 1;
    mIdentifyFinished = 0;
    if (!xTimerIsTimerActive(mIdentifyTimer))
    {
        xTimerStart(mIdentifyTimer, 0);
    }
}
void RafaelIdentifyDelegate::OnIdentifyStop(Clusters::IdentifyCluster & cluster)
{
    mIdentifyFinished = 1;
}
void RafaelIdentifyDelegate::OnTriggerIdentifyEffectCompleted(chip::System::Layer * systemLayer, void * appState)
{
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(appState);
    instance->mIdentifyEffect         = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
    if (instance->mIdentifyStopCallback != nullptr)
    {
        instance->mIdentifyStopCallback();
    }
}
void RafaelIdentifyDelegate::OnTriggerIdentifyEffectBlink(chip::System::Layer * systemLayer, void * appState)
{
    static uint8_t blink_cnt;
    static bool onoff                 = true;
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(appState);
    if (instance->mIdentifyEffect == Clusters::Identify::EffectIdentifierEnum::kStopEffect)
        blink_cnt = 2;
    if (blink_cnt < 2)
    {
        if (instance->mIdentifyToggleCallback != nullptr)
        {
            instance->mIdentifyToggleCallback(onoff);
        }
        blink_cnt++;
        onoff = !onoff;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectBlink, appState);
    }
    else
    {
        blink_cnt                 = 0;
        onoff                     = true;
        instance->mIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectCompleted, appState);
    }
}

void RafaelIdentifyDelegate::OnTriggerIdentifyEffectBreathe(chip::System::Layer * systemLayer, void * appState)
{
    static uint8_t breath_cnt;
    static bool onoff                 = true;
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(appState);
    if (instance->mIdentifyEffect == Clusters::Identify::EffectIdentifierEnum::kStopEffect)
        breath_cnt = 30;
    if (breath_cnt < 30)
    {
        if (instance->mIdentifyToggleCallback != nullptr)
        {
            instance->mIdentifyToggleCallback(onoff);
        }
        onoff = !onoff;
        breath_cnt++;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectBreathe, appState);
    }
    else
    {
        breath_cnt                = 0;
        onoff                     = true;
        instance->mIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectCompleted, appState);
    }
}

void RafaelIdentifyDelegate::OnTriggerIdentifyEffectOk(chip::System::Layer * systemLayer, void * appState)
{
    static uint8_t okay_cnt;
    static bool onoff                 = true;
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(appState);
    if (instance->mIdentifyEffect == Clusters::Identify::EffectIdentifierEnum::kStopEffect)
        okay_cnt = 4;
    if (okay_cnt < 4)
    {
        if (instance->mIdentifyToggleCallback != nullptr)
        {
            instance->mIdentifyToggleCallback(onoff);
        }
        onoff = !onoff;
        okay_cnt++;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectOk, appState);
    }
    else
    {
        okay_cnt                  = 0;
        onoff                     = true;
        instance->mIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectCompleted, appState);
    }
}

void RafaelIdentifyDelegate::OnTriggerIdentifyEffectChannel(chip::System::Layer * systemLayer, void * appState)
{
    static uint8_t channel_cnt;
    RafaelIdentifyDelegate * instance = static_cast<RafaelIdentifyDelegate *>(appState);
    if (instance->mIdentifyEffect == Clusters::Identify::EffectIdentifierEnum::kStopEffect)
        channel_cnt = 16;
    if (channel_cnt < 16)
    {
        if (channel_cnt == 0)
        {
            if (instance->mIdentifyToggleCallback != nullptr)
            {
                instance->mIdentifyToggleCallback(1);
            }
        }
        else
        {
            if (instance->mIdentifyToggleCallback != nullptr)
            {
                instance->mIdentifyToggleCallback(0);
            }
        }
        channel_cnt++;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectChannel, appState);
    }
    else
    {
        channel_cnt               = 0;
        instance->mIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        (void) SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(500), OnTriggerIdentifyEffectCompleted, appState);
    }
}

void RafaelIdentifyDelegate::OnTriggerEffect(Clusters::IdentifyCluster & cluster)
{
    mIdentifyEffect = cluster.GetEffectIdentifier();
    switch (cluster.GetEffectIdentifier())
    {
    case Clusters::Identify::EffectIdentifierEnum::kBlink:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBlink");
        (void) SystemLayer().StartTimer(chip::System::Clock::Seconds16(1), OnTriggerIdentifyEffectBlink, this);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kBreathe:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kBreathe");
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(1), OnTriggerIdentifyEffectBreathe, this);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kOkay:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kOkay");
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(1), OnTriggerIdentifyEffectOk, this);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kChannelChange:
        ChipLogProgress(Zcl, "Clusters::Identify::EffectIdentifierEnum::kChannelChange");
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(1), OnTriggerIdentifyEffectChannel, this);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kFinishEffect:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_FINISH_EFFECT");
        mIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        break;
    default:
        ChipLogProgress(Zcl, "No identifier effect");
        return;
    }
}
