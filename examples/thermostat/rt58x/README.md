# RT58x Thermostat Example

A Matter thermostat for the Rafael Micro RT58x family, with a UART status report for a host MCU.

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

The thermostat example implements the Thermostat cluster: system mode, local
temperature and the heating/cooling setpoints. All of it is driven from the
Matter side - there are no buttons for the thermostat functions on the EVK.

Whenever the mode, the local temperature or a setpoint changes, the application
pushes a **status report frame on UART1** (TX = GPIO 28, RX = GPIO 29, 115200)
so a host MCU can render it on the product's display. The frame is transmit-only
(the example does not parse incoming UART commands):

| Header | Length | Command ID | Payload | Checksum |
| :-: | :-: | :-: | :-: | :-: |
| `FF FC FC FF` | `09` | `01 02` | 7 octets | 1 octet |

The payload is `mode (1)`, `local temperature (2)`, `heating setpoint (2)`,
`cooling setpoint (2)`, all little-endian, temperatures in 0.01 degrees Celsius.
The checksum is the bitwise NOT of the sum of the length, command ID and payload
octets (the header is excluded), matching the convention used by the
[door lock UART protocol](../../lock-app/rt58x/UART%20Command%20Manual.md).

The mode octet uses the application's own numbering:
`0` = Off, `1` = Auto, `2` = other, `3` = Cool, `4` = Heat, `5` = Emergency
heat, `6` = Precooling, `7` = Fan only, `8` = Dry, `9` = Sleep.

Commissioning uses Bluetooth LE to hand over the Thread network credentials; once
the device has joined the Thread network it is reachable over IPv6 and is
controlled with Matter commands from any commissioner (chip-tool, Apple Home,
Google Home, Amazon Alexa, SmartThings ...).

### Data model

| Endpoint | Device type | Server clusters |
| :--- | :--- | :--- |
| 0 | Root Node (0x0016) | Basic Information, Access Control, General / Network Commissioning, General Diagnostics, Thread Network Diagnostics, OTA Requestor, ... |
| 1 | Thermostat (0x0301) | Identify, Groups, Descriptor, Thermostat |

The full definition is in
[../thermostat-common/thermostat.zap](../thermostat-common/thermostat.zap) (`.zap` for the ZAP tool, `.matter` for the
generated data model). See
[ZAP and codegen](../../../docs/zap_and_codegen/zap_intro.md) when changing it.

### Device identifiers

| Field | Value |
| :--- | :--- |
| Vendor ID | `0x1346` (`0xFFF1` when built without `--certs`) |
| Product ID | `0x400E` |
| Product name | `Thermostat` |
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

### LEDs

| GPIO | Meaning |
| :--- | :--- |
| 20 | Commissioning indicator: lit while the device is not commissioned, off once commissioning completed. Also used by the Identify cluster |

### UART interfaces

| UART | Pins | Baud | Purpose |
| :--- | :--- | :--- | :--- |
| UART0 | TX = GPIO 17, RX = GPIO 16 | 2000000 | Console / debug log |
| UART1 | TX = GPIO 28, RX = GPIO 29 | 115200 | Thermostat status report to a host MCU (transmit only) |

The console UART is set up in
[examples/platform/rt58x/uart.cpp](../../platform/rt58x/uart.cpp) - the port,
the TX/RX pins and the baud rate all come from the `HOSAL_UART_DEV_DECL(uart0_dev, ...)`
declaration at the top of that file, so change them there to match your board.

UART1 is declared at the top of [src/AppTask.cpp](src/AppTask.cpp), next to
`AppTask::UpdateThermoStatUI()` which builds and sends the status frame.

## Trying it out

The examples below use `chip-tool`; every command ends with
`<node-id> <endpoint-id>`, so `1 1` means "node 1, endpoint 1". Any other
controller works the same way.

```bash
chip-tool thermostat read system-mode 1 1
chip-tool thermostat read local-temperature 1 1
chip-tool thermostat read occupied-heating-setpoint 1 1
chip-tool thermostat read occupied-cooling-setpoint 1 1

# 4 = Heat, 3 = Cool, 1 = Auto, 0 = Off
chip-tool thermostat write system-mode 4 1 1

# setpoints are in 0.01 degrees Celsius
chip-tool thermostat write occupied-heating-setpoint 2500 1 1
chip-tool thermostat write occupied-cooling-setpoint 2800 1 1

# raise the heating setpoint by 2.0 degrees Celsius (mode 0 = heat, amount in 0.1 C steps)
chip-tool thermostat setpoint-raise-lower 0 20 1 1
```

Every change is echoed on the console together with the UART frame that was
sent to the host MCU:

```
CoolingSetpoint 2800
Thermostat Status - M:4 T:2000'C H:2500'C C:2800'C
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
