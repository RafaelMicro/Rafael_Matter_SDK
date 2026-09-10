# RT58x Occupancy Sensor Example

A Matter occupancy sensor for the Rafael Micro RT58x family.

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

The occupancy sensor publishes the Occupancy Sensing cluster, implemented with
the code-driven cluster server (the cluster owns `FeatureMap`,
`OccupancySensorType` and the `HoldTime` countdown; the application only feeds
it the sensor state). On the EVK the detector is simulated with a button:
holding it down means "occupancy detected", releasing it ends the detection and
starts the `HoldTime` countdown, after which the cluster clears the `Occupancy`
attribute by itself.

`HoldTime` defaults to **10 s** and can be written by a controller within the
supported range of **1 - 300 s**.

The Power Source cluster carries a **simulated battery**: the remaining
percentage starts at 100 %, drops by 5 % every 10 minutes and wraps back to
100 %.

Commissioning uses Bluetooth LE to hand over the Thread network credentials; once
the device has joined the Thread network it is reachable over IPv6 and is
controlled with Matter commands from any commissioner (chip-tool, Apple Home,
Google Home, Amazon Alexa, SmartThings ...).

### Data model

| Endpoint | Device type | Server clusters |
| :--- | :--- | :--- |
| 0 | Root Node (0x0016) | Basic Information, Access Control, General / Network Commissioning, General Diagnostics, Thread Network Diagnostics, OTA Requestor, ... |
| 1 | Occupancy Sensor (0x0107) | Identify, Groups, Descriptor, Power Source, Occupancy Sensing |

The full definition is in
[../occupancy-sensor-common/occupancy-sensor-app.zap](../occupancy-sensor-common/occupancy-sensor-app.zap) (`.zap` for the ZAP tool, `.matter` for the
generated data model). See
[ZAP and codegen](../../../docs/zap_and_codegen/zap_intro.md) when changing it.

### Device identifiers

| Field | Value |
| :--- | :--- |
| Vendor ID | `0x1346` (`0xFFF1` when built without `--certs`) |
| Product ID | `0x400E` |
| Product name | `OccupancySensor` |
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
| 1 | Held: occupancy detected (`Occupancy` = 1). Released: detection ends, the cluster clears `Occupancy` after `HoldTime` |

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |
| 21 | Lit while occupancy is reported, goes off when `HoldTime` expires |

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
chip-tool occupancysensing read occupancy 1 1
chip-tool occupancysensing read hold-time 1 1

# shorten / lengthen the hold time (1 - 300 s)
chip-tool occupancysensing write hold-time 30 1 1

# watch detection and the hold-time expiry
chip-tool occupancysensing subscribe occupancy 1 10 1 1
```

The console log follows the state machine:

```
Occupancy detected
Detection ended - will clear in 10 s
HoldTime expired, occupancy cleared
```

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
