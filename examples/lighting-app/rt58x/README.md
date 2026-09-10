# RT58x Lighting Example

A Matter extended color light for the Rafael Micro RT58x family.

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

The lighting example is an **Extended Color Light**: it supports on/off,
dimming, hue/saturation, XY color and color temperature, plus groups and scenes.
The RGB LED of the EVK is driven by three 16 kHz PWM channels, so brightness and
color changes are directly visible on the board.

| Signal | RT583 | RT584L / RT584H / RT584HA4 |
| :--- | :--- | :--- |
| PWM red | GPIO 22 | GPIO 8 |
| PWM green | GPIO 23 | GPIO 9 |
| PWM blue | GPIO 21 | GPIO 7 |

The PWM duty is inverted for the EVK wiring (`calc_duty()` in
[src/init_device_environment.cpp](src/init_device_environment.cpp)); adjust that
function when adapting the example to a different LED driver.

This is the example used throughout the Rafael documentation, so it is the best
starting point for a new product.

Commissioning uses Bluetooth LE to hand over the Thread network credentials; once
the device has joined the Thread network it is reachable over IPv6 and is
controlled with Matter commands from any commissioner (chip-tool, Apple Home,
Google Home, Amazon Alexa, SmartThings ...).

### Data model

| Endpoint | Device type | Server clusters |
| :--- | :--- | :--- |
| 0 | Root Node (0x0016) | Basic Information, Access Control, General / Network Commissioning, General Diagnostics, Thread Network Diagnostics, OTA Requestor, ... |
| 1 | Extended Color Light (0x010D) | Identify, Groups, Scenes Management, On/Off, Level Control, Color Control, Descriptor |

The full definition is in
[../lighting-common/lighting-app.zap](../lighting-common/lighting-app.zap) (`.zap` for the ZAP tool, `.matter` for the
generated data model). See
[ZAP and codegen](../../../docs/zap_and_codegen/zap_intro.md) when changing it.

### Device identifiers

| Field | Value |
| :--- | :--- |
| Vendor ID | `0x1346` (`0xFFF1` when built without `--certs`) |
| Product ID | `0x400E` |
| Product name | `DimmableLight` |
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
| 1 | Release: toggle the light on/off (the On/Off cluster is updated, so controllers see the change) |

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |
| RGB LED | The light itself - on/off, level and color are rendered on the PWM channels listed above |

### UART interfaces

| UART | Pins | Baud | Purpose |
| :--- | :--- | :--- | :--- |
| UART0 | TX = GPIO 17, RX = GPIO 16 | 2000000 | Console / debug log |

The console UART is set up in
[examples/platform/rt58x/uart.cpp](../../platform/rt58x/uart.cpp) - the port,
the TX/RX pins and the baud rate all come from the `HOSAL_UART_DEV_DECL(uart0_dev, ...)`
declaration at the top of that file, so change them there to match your board.


### Group (multicast) control

The light also accepts group commands, which is how several lights are switched
at once. Refer to the _Configuring the server side for Group Commands_ and
_Using the Client to Send Group (Multicast) Matter Commands_ sections of the
[chip-tool documentation](https://github.com/project-chip/connectedhomeip/blob/master/examples/chip-tool/README.md).

## Trying it out

The examples below use `chip-tool`; every command ends with
`<node-id> <endpoint-id>`, so `1 1` means "node 1, endpoint 1". Any other
controller works the same way.

```bash
chip-tool onoff on 1 1
chip-tool onoff off 1 1
chip-tool onoff toggle 1 1
chip-tool onoff read on-off 1 1

# dim to ~50 % with a 1 s transition (level, transitionTime, optionsMask, optionsOverride)
chip-tool levelcontrol move-to-level 128 10 0 0 1 1
chip-tool levelcontrol read current-level 1 1

# color: hue/saturation and color temperature (mireds)
chip-tool colorcontrol move-to-hue-and-saturation 240 254 0 0 0 1 1
chip-tool colorcontrol move-to-color-temperature 370 0 0 0 1 1
```

The console log follows every action:

```
LightMgr:ON: OFF->ON
LightMgr:Level: 128
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
