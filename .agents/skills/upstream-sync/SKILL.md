---
name: upstream-sync
description:
    Upstream sync inventory for this Rafael Micro RT58x Matter SDK fork. Use
    when syncing with official project-chip/connectedhomeip releases to check
    that no Rafael-authored files or in-place modifications to upstream files
    are lost, or when locating Rafael-authored source files (including those
    outside third_party/rafael).
---

# Upstream Sync Inventory

This repo is Rafael Micro's fork of `project-chip/connectedhomeip` with
support for the RT58x SoC family. It syncs with official upstream releases by
**file overlay** (copying release files over; no shared git history). During a
sync, Rafael changes OUTSIDE the well-known Rafael paths
(`third_party/rafael/`, `src/platform/RT58x/`, `examples/*/rt58x/`,
`examples/platform/rt58x/`, `third_party/openthread/openthread-rafael/`) are
at risk of being silently clobbered. Before and after a sync, check the full
inventory below. Three categories: files under `third_party/rafael/`
(obvious), Rafael files **elsewhere in the tree** (easy to miss), and
in-place modifications to upstream files (highest risk).

## Sync procedure: delete first, then copy

Copy-only overlays accumulate garbage.
The fork keeps a trimmed tree. The scope rule that reproduces it, applied to every file `Q` in the new release:

1. Skip `Q` if its level-2 directory under `examples/`, `third_party/`,
   `src/platform/`, `config/` or `credentials/` does not already exist here.
   (Keeps new-in-release vendor ports and example apps out.)
2. Otherwise walk up from `dirname(Q)` to the first directory that existed in
   the *previous* upstream release; copy `Q` only if that directory exists
   here. (Keeps directories this fork deliberately deleted deleted, while
   still letting new subdirectories appear inside kept ones, e.g. new
   `src/app/clusters/*` in a code-driven-cluster migration.)
3. Skip `Q` entirely if it is a root-level file, or lives under `.agents/`
   or `.github/`. These three areas are fork-owned and trimmed at *file*
   level, so the directory rules above cannot express them — take nothing new
   from upstream and let whatever is already here stand.

4. Never let `rsync` touch submodule gitlink paths; set those with
   `git update-index --cacheinfo 160000,<sha>,<path>`. Beware that a later
   `git add -A` rewrites gitlinks back to whatever the worktree has checked
   out — update the submodule checkouts too, or re-set the pins afterwards.

Then re-apply the in-place modifications with a 3-way merge per file
(`git merge-file -p --diff3 <rafael> <prev-upstream> <new-upstream>`), which
is why the tables below record *what* each change is, not just the filename.
## Inside `third_party/rafael/`

| Path | Content |
| ---- | ------- |
| `repo/` | Rafael SDK submodule (`github.com/RafaelMicro/components`): `components/platform/soc/{rt582,rt584}`, `components/hosal`, `components/network` (RT569 RF/FW), FreeRTOS under `repo/third_party/freertos/` |
| `rt58x_sdk.gni` | `rt58x_sdk()` build template; args: `rt58x_sdk_root`, `use_static_sdk_lib`, `use_rafael_thread_lib`, per-board include dirs |
| `rt58x_arm.gni` | ARM toolchain flags per board (arch/cpu/fpu) |
| `rt58x_executable.gni` | `rt58x_executable()` template → `.bin` via objcopy |
| `BUILD.gn` | mbedtls target (`mbedtls_rt58x_config`), syscalls stub, SDK group |
| `BLE_App_Profile/`, `BLE_FOTA/` | Rafael BLE profile and BLE FOTA sources |
| `pch/` | Precompiled-header build (`rt58x_use_pch` arg, `chip_pch.h` header list, `build_pch.py` which recovers the compile flags from `compile_commands.json`, `chip_pch_config`). Anchored on `src/platform/RT58x:RT58x`'s `public_configs`/`public_deps`, so it needs no per-cluster BUILD.gn edits and new upstream `src/app/clusters/*` pick it up automatically. After a sync verify the headers in `chip_pch.h` and the `*_config`/`*_buildconfig` labels in `pch/BUILD.gn` still exist, and that the build reports no `-Winvalid-pch` warnings |
| `rafael_ota_tool.py` | Matter OTA image packaging tool |
| `get_fw_version.py`, `get_git_hash.py` | Build-time version/hash injection |
| `syscalls_stubs.cpp` | newlib syscall stubs |

## Outside `third_party/rafael/` (Rafael-authored, non-obvious)

