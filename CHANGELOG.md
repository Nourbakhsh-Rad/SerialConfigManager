# Changelog

All notable changes to this project are documented here.
Versions follow the `SCM_LIB_VERSION` string in `src/SerialCfgDefs.h`.

## [2.5.0]

Release-preparation review (host-side compile with `-Wall -Wextra` for ESP32 and
ESP8266, plus the new host tests in `extras/host-tests/`; tested on real hardware by the author).

### Added
- `writeRS485()` (4 overloads) as a real inline alias of `writeSerial()`. It was
  documented, listed in `keywords.txt` and used by the `RS485_HalfDuplex` example,
  but never actually declared - that example did not compile.
- Include guard in `SerialCfgDefs.h`.
- `extras/host-tests/`: stub-based unit tests runnable on a PC, and a GitHub Actions
  workflow (library lint, example compilation for ESP32/ESP8266, host tests).

### Fixed
- `begin()` lost its own diagnostic ("Failed to load ... using defaults") because
  `resetToDefaults(true)` -> `saveConfig()` clears the error. The message is preserved now.
- `parseJsonConfig()`/file loading: a missing `serial1.baudRate` fell back to 115200
  instead of Serial1's 9600 default.
- ESP32: specifying only one of `rxPin`/`txPin` was silently ignored; it is honoured now
  (`-1` = default for the other pin).
- ESP32 variants built with "USB CDC On Boot" failed to compile (`&Serial` is not a
  `HardwareSerial`); SER0 maps to `Serial0` there.
- ESP8266: `setSerialPins()` accepted custom pins on `SER0` and then ignored them; it now
  rejects custom pins on both ports. The `rxEnabled`/`txEnabled` pin hack is ESP32-only.
- Examples set Serial0 to 9600 / 57600 while printing to the same port at 115200, so the
  Serial Monitor showed garbage; ESP32 examples now set explicit `Serial1` pins.
- `%lu` with `uint32_t` format warning in `BasicUsage`; forward declaration added to
  `AutoDirectionSend` so it also builds outside the Arduino IDE.
- Wrong `@file` header in `SerialConfigManager.cpp`; mixed CRLF/LF line endings.

### Changed
- Renamed the generic macros `CFG_FS_MODE` -> `SCM_FS_MODE` and `CFGfs` -> `SCM_FS` to avoid
  name clashes with user code (no compatibility aliases; the library was not yet published).
- `printConfig()` takes a `Stream&` instead of `HardwareSerial&` (source compatible).
- `applySerialConfig()` no longer blocks for an unconditional `delay(100)`.
- README: corrected `resetToDefaults(bool)`, `end()`, error table, PlatformIO install line,
  stale changelog section; added caveats about console UART and ESP32 `Serial1` pins.

## [2.4.2]

Final pre-release cleanup pass: no functional bugs found beyond what prior
versions already fixed. This release compiled cleanly (0 errors, 0 warnings
attributable to the library itself) under `-Wall -Wextra` across all six
supported configurations (FS/EEPROM/None storage × ESP32/ESP8266) using a
local Arduino-core stub harness.

### Fixed
- `getVersion()`'s Doxygen comment and the matching README entry hardcoded
  a stale example version string (`"1.1.0"`, left over from when the
  version macro was first introduced) that had drifted out of sync with
  every version bump since. Both now describe the value without hardcoding
  a number that can go stale again.
- Silenced two real `-Wunused-parameter` warnings in `begin()`: `initEeprom()`
  has no `formatOnFail` equivalent, and neither parameter is used at all
  when `SCM_STORAGE_TYPE` is `SCM_STORAGE_NONE`. Both are now explicitly
  cast to `void` in those branches instead of triggering a warning on
  strict build configurations.
- Reduced signed/unsigned comparison warnings in the hand-rolled JSON
  parser (`extractJsonValue()`, `parseSerialConfig()`), which compared
  `int` loop variables against `String::length()` (`unsigned int` on real
  Arduino cores). Purely cosmetic — config JSON is always well under
  `INT_MAX` — but now cast once, cleanly, instead of relying on implicit
  conversion at every loop comparison.

