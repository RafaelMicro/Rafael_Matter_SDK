/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
 *    Copyright (c) 2024 Rafael Microelectronics, Inc.
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

/**
 *    @file
 *          CHIP System Layer clock support for Rafael RT58x SoCs.
 *
 *          The monotonic clock is sourced from the FreeRTOS tick counter,
 *          widened to 64 bits by state private to this file.
 *
 *          Wall-clock (UTC) time is tracked as an offset against that
 *          monotonic base. The offset is seeded at boot by
 *          InitClock_RealTime() and updated through SetClock_RealTime(),
 *          e.g. by the Time Synchronization cluster, so that UTC time can
 *          be read back at any point afterwards.
 */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/TimeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/RT58x/SystemTimeSupport.h>

#include <FreeRTOS.h>
#include <task.h>

#include <inttypes.h>

namespace chip {
namespace System {
namespace Clock {

namespace Internal {
ClockImpl gClockImpl;
} // namespace Internal

namespace {

static_assert(kMicrosecondsPerSecond % configTICK_RATE_HZ == 0, "Tick rate must divide one second evenly");
constexpr uint64_t kMicrosecondsPerTick = kMicrosecondsPerSecond / configTICK_RATE_HZ;

// UTC time corresponding to the monotonic-clock zero point.
Microseconds64 sUtcTimeBase = Microseconds64::zero();

Microseconds64 Uptime()
{
    static uint32_t sTickWraps = 0;
    static uint32_t sLastTicks = 0;

    uint64_t ticks;

    // Task context only, which is where the CHIP stack reads the clock from.
    taskENTER_CRITICAL();
    {
        const uint32_t nowTicks = xTaskGetTickCount();
        if (nowTicks < sLastTicks)
        {
            sTickWraps++;
        }
        sLastTicks = nowTicks;
        ticks      = (static_cast<uint64_t>(sTickWraps) << 32) | nowTicks;
    }
    taskEXIT_CRITICAL();

    return Microseconds64(ticks * kMicrosecondsPerTick);
}

} // namespace

Microseconds64 ClockImpl::GetMonotonicMicroseconds64(void)
{
    return Uptime();
}

Milliseconds64 ClockImpl::GetMonotonicMilliseconds64(void)
{
    return std::chrono::duration_cast<Milliseconds64>(Uptime());
}

CHIP_ERROR ClockImpl::GetClock_RealTime(Microseconds64 & aCurTime)
{
    aCurTime = sUtcTimeBase + Uptime();

    return CHIP_NO_ERROR;
}

CHIP_ERROR ClockImpl::GetClock_RealTimeMS(Milliseconds64 & aCurTime)
{
    Microseconds64 curTimeUS;

    ReturnErrorOnFailure(GetClock_RealTime(curTimeUS));
    aCurTime = std::chrono::duration_cast<Milliseconds64>(curTimeUS);

    return CHIP_NO_ERROR;
}

CHIP_ERROR ClockImpl::SetClock_RealTime(Microseconds64 aNewCurTime)
{
    const Microseconds64 uptime = Uptime();

    // A timestamp smaller than the current uptime cannot be anchored to the
    // monotonic clock; fall back to the epoch in that case.
    sUtcTimeBase = (aNewCurTime > uptime) ? aNewCurTime - uptime : Microseconds64::zero();

    return CHIP_NO_ERROR;
}

/**
 * Seed the wall clock with CHIP_SYSTEM_CONFIG_VALID_REAL_TIME_THRESHOLD so
 * that real-time reads pass the validity check even before the first
 * synchronization. Called once during platform initialization.
 */
CHIP_ERROR InitClock_RealTime()
{
    const Microseconds64 threshold =
        Microseconds64(static_cast<uint64_t>(CHIP_SYSTEM_CONFIG_VALID_REAL_TIME_THRESHOLD) * kMicrosecondsPerSecond);

    return System::SystemClock().SetClock_RealTime(threshold);
}

} // namespace Clock
} // namespace System
} // namespace chip
