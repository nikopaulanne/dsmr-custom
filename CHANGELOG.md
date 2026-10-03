# Changelog

All notable changes to this project will be documented in this file.

## [1.3.0] - 2026-10-02

### Changed

- Updated ESPHome framework selection for current releases and documented the tested minimum/current version matrix.
- Migrated ESP-IDF 6+ AES-GCM decryption to PSA Crypto, retained the ESP-IDF 5 hardware AES-GCM path, and updated linker archive discovery for both Mbed TLS layouts. Host tests and a native-toolchain ESP-IDF 6.0.1 ESP32 build pass. The PlatformIO IDF 6.0.1 build also passes after manually running ESP-IDF's `bootloader_ld_in_preprocess` Ninja target on a clean build; the firmware link then runs `post_build.py` and links `tfpsacrypto`.
- Clarified that encrypted telegram support is experimental and that the hardware smoke test covered unencrypted P1 data only.
- Closed issue #10 without a component parser change; splitting multi-value OBIS text remains a Home Assistant configuration task.

### Fixed

- Validate the original telegram CRC before publishing any measurements or normalizing wrapped values. Raw telegram diagnostics retain rejected frames; custom OBIS fields remain usable when standard-field interpretation fails on a valid frame.
- Make custom sensor overrides independent of registration order and use the parser's actual OBIS definitions, including configured M-Bus channels. Update all custom sensors sharing an OBIS code.
- Generate one combined standard-field list across repeated sensor/text sensor blocks, including empty lists and telegram-only text configurations.
- Validate keys as exactly 32 ASCII hexadecimal characters. Reject invalid runtime replacements without discarding the previous key, and reset partial reception when changing encryption mode.
- Receive UART data incrementally without blocking the ESPHome component loop, handle buffer allocation failures, and avoid telegram-sized Arduino stack allocations.
- Handle ESP-IDF 5 builds containing both a port-wrapper and upstream `libmbedtls.a`; link the selected upstream archives explicitly.
- Fix the example logger configuration and remove tracked generated Python bytecode.
- Reject nonfinite/out-of-range custom numeric values. Use native ESPHome schemas for custom sensor IDs, filters, automations and device-class validation.

### Upgrade notes

- Bad checksums now suppress custom values as well as standard values.
- Remove overridden standard sensors from YAML to avoid unused registered entities.
- Decryption keys require exactly 32 ASCII hexadecimal characters; the runtime key service accepts an empty string to clear the key.
- The latest local receiver/parser changes passed a Slimmelezer D1 Mini build, OTA and unencrypted runtime smoke test with ESPHome 2026.9.1 on 2026-10-02. Installation from the release tag remains to be checked after publication. Encryption remains experimental.

### Added

- Added host checks for AES-GCM wrapper behavior, decryption-key parsing, and encrypted frame size validation.
- Added compile coverage for ESP32 Arduino alongside ESP8266 Arduino, ESP32 ESP-IDF, and ESP32-C6 ESP-IDF.
- Added local test instructions and compatibility research/status notes.
- Added production-code host regressions for CRC rejection, publication overrides, repeated OBIS codes, frame boundaries, receive timeouts, key switching and synthetic encrypted reception, plus configuration/source-generation regressions for the minimum and current ESPHome releases.

## [1.2.0] - 2025-12-03

### Added

- **ESP-IDF Encryption Support:** Added experimental support for AES-GCM decryption on ESP-IDF platforms (e.g., ESP32-C6).
  - Uses system MbedTLS library with hardware acceleration where available.
  - Implemented via PlatformIO `extra_scripts` to handle library linking.

## [1.1.0] - 2025-12-02

### Added

- ESP-IDF framework support for ESP32-C6, ESP32-H2, and future chips
- Framework-agnostic string handling using conditional compilation
- AUTO_LOAD directive for sensor/text_sensor components (fixes #7)
- Comprehensive testing configuration files
- GitHub Actions CI/CD for compile testing

### Changed

- Replaced hardcoded Arduino.h dependency with conditional compilation
- Migrated Arduino String operations to framework-agnostic implementation
- Updated util.h to support both Arduino String and std::string

### Fixed

- Issue #6: Arduino.h missing file error when using ESP-IDF framework
- Issue #7: sensor.h missing when no sensor platform defined in YAML configuration

### Breaking Changes

None - fully backward compatible with existing configurations.

## [1.0.2] - Previous Release
- Initial stable release with custom OBIS sensor support