### Added
- A "Known Limitations" section in the README documenting the hand-rolled
  JSON parser's scope (not a full JSON implementation), the soft-only
  nature of `rxEnabled`/`txEnabled`, `writeSerial()`'s flush-has-no-timeout
  caveat for half-duplex RS-485, and the `initStorage=false` contract from
  2.4.1 — gathered in one place instead of scattered across docstrings.

## [2.4.1]

### Fixed
- **Corrected a regression introduced in 2.4.0.** `initStorage` means
  "should `begin()` perform the storage backend's own setup
  (`SCM_FS.begin()` / `EEPROM.begin()`)?", not "should storage be used at
  all?". 2.4.0 misread this as the latter and made `initStorage=false`
  skip storage entirely (`_fsAvailable`/`_eepromInitialized` defaulted to
  `false`), which broke the intended use case: a caller who already
  initialized the filesystem/EEPROM themselves (elsewhere in their own
  `setup()`) and passes `initStorage=false` specifically so `begin()`
  doesn't redundantly re-init it, while still expecting `begin()` to load
  the existing saved config. Restored the correct contract: with
  `initStorage=false`, `begin()` trusts that storage is already ready and
  goes straight to using it (skipping only the mount/init call itself).
  Docs (header Doxygen + README) and two examples that had been written
  around the incorrect 2.4.0 behavior were corrected to match.
- The ESP32 double-mount fix, the EEPROM `delay(200)` removal, and the
  `EEPROM.begin()` return-value check from 2.4.0 are unaffected and remain
  in place.

## [2.4.0]

### Fixed
- **`begin(formatOnFail, initStorage=false)` no longer touches storage at
  all**, matching its own documented "runs purely in-memory" contract for
  both the FS and EEPROM backends. Previously (FS backend) `_fsAvailable`
  was optimistically set `true` even when the filesystem was never
  mounted, so `begin()` could go on to call `fsConfigFileExists()` and
  even write a defaults file (`resetToDefaults(true)` → `saveConfig()`) on
  a filesystem that was never `.begin()`'d - undefined behavior on some FS
  drivers, and wasted work on all of them.
- `initFileSystem()` on ESP32 no longer mounts twice. When `formatOnFail`
  is `true`, `SCM_FS.begin(true)` already retries with a format internally
  on a failed mount; the old code called `SCM_FS.begin(true)` a second time,
  with identical arguments, if that still failed - which can never succeed
  differently and just doubled the worst-case mount time on a corrupt or
  fresh partition.
- `initEeprom()` had an unconditional `delay(200)` after `EEPROM.begin()`
  with no basis in the library's actual behavior (`EEPROM.begin()` already
  blocks until ready) - pure, unjustified startup latency on every
  `begin(..., true)` call with the EEPROM backend selected. Removed.
- `initEeprom()` now checks `EEPROM.begin()`'s return value on ESP32
  (`false` = allocation failure) instead of always reporting success; on
  ESP8266, where `EEPROM.begin()` returns `void`, behavior is unchanged.

## [2.3.0]

### Fixed
- **`writeSerial()`/`writeRS485()` blocked for the full `timeoutMs` on
  (almost) every call and then reported failure.** The post-write "wait
  for drain" loop checked `while (ser->availableForWrite() > 0)`, but
  `availableForWrite()` reports free *buffer* space, not pending
  *transmission* - it is `> 0` immediately after essentially any write
  that didn't fill the TX buffer to capacity, so the loop could only ever
  exit through the timeout branch. Replaced with `HardwareSerial::flush()`,
  the correct, portable call for "block until the data has actually been
  transmitted."

### Changed (performance)
- `writeSerial()`/`writeRS485()` now only run the transmit → drain → receive
  RS-485 direction-control sequence when the port is actually configured as
  `SCM_RS485_HALF_DUPLEX`. A plain port or a full-duplex RS-485 port (whose
  DE/RE never need to move) skips that sequence entirely, so the call
  returns as soon as the data is queued - as fast as a raw
  `HardwareSerial::write()` - instead of unconditionally waiting on a drain
  it never needed in the first place.

### Added
- `writeSerial(port, const char* data, timeoutMs)` and the matching
  `writeRS485()` overload: length is measured internally via `strlen()`, so
  a null-terminated C-string can be sent without the caller computing or
  passing a length.

