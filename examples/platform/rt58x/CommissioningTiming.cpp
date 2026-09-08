/*
 *
 *    Copyright (c) 2025 Project CHIP Authors
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

#include "CommissioningTiming.h"

#if APP_ENABLE_COMMISSIONING_TIMING

#include <inttypes.h>

#include <lib/support/logging/CHIPLogging.h>
#include <system/SystemClock.h>
#include <transport/SecureSession.h>

using chip::System::Clock::Timestamp;
using namespace chip::DeviceLayer;

namespace RafaelCommissioningTiming {
namespace {

Timestamp sCommissioningStartTimestamp;
bool sCommissioningStartValid = false;
bool sPaseStageInProgress     = false;
Timestamp sPaseEndTimestamp;
bool sPaseTimingValid = false;
Timestamp sCaseStartTimestamp;
bool sCaseStageInProgress = false;
Timestamp sCaseEndTimestamp;
bool sCaseTimingValid = false;
Timestamp sOperationalResumeStartTimestamp;
bool sOperationalResumeTimingValid = false;
Timestamp sBootToServerReadyStartTimestamp;
bool sBootToServerReadyTimingValid = false;

Timestamp Now()
{
    return chip::System::SystemClock().GetMonotonicTimestamp();
}

void HandleBleConnectionEstablished()
{
    sCommissioningStartTimestamp = Now();
    sCommissioningStartValid     = true;
    sPaseStageInProgress         = true;
    sCaseStageInProgress         = false;
    sPaseTimingValid             = false;
    sCaseTimingValid             = false;
    sPaseEndTimestamp            = Timestamp();
    sCaseStartTimestamp          = Timestamp();
    sCaseEndTimestamp            = Timestamp();
    ChipLogProgress(NotSpecified, "Commissioning BLE connection established(PASE stage started) (tick=%" PRIu64 " ms)",
                    static_cast<uint64_t>(sCommissioningStartTimestamp.count()));
}

void HandleServerReady()
{
    if (sOperationalResumeTimingValid)
    {
        const auto now            = Now();
        const auto resumeDuration = now - sOperationalResumeStartTimestamp;
        ChipLogProgress(NotSpecified,
                        "Operational resume complete (start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms)",
                        static_cast<uint64_t>(sOperationalResumeStartTimestamp.count()), static_cast<uint64_t>(now.count()),
                        static_cast<uint64_t>(resumeDuration.count()));
        sOperationalResumeTimingValid = false;
    }

    if (sBootToServerReadyTimingValid)
    {
        const auto now          = Now();
        const auto bootDuration = now - sBootToServerReadyStartTimestamp;
        ChipLogProgress(NotSpecified,
                        "Boot->ServerReady timing (start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms)",
                        static_cast<uint64_t>(sBootToServerReadyStartTimestamp.count()), static_cast<uint64_t>(now.count()),
                        static_cast<uint64_t>(bootDuration.count()));
        sBootToServerReadyTimingValid = false;
    }
}

void HandleCommissioningComplete()
{
    if (sCommissioningStartValid)
    {
        const auto completion = Now();
        const auto duration   = completion - sCommissioningStartTimestamp;
        const auto paseDuration =
            sPaseTimingValid ? (sPaseEndTimestamp - sCommissioningStartTimestamp) : chip::System::Clock::Milliseconds64(0);
        const auto caseDuration =
            sCaseTimingValid ? (sCaseEndTimestamp - sCaseStartTimestamp) : chip::System::Clock::Milliseconds64(0);
        ChipLogProgress(NotSpecified, "===Commissioning Timestamp===");
        ChipLogProgress(NotSpecified,
                        "Commissioning completed (overall start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms)",
                        static_cast<uint64_t>(sCommissioningStartTimestamp.count()), static_cast<uint64_t>(completion.count()),
                        static_cast<uint64_t>(duration.count()));
        ChipLogProgress(NotSpecified, "PASE start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms",
                        static_cast<uint64_t>(sCommissioningStartTimestamp.count()),
                        static_cast<uint64_t>(sPaseTimingValid ? sPaseEndTimestamp.count() : completion.count()),
                        static_cast<uint64_t>(paseDuration.count()));
        ChipLogProgress(NotSpecified, "CASE start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms",
                        static_cast<uint64_t>(sCaseTimingValid ? sCaseStartTimestamp.count() : completion.count()),
                        static_cast<uint64_t>(sCaseTimingValid ? sCaseEndTimestamp.count() : completion.count()),
                        static_cast<uint64_t>(caseDuration.count()));
        ChipLogProgress(NotSpecified, "======");
        sCommissioningStartValid = false;
    }
    sCaseStageInProgress = false;
    sPaseStageInProgress = false;
}

void HandleSecureSessionEstablished(uint8_t secureSessionType)
{
    const auto sessionType = static_cast<chip::Transport::SecureSession::Type>(secureSessionType);
    const auto now         = Now();

    if (sessionType == chip::Transport::SecureSession::Type::kPASE)
    {
        if (sCommissioningStartValid && sPaseStageInProgress)
        {
            const auto paseDuration = now - sCommissioningStartTimestamp;
            ChipLogProgress(NotSpecified,
                            "PASE session complete (start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms)",
                            static_cast<uint64_t>(sCommissioningStartTimestamp.count()), static_cast<uint64_t>(now.count()),
                            static_cast<uint64_t>(paseDuration.count()));
        }
        sPaseEndTimestamp    = now;
        sPaseTimingValid     = true;
        sPaseStageInProgress = false;
        sCaseStartTimestamp  = now;
        sCaseStageInProgress = true;
        ChipLogProgress(NotSpecified, "CASE stage started (tick=%" PRIu64 " ms)", static_cast<uint64_t>(now.count()));
    }
    else if (sessionType == chip::Transport::SecureSession::Type::kCASE)
    {
        if (sCaseStageInProgress)
        {
            const auto caseDuration = now - sCaseStartTimestamp;
            ChipLogProgress(NotSpecified,
                            "CASE session complete (start=%" PRIu64 " ms, end=%" PRIu64 " ms, duration=%" PRIu64 " ms)",
                            static_cast<uint64_t>(sCaseStartTimestamp.count()), static_cast<uint64_t>(now.count()),
                            static_cast<uint64_t>(caseDuration.count()));
        }
        sCaseEndTimestamp    = now;
        sCaseTimingValid     = true;
        sCaseStageInProgress = false;
    }
}

} // namespace

void MarkBootStart()
{
    sBootToServerReadyStartTimestamp = Now();
    sBootToServerReadyTimingValid    = true;
}

void MarkOperationalResumeStart()
{
    sOperationalResumeStartTimestamp = Now();
    sOperationalResumeTimingValid    = true;
}

void HandleChipDeviceEvent(const ChipDeviceEvent * event)
{
    if (event == nullptr)
    {
        return;
    }

    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLEConnectionEstablished:
        HandleBleConnectionEstablished();
        break;
    case DeviceEventType::kServerReady:
        HandleServerReady();
        break;
    case DeviceEventType::kCommissioningComplete:
        HandleCommissioningComplete();
        break;
    case DeviceEventType::kSecureSessionEstablished:
        HandleSecureSessionEstablished(event->SecureSessionEstablished.SecureSessionType);
        break;
    default:
        break;
    }
}

} // namespace RafaelCommissioningTiming

#endif // APP_ENABLE_COMMISSIONING_TIMING
