# RT58x Lighting Example (Matter + Zigbee dual mode)

A dual-protocol light: the same device joins either a Matter/Thread fabric or a Zigbee network. RT584HA4 only.

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

This example is the [lighting example](../../lighting-app/rt58x/README.md)
extended with a **Zigbee** stack (ZBOSS) next to the Matter/Thread stack. The
radio runs in multi-protocol mode (`HOSAL_RF_MODE_MULTI_PROTOCOL`) and at boot
both stacks are started, so the device can be commissioned into **either**
ecosystem. Whichever network claims the device first wins:

- Matter commissioning completes -> the Zigbee task is suspended and `Matter` is
  stored as the active protocol.
- The device joins a Zigbee network -> the Matter task is suspended, the Thread
  provisioning is erased and `Zigbee` is stored as the active protocol.

The selected protocol is persisted in flash under the key `prot`, so the choice
survives a reboot; the log prints it at boot (`Protocol: None|Matter|Zigbee`).
A factory reset (button on GPIO 0, >= 6 s) resets the *active* protocol, and
resets the stored selection back to `None` when no protocol has been selected
yet.

> **Zigbee is only supported on the RT584HA4** (4 MB flash, it has the dedicated
> Zigbee data partition). The build asserts on any other board.

The Rafael BLE multi-control channel is enabled in this example
(`raf_enable_multi_control = true`), which allows a BLE client to read the
device information and drive clusters over BLE - see
[BLE Multi Control Guide](../../../rafael_docs/Rafael_BLE_Multi_Control_Guide.md).
The Matter shell is enabled as well (`chip_build_libshell = true`).

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
| 0 | Hold >= 6 s: factory reset of the active protocol (Matter or Zigbee); with no protocol selected yet it clears the stored settings and reboots |

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |
| RGB LED | The light itself. RT584HA4 PWM pins: red = GPIO 8, green = GPIO 9, blue = GPIO 7 |

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

### As a Matter light

Identical to the [lighting example](../../lighting-app/rt58x/README.md):

```bash
chip-tool pairing ble-thread 1 hex:<operationalDataset> 20202021 3840
chip-tool onoff toggle 1 1
chip-tool levelcontrol move-to-level 128 10 0 0 1 1
chip-tool colorcontrol move-to-hue-and-saturation 240 254 0 0 0 1 1
```

After commissioning completes the log confirms that Zigbee has been shut down:

```
Deleting Zigbee task
```

### As a Zigbee light

Open the network on a Zigbee coordinator and let the device perform BDB network
steering (it retries automatically while no protocol has been selected). Once it
joins, the log reports the network it attached to:

```
ZigBee APP joined
PAN ID: 1A62, Short Addr: 4E7B, Channel: 15
Deleting Matter task
```

From then on the light answers Zigbee ZCL On/Off, Level Control and Color
Control commands from the coordinator.

## Build options

The GN arguments of this example live in [args.gni](args.gni); the ones worth
knowing about:

| Argument | Default | Meaning |
| :--- | :--- | :--- |
| `chip_enable_ota_requestor` | `true` | Matter OTA software update support, see [Rafael Software Update](../../../rafael_docs/Rafael_Software_Update.md) |
| `chip_build_libshell` | `true` | Matter shell on the console UART |
| `chip_detail_logging` | `false` | Verbose (detail level) Matter logs |
| `raf_enable_multi_control` | `true` | Rafael BLE multi-control channel, see [BLE Multi Control Guide](../../../rafael_docs/Rafael_BLE_Multi_Control_Guide.md) |
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
- [BLE Multi Control Guide](../../../rafael_docs/Rafael_BLE_Multi_Control_Guide.md) - controlling the device over BLE
- [Lighting example](../../lighting-app/rt58x/README.md) - the single-protocol version of this application