| Path | Content |
| ---- | ------- |
| `src/platform/RT58x/` | Full CHIP DeviceLayer implementation: `BLEManagerImpl`, `BLEManagerImpl_Multicontrol` + `RafaelMultiControl`, `ThreadStackManagerImpl`, `ConfigurationManagerImpl`, `ConnectivityManagerImpl`, `KeyValueStoreManagerImpl` + `RT58xConfig` (NVM/KVS), `FactoryDataProvider`/`FactoryDataParser`, `OTAImageProcessorImpl`, `PlatformManagerImpl`, `DiagnosticDataProviderImpl`, `Logging`, `SystemTimeSupport`, `CHIPMem-Platform`, platform config headers, `args.gni`, `BUILD.gn` |
| `examples/platform/rt58x/` | Shared example glue: `matter_config.cpp`, `init_rt58x_platform.cpp`, `heap_5_rt58x.c` (FreeRTOS heap_5), `uart.cpp`, `matter_shell.cpp`, `cmd_rafael.cpp` (shell commands), `DTM.cpp`/`DTM_RF.c` (RF direct test mode), `OTAConfig.cpp`, `SubscriptionCallback.cpp`, `FreeRTOSConfig.h`, `ldscript/*.ld`, `project_include/OpenThreadConfig.h` |
| `examples/<app>/rt58x/` | 13 example apps (contact-sensor-app, humidity-sensor-app, light-switch-app, lighting-app, lighting-zigbee, lock-app, occupancy-sensor-app, smart-plug-app, smoke-alarm-app, temperature-measurement-app, thermostat, water-valve-app, window-app), each with `BUILD.gn`, `args.gni`, `src/`, `include/CHIPProjectConfig.h`; `lighting-zigbee` adds `zigbee_src/` |
| `src/lib/shell/streamer_rt58x.cpp`, `src/lib/shell/MainLoopRT58x.cpp` | Matter shell port |
| `third_party/openthread/openthread-rafael/` | Rafael OpenThread submodule (`github.com/RafaelMicro/openthread`): `openthread/` core + `openthread_port/` (incl. `openthread-core-rafael-config.h`) |
| `third_party/openthread/platforms/rt58x/` | OT platform glue: `BUILD.gn`, `system.c` |
| `examples/build_overrides/rt58x_sdk.gni`, `examples/build_overrides/ot_rt58x.gni` | Set `rt58x_sdk_build_root` for example builds |
| `tools/Debugger/OpenOCD/` | `script/openocd_rt58x.sh`, `script/target/{rt58x,rt584}.cfg`, `script/interface/cmsis-dap.cfg`, bundled openocd binaries |
| `.vscode/` | `tasks.json` (build/ZAP/OTA tasks), `launch.json` (cortex-debug), `arm-none-eabi-gdb.sh` (GDB shim) |
| `.devcontainer/RT582_Dockerfile` | Dev container |
| `rafael_docs/` | All Rafael markdown docs |
| `scripts/examples/gn_rt58x_example.sh` | The RT58x build script |
| `examples/Template/` | 44 device-type ZAP template directories (Actuators_*, Appliances_*, Sensors_*, LightApp_*, Switchs_*, HVAC_*, Media_*, Entry_*, Robotic_*) — Rafael-authored, no upstream counterpart |
| `examples/lighting-zigbee/` | Whole app is Rafael-authored, including `lighting-common/src/ColorFormat.cpp` (must stay identical to the `examples/lighting-app` copy) |
## Known in-place modifications to upstream files

These are Rafael changes made directly inside upstream source files — the
highest clobber-risk category, since an upstream sync overwrites the whole
file. Re-apply and re-verify each of these after every sync. (Append new
entries here whenever an upstream file is modified in place.)

### Silent behavior changes (most dangerous — build still succeeds if clobbered)

| File | Rafael change |
| ---- | ------------- |
| `src/app/clusters/level-control/codegen/level-control.cpp` | `scheduleTimerCallbackMs()`: added a `delayMs == 0` early path that invokes `timerCallback()` synchronously instead of going through `SystemLayer().StartTimer()` |
| `src/app/clusters/color-control-server/color-control-server.cpp` | `ColorControlServer::scheduleTimerCallbackMs()`: same `delayMs == 0` synchronous-callback handling |
| `examples/lighting-app/lighting-common/src/ColorFormat.cpp` (identical copy: `examples/lighting-zigbee/lighting-common/src/ColorFormat.cpp` — keep both in sync) | Fixes over upstream: `XYToRgb()` guards `currentY == 0` (upstream divides by zero → NaN passes through `clamp`, and NaN→uint8 cast is UB) and divides by 65536 per spec instead of 65535; `CTToRgb()` guards `ctMireds == 0` and clamps to 0xFEFF; `HsvToRgb()` rewritten for Matter's 0–254 value range (upstream assumed 0–255 with `/43` and `>>8` truncation) using `DivRound` rounding and `uint16_t` intermediates; `rgb` structs zero-initialized |

### Platform registration (build fails loudly if clobbered — still must be re-applied)

| File | Rafael change |
| ---- | ------------- |
| `src/platform/device.gni` | `rafael_board` arg; `rt58x` added to the platform allowlist and `_chip_device_layer = "RT58x"` mapping |
| `src/platform/BUILD.gn` | `rt58x` branches: `CHIP_DEVICE_LAYER_TARGET=RT58x` define, `RT58x` in the device-layer dirs list, `_platform_target = "RT58x"` |
| `src/platform/logging/BUILD.gn` | `rt58x` branch for the platform logging backend |
| `src/system/BUILD.gn` | Imports `//build_overrides/rt58x_sdk.gni`; adds `${rt58x_sdk_build_root}:rt58x_sdk` to `public_deps` for `rt58x` |
| `src/lib/shell/BUILD.gn` | `rt58x` branch adding `MainLoopRT58x.cpp` and `streamer_rt58x.cpp` |
| `.gitmodules` | Registers Rafael submodules: `third_party/openthread/openthread-rafael`, `third_party/rafael/repo/components` |
| `src/app/clusters/thread-network-diagnostics-server/DirectThreadNetworkDiagnosticsProvider.cpp` | 7 calls qualified from `Internal::MapOpenThreadError` to `DeviceLayer::Internal::MapOpenThreadError`. The file sits in `namespace chip::app::Clusters::ThreadNetworkDiagnostics` and relied on `using namespace chip::DeviceLayer;` to resolve the unqualified name; the precompiled header (`third_party/rafael/pch/`) makes `chip::app::Internal` visible first, which hides it. Any upstream file that uses `Internal::` unqualified inside `chip::app` will need the same fix |
