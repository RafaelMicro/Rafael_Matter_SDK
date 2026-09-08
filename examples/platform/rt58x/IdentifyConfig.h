
#pragma once

#include "app/clusters/identify-server/IdentifyCluster.h"
#include "app/server-cluster/ServerClusterInterfaceRegistry.h"
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <platform/CHIPDeviceLayer.h>

#include "timers.h"
#include <FreeRTOS.h>

class RafaelIdentifyDelegate : public chip::app::Clusters::IdentifyDelegate
{
public:
    RafaelIdentifyDelegate(chip::EndpointId endPointId) :
        mEndPointId(endPointId), mIdentifyTimer(nullptr), mIdentifyFinished(0), mIdentifyOnOff(0)
    {}
    void OnIdentifyStart(chip::app::Clusters::IdentifyCluster & cluster) override;

    void OnIdentifyStop(chip::app::Clusters::IdentifyCluster & cluster) override;

    void OnTriggerEffect(chip::app::Clusters::IdentifyCluster & cluster) override;

    bool IsTriggerEffectEnabled() const override { return true; }
    static void OnTriggerIdentifyEffectBlink(chip::System::Layer * systemLayer, void * appState);
    static void OnTriggerIdentifyEffectBreathe(chip::System::Layer * systemLayer, void * appState);
    static void OnTriggerIdentifyEffectOk(chip::System::Layer * systemLayer, void * appState);
    static void OnTriggerIdentifyEffectChannel(chip::System::Layer * systemLayer, void * appState);
    static void OnTriggerIdentifyEffectCompleted(chip::System::Layer * systemLayer, void * appState);

    void SetIdentifyCallback(void (*onToggle)(uint8_t), void (*onStop)(void))
    {
        mIdentifyToggleCallback = onToggle;
        mIdentifyStopCallback   = onStop;
    }
    void SetIdentifyTimer(void);
    static void IdentifyTimerEventHandler(TimerHandle_t xTimer);
    chip::EndpointId mEndPointId = 1;

private:
    void (*mIdentifyToggleCallback)(uint8_t) = nullptr;
    void (*mIdentifyStopCallback)(void)      = nullptr;
    TimerHandle_t mIdentifyTimer;
    StaticTimer_t mIdentifyTimerStruct;
    // Blink-timer stop flag. Not the cluster state: use IdentifyCluster::IsIdentifying() for that.
    uint8_t mIdentifyFinished;
    uint8_t mIdentifyOnOff;
    chip::app::Clusters::Identify::EffectIdentifierEnum mIdentifyEffect =
        chip::app::Clusters::Identify::EffectIdentifierEnum::kStopEffect;
};

CHIP_ERROR IdentifyInit(void (*onToggle)(uint8_t), void (*onStop)(void));
