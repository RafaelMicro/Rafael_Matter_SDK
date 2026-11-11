# RT58x Smoke / CO Alarm Example

A Matter smoke and CO alarm for the Rafael Micro RT58x family.

> Setting up the build environment, building, flashing, reading the console log
> and commissioning are the same for every RT58x example and are documented once
> under [rafael_docs](../../../rafael_docs): start with
> [Environment Setup Overview](../../../rafael_docs/matter_env_setup.md), then
> [Flash Application](../../../rafael_docs/flash_application.md) and
> [Matter over Thread Overview](../../../rafael_docs/matter_over_thread_overview.md).
> This guide only covers what is specific to this example.

- [Introduction](#introduction)
- [User interface](#user-interface)
- [Trying it out](#trying-it-out)
- [Build options](#build-options)
- [Related documents](#related-documents)

## Introduction

The smoke alarm example implements the Smoke CO Alarm cluster with both the
**smoke** and the **CO** features enabled. The cluster is code-driven and owned
by the application ([src/SmokeManager.cpp](src/SmokeManager.cpp)), which also
maintains the `ExpressedState` priority order required by the specification.

Two ways to exercise the alarm are provided:

- **Buttons** - simulate smoke or CO detection: the warning is raised while the
  button is held and cleared on release.
- **Test event triggers** - the full set of `SmokeCOTrigger` values (smoke, CO,
  battery, malfunction, end of life, interconnect, contamination, sensitivity
  and silence) can be injected remotely, which is how the certification test
  harness drives the device.

A self test keeps `TestInProgress` set for 10 seconds and then clears it.

Commissioning uses Bluetooth LE to hand over the Thread network credentials; once
the device has joined the Thread network it is reachable over IPv6 and is
controlled with Matter commands from any commissioner (chip-tool, Apple Home,
Google Home, Amazon Alexa, SmartThings ...).

### Data model

| Endpoint | Device type | Server clusters |
| :--- | :--- | :--- |
| 0 | Root Node (0x0016) | Basic Information, Access Control, General / Network Commissioning, General Diagnostics, Thread Network Diagnostics, OTA Requestor, ... |
| 1 | Smoke CO Alarm (0x0076) | Identify, Descriptor, Power Source, Smoke CO Alarm |

The full definition is in
[../smoke-alarm-common/smoke-alarm-app.zap](../smoke-alarm-common/smoke-alarm-app.zap) (`.zap` for the ZAP tool, `.matter` for the
generated data model). See
[ZAP and codegen](../../../docs/zap_and_codegen/zap_intro.md) when changing it.

### Device identifiers

| Field | Value |
| :--- | :--- |
| Vendor ID | `0x1346` (`0xFFF1` when built without `--certs`) |
| Product ID | `0x400E` |
| Product name | `SmockCOAlarm` |
| Setup passcode / discriminator | `20202021` / `3840` (test values) |

These are the built-in test values from
[include/CHIPProjectConfig.h](include/CHIPProjectConfig.h); see
[Rafael Factory Data](../../../rafael_docs/Rafael_Factory_Data.md) for provisioning production
identifiers and certificates.

## User interface

The buttons on GPIO 0 - GPIO 4 are configured as inputs with an internal 100 k
pull-up and are therefore **active low**: pressing a button pulls the pin to
ground. The indicator LEDs are **active low** as well - the application drives
the pin low to light the LED.

### Buttons

| GPIO | Action |
| :--- | :--- |
| 0 | Hold >= 6 s: factory reset (release earlier to cancel) |
| 1 | Hold: set `SmokeState` to **Warning**; release: back to **Normal** |
| 2 | Hold: set `COState` to **Warning**; release: back to **Normal** |

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |
| 21 | Blinks (100 ms) while an alarm is expressed, off when the alarm state is normal |

### UART interfaces

| UART | Pins | Baud | Purpose |
| :--- | :--- | :--- | :--- |
| UART0 | TX = GPIO 17, RX = GPIO 16 | 2000000 | Console / debug log |

The console UART is set up in
[examples/platform/rt58x/uart.cpp](../../platform/rt58x/uart.cpp) - the port,
the TX/RX pins and the baud rate all come from the `HOSAL_UART_DEV_DECL(uart0_dev, ...)`
declaration at the top of that file, so change them there to match your board.

## Trying it out

The examples below use `chip-tool`; every command ends with
`<node-id> <endpoint-id>`, so `1 1` means "node 1, endpoint 1". Any other
controller works the same way.

```bash
chip-tool smokecoalarm read expressed-state 1 1
chip-tool smokecoalarm read smoke-state 1 1
chip-tool smokecoalarm read co-state 1 1
chip-tool smokecoalarm read test-in-progress 1 1

# run a self test remotely
chip-tool smokecoalarm self-test-request 1 1
```

### Test event triggers

The enable key of this example is
`00112233445566778899aabbccddeeff` (test value, see
[src/AppTask.cpp](src/AppTask.cpp)). Trigger values are the `SmokeCOTrigger`
constants, for example:

```bash
# force a smoke warning, then a critical smoke alarm, then clear it
chip-tool generaldiagnostics test-event-trigger hex:00112233445566778899aabbccddeeff 0x005C000000000090 1 0
chip-tool generaldiagnostics test-event-trigger hex:00112233445566778899aabbccddeeff 0x005C00000000009C 1 0
chip-tool generaldiagnostics test-event-trigger hex:00112233445566778899aabbccddeeff 0x005C0000000000A0 1 0
```

| Trigger | Value |
| :--- | :--- |
| Force smoke warning / critical | `0x005C000000000090` / `0x005C00000000009C` |
| Force CO warning / critical | `0x005C000000000091` / `0x005C00000000009D`|
| Force low battery warning / critical | `0x005C000000000095` / `0x005C00000000009E` |
| Force malfunction | `0x005C000000000093` |
| Force end of life | `0x005C00000000009A` |
| Clear smoke / CO | `0x005C0000000000A0` / `0x005C0000000000A1` |
| Clear battery / malfunction / end of life | `0x005C0000000000A5` / `0x005C0000000000A3` / `0x005C0000000000AA` |

The complete list is in
[SmokeCOTestEventTriggerHandler.h](../../../src/app/clusters/smoke-co-alarm-server/SmokeCOTestEventTriggerHandler.h).

## Build options

The GN arguments of this example live in [args.gni](args.gni); the ones worth
knowing about:

| Argument | Default | Meaning |
| :--- | :--- | :--- |
| `chip_enable_ota_requestor` | `true` | Matter OTA software update support, see [Rafael Software Update](../../../rafael_docs/Rafael_Software_Update.md) |
| `chip_build_libshell` | `false` | Matter shell on the console UART |
| `chip_detail_logging` | `false` | Verbose (detail level) Matter logs |
| `raf_enable_multi_control` | `false` | Rafael BLE multi-control channel, see [BLE Multi Control Guide](../../../rafael_docs/Rafael_BLE_Multi_Control_Guide.md) |
| `chip_openthread_ftd` | `true` | Full Thread Device; the `--sed`/`--ssed` flags of the build script turn this off |

## Related documents

- [Environment Setup Overview](../../../rafael_docs/matter_env_setup.md) - toolchain and SDK setup
- [Linux / macOS setup](../../../rafael_docs/linux_macos_setup.md), [Windows (VSCode + WSL2) setup](../../../rafael_docs/windows_setup.md)
- [Flash Application](../../../rafael_docs/flash_application.md) - ISP tool procedure
- [Matter over Thread Overview](../../../rafael_docs/matter_over_thread_overview.md) - border router and controller setup
- [Matter Application Note](../../../rafael_docs/Matter_Application_Note.md) - pairing with Apple Home / Google Home
- [Rafael Software Update](../../../rafael_docs/Rafael_Software_Update.md) - building and delivering an OTA image
- [Rafael Factory Data](../../../rafael_docs/Rafael_Factory_Data.md) - device attestation credentials
- [Flash Layout Customer Guide](../../../rafael_docs/Flash_Layout_Customer_Guide.md) - flash partitioning per chip
- [Rafael App File Structure](../../../rafael_docs/Rafael_App_File_Structure.md) - what each file of an example does
