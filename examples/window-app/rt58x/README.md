# RT58x Window Covering Example

A Matter window covering (lift and tilt) for the Rafael Micro RT58x family.

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

The window covering example implements the Window Covering cluster as a
position-aware **tilt blind with lift and tilt** (`Type` = 0x08,
`FeatureMap` = 0x17: Lift, Tilt, position-aware lift and tilt).

Movement is simulated in software: a 500 ms timer walks the current position
towards the target in **10 % steps** and updates `OperationalStatus` while
moving, which is exactly what a controller expects from a real motor. Both lift
and tilt positions are persisted across reboots.

The three application buttons form a small local UI. Because the same buttons
serve lift and tilt, one of them selects which axis the other two move.

Commissioning uses Bluetooth LE to hand over the Thread network credentials; once
the device has joined the Thread network it is reachable over IPv6 and is
controlled with Matter commands from any commissioner (chip-tool, Apple Home,
Google Home, Amazon Alexa, SmartThings ...).

### Data model

| Endpoint | Device type | Server clusters |
| :--- | :--- | :--- |
| 0 | Root Node (0x0016) | Basic Information, Access Control, General / Network Commissioning, General Diagnostics, Thread Network Diagnostics, OTA Requestor, ... |
| 1 | Window Covering (0x0202) | Identify, Groups, Descriptor, Window Covering |

The full definition is in
[../common/window-app.zap](../common/window-app.zap) (`.zap` for the ZAP tool, `.matter` for the
generated data model). See
[ZAP and codegen](../../../docs/zap_and_codegen/zap_intro.md) when changing it.

### Device identifiers

| Field | Value |
| :--- | :--- |
| Vendor ID | `0x1346` (`0xFFF1` when built without `--certs`) |
| Product ID | `0x400E` |
| Product name | `WindowControl` |
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
| 1 | Release: switch the axis the buttons below act on, **LIFT <-> TILT** |
| 2 | Release: step the selected axis up / open (10 %) |
| 3 | Release: step the selected axis down / close (10 %) |

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |

### UART interfaces

| UART | Pins | Baud | Purpose |
| :--- | :--- | :--- | :--- |
| UART0 | TX = GPIO 17, RX = GPIO 16 | 2000000 | Console / debug log |

The console UART is set up in
[examples/platform/rt58x/uart.cpp](../../platform/rt58x/uart.cpp) - the port,
the TX/RX pins and the baud rate all come from the `HOSAL_UART_DEV_DECL(uart0_dev, ...)`
declaration at the top of that file, so change them there to match your board.


The EVK has no motor and no position indicator: the position is visible in the
cluster attributes and in the console log.

```
Window Action move: LIFT -> TILT
Window covering move: up
```

## Trying it out

The examples below use `chip-tool`; every command ends with
`<node-id> <endpoint-id>`, so `1 1` means "node 1, endpoint 1". Any other
controller works the same way.

```bash
# positions are in 0.01 %, so 5000 = 50.00 % closed
chip-tool windowcovering read current-position-lift-percent100ths 1 1
chip-tool windowcovering read current-position-tilt-percent100ths 1 1
chip-tool windowcovering read operational-status 1 1

chip-tool windowcovering up-or-open 1 1
chip-tool windowcovering down-or-close 1 1
chip-tool windowcovering stop-motion 1 1

# go to a position (argument in 0.01 %)
chip-tool windowcovering go-to-lift-percentage 5000 1 1
chip-tool windowcovering go-to-tilt-percentage 2500 1 1

# watch the movement, one report per 500 ms step
chip-tool windowcovering subscribe current-position-lift-percent100ths 1 10 1 1
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