## [2.2.0]

### Added
- `writeSerial(port, data, ...)` — a universal send helper with the same
  overloads as `writeRS485()`. The calling code no longer needs to know or
  check whether a port is wired for RS-485: `writeSerial()` automatically
  switches DE/RE to transmit/receive when RS-485 is enabled for that port,
  and is simply a plain timeout-protected write otherwise.
- `examples/AutoDirectionSend` — demonstrates sending to a plain UART port
  and an RS-485 port through the exact same call.
- `examples/StructBasedConfig` — demonstrates configuring a port in one
  call via `setSerialConfig()` and a filled-in `SerialConfigs` struct.

### Changed
- `writeRS485()` is now a thin alias of `writeSerial()` (identical
  behavior/signatures) kept for call sites that want to name RS-485 intent
  explicitly. No breaking change: existing `writeRS485()` calls keep working.

## [2.1.0]

### Added
- Public API to match what `README.md` already documented but the library
  never actually implemented: `isInitialized()`, `configFileExists()`,
  `deleteConfigFile()`, `configToJson()`, `parseJsonConfig()`.
- `SerialConfigManager::getVersion()` / `SCM_LIB_VERSION`.
- Config-schema versioning: a `configVersion` field in the JSON output, and
  a version byte in the EEPROM layout (`SCM_CONFIG_VERSION`). EEPROM data
  written by an incompatible version is now rejected instead of
  misinterpreted; JSON files are read leniently across versions.
- Arduino Library Manager / PlatformIO packaging: `library.properties`,
  `library.json`, `keywords.txt`, `LICENSE` (MIT).
- Standard `examples/<Name>/<Name>.ino` layout (`BasicUsage`,
  `RS485_HalfDuplex`, `JsonExportImport`), replacing the single root-level
  `.ino`.
- Doxygen-style documentation comments across the public header.

### Fixed
- **EEPROM storage backend did not compile at all**: `begin()` referenced
  an undeclared `eepromInitialized` identifier (missing the `_` prefix used
  by the real member `_eepromInitialized`), `initEeprom()` was defined as a
  free function instead of `SerialConfigManager::initEeprom()`, and
  `<EEPROM.h>` was never included. Anyone building with
  `SCM_STORAGE_TYPE=SCM_STORAGE_EEPROM` got a hard compile/link failure.
- EEPROM "is this valid data" check used to test whether the first stored
  byte was `0xFF`, which is not a reliable signal. Replaced with an
  explicit signature byte (`SCM_EEPROM_MAGIC`) plus a version byte.
- `enableRS485(port, true)` left the DE/RE pins floating (no defined logic
  level) until the next `applySerialConfig()` call, risking bus contention
  on a live RS-485 network. It now defaults to receive mode immediately.
- `isSerialActive()` previously returned `true` whenever a port was
  `enabled`, even if `applySerialConfig()` had never actually been called
  for it - contradicting its own documented behavior. It now tracks real
  applied state per port.
- `parseSerialConfig()` (used by both file loading and the new
  `parseJsonConfig()`) accepted any integer from the input JSON and wrote
  it straight into the live config - including into fields later handed to
  the hardware UART / `pinMode()`. Every numeric field is now validated
  (`isValidBaudRate`, `isValidDataBits`, ... and int8_t range checks for
  pins); a corrupted or malformed payload now fails the whole parse instead
  of producing a partially-invalid config.
- EEPROM save/load used a hardcoded `uint8_t buffer[256]` with no relation
  to `SCM_EEPROM_SIZE`; now sized from `sizeof(SerialConfigs)`, with a
  `static_assert` that fails the build if the reserved EEPROM window is
  ever too small.

### Changed
- `serializeConfig()` now reserves the JSON `String`'s capacity up front to
  reduce heap churn/fragmentation on repeated saves (relevant on ESP8266).
- Removed the unused `SCM_BUFFER_SIZE` macro (never referenced anywhere in
  the library).

## [2.0]
- Baseline as documented in the original `README.md`: unified port-based
  API, RS-485 half/full-duplex support, getter functions, `writeRS485()`
  timeout support, improved JSON parsing, `stopBits` fix in the hardware
  config-word generation.
