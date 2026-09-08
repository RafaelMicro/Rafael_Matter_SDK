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

/**
 *    @file
 *      Commissioning timing instrumentation shared by all RT58x example apps.
 *
 *      It measures and logs:
 *        - Boot -> ServerReady
 *        - Operational resume (already commissioned device) -> ServerReady
 *        - PASE / CASE session setup and the overall commissioning duration
 */

#pragma once

#include <platform/CHIPDeviceLayer.h>

/**
 * Set to 0 (for instance from the project BUILD.gn defines) to compile the
 * instrumentation out. Every entry point below then becomes an empty inline, so
 * no call site needs a #if guard.
 */
#ifndef APP_ENABLE_COMMISSIONING_TIMING
#define APP_ENABLE_COMMISSIONING_TIMING 1
#endif

namespace RafaelCommissioningTiming {

#if APP_ENABLE_COMMISSIONING_TIMING

/**
 * Starts the Boot->ServerReady stopwatch. Call it as early as possible in
 * AppTask::Init().
 */
void MarkBootStart();

/**
 * Starts the operational resume stopwatch. Call it from AppTask::InitServer()
 * when the device is already commissioned (FabricCount() != 0).
 */
void MarkOperationalResumeStart();

/**
 * Feeds a CHIP device event to the instrumentation. Call it for every event
 * from AppTask::ChipEventHandler(); events that are not of interest are
 * ignored.
 */
void HandleChipDeviceEvent(const chip::DeviceLayer::ChipDeviceEvent * event);

#else // APP_ENABLE_COMMISSIONING_TIMING

inline void MarkBootStart() {}
inline void MarkOperationalResumeStart() {}
inline void HandleChipDeviceEvent(const chip::DeviceLayer::ChipDeviceEvent *) {}

#endif // APP_ENABLE_COMMISSIONING_TIMING

} // namespace RafaelCommissioningTiming
