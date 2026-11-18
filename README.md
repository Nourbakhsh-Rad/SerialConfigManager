# SerialConfigManager

![Version](https://img.shields.io/badge/version-2.5.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-00979D?logo=arduino)](https://www.arduino.cc/)
[![ESP32](https://img.shields.io/badge/ESP32-Supported-E7352C)](https://www.espressif.com/en/products/socs/esp32)
[![ESP8266](https://img.shields.io/badge/ESP8266-Supported-000000)](https://www.espressif.com/en/products/socs/esp8266)

Unified UART and RS-485 configuration manager for ESP32 and ESP8266.

**Version:** 2.5.0  
**License:** MIT

[![CI](https://github.com/Nourbakhsh-Rad/SerialConfigManager/actions/workflows/ci.yml/badge.svg)](https://github.com/Nourbakhsh-Rad/SerialConfigManager/actions/workflows/ci.yml)

---

## 📑 Table of Contents

1. [Introduction](#introduction)
2. [Features](#features)
3. [Installation & Setup](#installation--setup)
4. [Data Structures](#data-structures)
5. [Constants & Macros](#constants--macros)
6. [API Reference](#api-reference)
   - [Class Management](#class-management)
   - [Port Configuration](#port-configuration)
   - [Getter Functions](#getter-functions)
   - [RS-485 Management](#rs-485-management)
   - [Save & Load](#save--load)
   - [Utility Functions](#utility-functions)
7. [Practical Examples](#practical-examples)
8. [Troubleshooting & Errors](#troubleshooting--errors)
9. [ESP32 & ESP8266 Specifics](#esp32--esp8266-specifics)
10. [Changelog](#changelog)

---

## Introduction

**SerialConfigManager** is a modular and powerful library for managing serial (UART) ports on ESP32 and ESP8266 microcontrollers. It allows you to:

- Configure each serial port independently (baud rate, data bits, parity, stop bits).
- Assign custom RX/TX pins for each port (on ESP32).
- Support RS-485 with automatic DE/RE pin control (half-duplex and full-duplex).
- Persist settings to the file system (SPIFFS, LittleFS, or FFat) as JSON, or to EEPROM (`-DSCM_STORAGE_TYPE=SCM_STORAGE_EEPROM`).
- Export/import settings as JSON strings.

This library is ideal for industrial projects, automation, long-distance communication, and any application requiring flexible serial configuration.

---

## Features

- ✅ **Unified API** for both ports (`SER0` and `SER1`)
- ✅ **RS-485 support** with automatic DE/RE control and logic inversion
- ✅ **Persistent storage** in LittleFS/SPIFFS/FFat
- ✅ **Full input validation** (baud rate, data bits, etc.)
- ✅ **Error management** with last error storage
- ✅ **Individual parameter getters** for easy inspection
- ✅ **Timeout support** in RS-485 writes to prevent blocking
- ✅ **ESP8266 compatibility** (with limitations on Serial1)
- ✅ **JSON export/import** for external systems
- ✅ **Debug output** with `printConfig()`

---

## Installation & Setup

### 1. Obtaining the Library

**Arduino IDE:** Library Manager → search "SerialConfigManager" → Install
(or "Add .ZIP Library..." if installing manually).

**PlatformIO:** add to `platformio.ini`:
```ini
lib_deps = https://github.com/Nourbakhsh-Rad/SerialConfigManager.git#v2.5.0
```

**Manual install:** copy the whole folder (including `library.properties`
and `src/`) into your Arduino `libraries/` directory, then:

```cpp
#include <SerialConfigManager.h>
```

Ready-to-run sketches are in [`examples/`](examples/) — `BasicUsage`,
`RS485_HalfDuplex`, `JsonExportImport`, `StructBasedConfig`, and
`AutoDirectionSend` — reachable from the Arduino IDE
via **File → Examples → SerialConfigManager**.

### 2. Choosing the File System

Define the macro `SCM_FS_MODE` to select the file system:

```cpp
#define SCM_FS_MODE 1   // 0=SPIFFS, 1=LittleFS (default), 2=FFat (ESP32 only)
#include "SerialConfigManager.h"
```

### 3. Initialization

```cpp
SerialConfigManager scm;

void setup() {
    Serial.begin(115200);
    
    // true = format on mount failure, true = let begin() mount the
    // filesystem itself (pass false here only if you already called
    // SCM_FS.begin() yourself earlier)
    if (!scm.begin(true, true)) {
        Serial.println("Error: " + scm.getLastError());
        return;
    }
}
```

---

## Data Structures

### `SerialConfigs`

This structure holds all settings for a serial port.

| Field | Type | Description |
|-------|------|-------------|
| `baudRate` | `uint32_t` | Baud rate (e.g., 115200) |
| `dataBits` | `uint8_t` | Data bits (5–8) |
| `parity` | `uint8_t` | Parity: 0=None, 1=Odd, 2=Even |
| `stopBits` | `uint8_t` | Stop bits: 1 or 2 |
| `enabled` | `bool` | Enable/disable the port |
| `rxEnabled` | `bool` | Receiver enable (limited support) |
| `txEnabled` | `bool` | Transmitter enable (limited support) |
| `rxPin` | `int8_t` | RX pin (-1 = default) |
| `txPin` | `int8_t` | TX pin (-1 = default) |
| `dePin` | `int8_t` | DE pin for RS-485 (-1 = not used) |
| `rePin` | `int8_t` | RE pin for RS-485 (-1 = not used) |
| `rs485Mode` | `uint8_t` | RS-485 mode (NONE, HALF_DUPLEX, FULL_DUPLEX) |
| `rs485Invert` | `bool` | Invert DE/RE logic |

---

## Constants & Macros

| Name | Value | Description |
|------|-------|-------------|
| `SER0` | 0 | Serial port 0 (Serial) |
| `SER1` | 1 | Serial port 1 (Serial1) |
| `SCM_RS485_NONE` | 0 | No RS-485 |
| `SCM_RS485_HALF_DUPLEX` | 1 | Half-duplex (DE/RE controlled) |
| `SCM_RS485_FULL_DUPLEX` | 2 | Full-duplex (DE/RE always active) |
| `SCM_DEFAULT_BAUD` | 115200 | Default baud rate |
| `SCM_DEFAULT_DATA_BITS` | 8 | Default data bits |
| `SCM_DEFAULT_PARITY` | 0 | No parity |
| `SCM_DEFAULT_STOP_BITS` | 1 | One stop bit |

---

## API Reference

### Class Management

#### `bool begin(bool formatOnFail = false, bool initStorage = false)`
Initializes the manager and loads any saved configuration. `initStorage`
controls whether `begin()` itself performs the storage backend's setup
(`SCM_FS.begin()` / `EEPROM.begin()`) — **not** whether storage is used at
all:
- `initStorage = true`: `begin()` mounts the filesystem (or inits EEPROM)
  itself. Use this when `begin()` is the only place your sketch sets up
  storage.
- `initStorage = false`: you already called `SCM_FS.begin()` /
  `EEPROM.begin()` yourself earlier. `begin()` trusts that storage is
  ready, skips re-initializing it, and goes straight to loading any saved
  config from it. ⚠️ This is **not** an "in-memory only" mode — passing
  `false` when storage was never actually initialized (by you or by
  `begin()`) is undefined behavior.

Either way, if no saved config is found (or loading it fails), defaults
are used and written back to storage.

#### `void end()`
Marks the manager as not initialized. It does not touch the hardware UARTs or the file system.

#### `bool isInitialized() const`
Returns whether the manager was successfully initialized.

#### `String getLastError() const`
Returns the last error message.

---

### Port Configuration

#### `bool setSerialConfig(uint8_t port, const SerialConfigs& config)`
Sets the full configuration for a port using a `SerialConfigs` structure.

#### `SerialConfigs getSerialConfig(uint8_t port) const`
Returns the current configuration as a `SerialConfigs` structure.

#### `bool applySerialConfig(uint8_t port)`
Applies the current configuration to the hardware (restarts the port).

#### Individual Setters:
- `setSerialBaudRate(port, baud)`
- `setSerialDataBits(port, bits)`
- `setSerialParity(port, parity)`
- `setSerialStopBits(port, bits)`
- `setSerialPins(port, rxPin, txPin)`
- `enableSerial(port, enable)`
- `setSerialRxTx(port, rxEnable, txEnable)`

---

### Getter Functions

| Function | Return Type |
|----------|-------------|
| `getBaudRate(port)` | `uint32_t` |
| `getDataBits(port)` | `uint8_t` |
| `getParity(port)` | `uint8_t` |
| `getStopBits(port)` | `uint8_t` |
| `isEnabled(port)` | `bool` |
| `isRxEnabled(port)` | `bool` |
| `isTxEnabled(port)` | `bool` |
| `getRxPin(port)` | `int8_t` |
| `getTxPin(port)` | `int8_t` |
| `getDePin(port)` | `int8_t` |
| `getRePin(port)` | `int8_t` |
| `getRS485Mode(port)` | `uint8_t` |
| `isRS485Inverted(port)` | `bool` |
| `isRS485Enabled(port)` | `bool` |
| `isTransmitMode(port)` | `bool` (current RS‑485 direction) |
| `isSerialActive(port)` | `bool` (whether the port is running) |

---

### RS-485 Management

#### `bool setRS485Pins(uint8_t port, int8_t dePin, int8_t rePin)`
Sets the DE and RE pins.

#### `bool setRS485Mode(uint8_t port, uint8_t mode)`
Sets the RS-485 mode (NONE, HALF_DUPLEX, FULL_DUPLEX).

#### `bool setRS485Invert(uint8_t port, bool invert)`
Inverts DE/RE logic (`true` = active low).

#### `bool enableRS485(uint8_t port, bool enable)`
Enables/disables RS‑485 (equivalent to setting mode to HALF_DUPLEX or NONE).

#### `void setTransmitMode(uint8_t port)`
Manually sets transmit mode (half-duplex only).

#### `void setReceiveMode(uint8_t port)`
Manually sets receive mode (half-duplex only).

#### `bool writeSerial(uint8_t port, const uint8_t* data, size_t len, unsigned long timeoutMs = 1000)`
(`writeRS485()` is an identical alias with the same overloads.)
**Recommended, universal send helper.** Call this for all outgoing writes
regardless of whether `port` is a plain UART or wired for RS-485 — you never
need to check `isRS485Enabled()` yourself. Only true RS-485 half-duplex
actually needs direction switching (DE/RE → transmit → write → wait for
transmission to finish → DE/RE → receive); for a plain port or a
full-duplex RS-485 port, none of that happens, so the call returns as soon
as the data is queued — no slower than a raw `HardwareSerial::write()`.

#### Overloads:
- `writeSerial(port, String& data, timeoutMs)`
- `writeSerial(port, const char* data, timeoutMs)` — length is measured for you via `strlen()`
- `writeSerial(port, char c, timeoutMs)`

---

### Save & Load

#### `bool saveConfig()`
Saves current settings to the configured storage backend.
The storage must have been initialized by begin() or by the application.

#### `bool loadConfig()`
Loads settings from the file.

#### `bool resetToDefaults(bool save = false)`
Resets both ports to factory defaults (Serial0 = 115200 8N1, Serial1 = 9600 8N1).
Pass `true` to also persist the defaults; with the default `false` only the in-memory
configuration changes.

#### `bool deleteConfigFile()`
Deletes the configuration file.

#### `bool configFileExists() const`
Checks if the configuration file exists.

---

### Utility Functions

#### `String configToJson() const`
Exports settings as a JSON string.

#### `bool parseJsonConfig(const String& json)`
Imports settings from a JSON string (in-memory only — call `saveConfig()`
afterwards to persist). Every field is validated; if anything is invalid,
the whole import is rejected (returns `false`, check `getLastError()`) and
the previous in-memory configuration is left untouched — a malformed or
malicious payload can never reach `applySerialConfig()`.

#### `static const char* getVersion()`
Returns the library release version string (matches the `**Version:**` line at the top of this README / `SCM_LIB_VERSION`).

#### `void printConfig(Stream& output = Serial)`
Prints settings for both ports.

#### `void printConfig(uint8_t port, Stream& output = Serial)`
Prints settings for a specific port.

#### Static Validation Functions:
- `isValidBaudRate(baud)`
- `isValidDataBits(bits)`
- `isValidParity(parity)`
- `isValidStopBits(bits)`
- `isValidPort(port)`
- `isValidRS485Mode(mode)`

---

## Practical Examples

### Example 1: Simple Configuration & Apply

```cpp
#include "SerialConfigManager.h"
SerialConfigManager scm;

void setup() {
    Serial.begin(115200);
    scm.begin(true, true);

    // Set Serial0 to 9600, 8N1
    scm.setSerialBaudRate(SER0, 9600);
    scm.setSerialDataBits(SER0, 8);
    scm.setSerialParity(SER0, 0);
    scm.setSerialStopBits(SER0, 1);
    scm.applySerialConfig(SER0);

    Serial.println("Configuration applied.");
}
```

### Example 2: RS-485 Half-Duplex with DE/RE

```cpp
// Configure Serial1 for RS‑485
scm.setSerialBaudRate(SER1, 115200);
scm.setSerialDataBits(SER1, 8);
scm.setSerialParity(SER1, 0);
scm.setSerialStopBits(SER1, 1);
scm.setRS485Pins(SER1, 18, 19);   // DE=GPIO18, RE=GPIO19
scm.setRS485Mode(SER1, SCM_RS485_HALF_DUPLEX);
scm.applySerialConfig(SER1);

// Send data with 500 ms timeout — writeSerial() is the same call
// whether or not RS-485 is enabled; writeRS485() is an equivalent alias.
if (scm.writeSerial(SER1, "Hello RS-485!", 500)) {
    Serial.println("Send successful");
} else {
    Serial.println("Send failed");
}
```

### Example 3: Inspecting Current Settings

```cpp
void printPortStatus(uint8_t port) {
    Serial.printf("Port %d:\n", port);
    Serial.printf("  Baud: %lu\n", scm.getBaudRate(port));
    Serial.printf("  Data Bits: %d\n", scm.getDataBits(port));
    Serial.printf("  Parity: %d\n", scm.getParity(port));
    Serial.printf("  Stop Bits: %d\n", scm.getStopBits(port));
    Serial.printf("  RS485 Mode: %d\n", scm.getRS485Mode(port));
    Serial.printf("  Transmit Mode: %s\n", scm.isTransmitMode(port) ? "Yes" : "No");
}
```

### Example 4: Save & Restore from File

```cpp
// Save settings
if (scm.saveConfig()) {
    Serial.println("Settings saved.");
}

// Later, reload (e.g., after reset)
scm.loadConfig();
scm.applySerialConfig(SER0);
scm.applySerialConfig(SER1);
```

### Example 5: JSON Export/Import

```cpp
String jsonConfig = scm.configToJson();
Serial.println(jsonConfig);

// Send jsonConfig to a server or SD card

// Later, receive and apply:
if (scm.parseJsonConfig(receivedJson)) {
    scm.applySerialConfig(SER0);
    scm.applySerialConfig(SER1);
}
```

---

## Troubleshooting & Errors

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `Invalid baud rate` | Unsupported baud rate | Use one of the predefined valid rates. |
| `Invalid port number` | Wrong port ID | Only `SER0` or `SER1` are accepted. |
| `FS unavailable; settings will not persist` | FS mount failure | Try `formatOnFail=true`, check the partition scheme / free space. |
| `Failed to parse config JSON; using defaults` | Stored config unreadable | Defaults were restored (and saved); re-apply your settings. |
| `Failed to parse config JSON` | Malformed JSON | Use `configToJson()` as a template. |

### Debugging Tips

- Always print `getLastError()` after a failed operation.
- Use `printConfig()` to inspect current settings.
- For RS-485, check `isTransmitMode()` to know the current direction.

### Known Limitations

- **JSON parser is intentionally lightweight, not a full JSON implementation.**
  `configToJson()`/`parseJsonConfig()` (and the FS-backed save/load that use
  them) handle the flat, fixed shape this library itself produces, but don't
  implement the full JSON spec — no Unicode escape sequences, and values are
  matched by key name anywhere in the string rather than by strict object
  scoping. Round-tripping your own exported JSON is reliable; hand-editing
  or accepting arbitrary third-party JSON is not recommended.
- **`rxEnabled`/`txEnabled` are a soft limitation, not a true hardware
  disable.** They only take effect for a *custom* (non-default) pin via
  `pinMode(pin, INPUT)`, which floats the pin but doesn't detach it from the
  UART peripheral at the hardware level.
- **`writeSerial()`/`writeRS485()`'s `timeoutMs` doesn't cover the final
  flush.** For half-duplex RS-485, the wait for "transmission physically
  finished" uses `HardwareSerial::flush()`, which has no timeout parameter
  of its own — a genuinely stuck UART could still block past `timeoutMs` at
  that step.
- **Serial0 is usually your console.** Changing SER0's baud rate/parity also changes
  the speed of `Serial.print()`; keep it equal to your Serial Monitor setting (the
  examples do this).
- **ESP32: always set explicit pins for `SER1`.** On several ESP32 boards/cores the
  default `Serial1` pins are wired to the flash chip. Use `setSerialPins(SER1, rx, tx)`
  with pins that exist on your board. A pin value of `-1` for just one of RX/TX keeps
  the platform default for that one.
- **ESP32 with "USB CDC On Boot":** `SER0` maps to the real hardware UART0 (`Serial0`),
  not the USB console.
- **Changing RS-485 settings after `applySerialConfig()`:** `setRS485Pins()` /
  `setRS485Mode()` only update the stored configuration; call `applySerialConfig()`
  again (or `enableRS485()`) for the DE/RE pins to be re-initialized.
- **ESP8266 has no pin remapping here:** `setSerialPins()` returns `false` unless both
  pins are `-1`.
- **`initStorage=false` in `begin()` requires storage to already be
  initialized by you.** It is not an "in-memory only" mode — see `begin()`'s
  documentation above.

---

## ESP32 & ESP8266 Specifics

### ESP32
- Both `Serial` and `Serial1` are fully supported.
- Custom RX/TX pins can be assigned via `setSerialPins`.
- Supports `FFat` in addition to SPIFFS/LittleFS.

### ESP8266
- `Serial1` is **TX only** (GPIO2), with no RX. Hence `setSerialPins` on `SER1` will fail.
- Only `LittleFS` and `SPIFFS` are supported (not `FFat`).
- For RS-485 on `Serial1`, you can only transmit; receiving must be handled via another port or software solution.

### Platform Differences

| Feature | ESP32 | ESP8266 |
|---------|-------|---------|
| Number of ports | 2 (custom pins allowed) | 2 (Serial1 TX only) |
| Pin assignment | Yes (`setSerialPins`) | No (for Serial1) |
| File systems | SPIFFS, LittleFS, FFat | SPIFFS, LittleFS |
| RS-485 | Full (half/full duplex) | Half-duplex with limitations |

---

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full, versioned history.

---

---

## Summary

**SerialConfigManager** is a complete, reliable, and flexible library for managing serial ports in ESP‑based projects. With it, you can easily configure complex settings, persist them, and take advantage of RS‑485 capabilities.

For more examples, see the [`examples/`](examples/) folder (`BasicUsage`, `RS485_HalfDuplex`, `JsonExportImport`, `StructBasedConfig`, `AutoDirectionSend`).

---

**Author:** Sh. Nourbakhsh Rad  
**License:** [MIT](LICENSE) — keep the copyright notice in copies of the code.
