# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Changed
- Updated ESPHome framework selection for current releases and documented the tested minimum/current version matrix.
- Migrated ESP-IDF 6+ AES-GCM decryption to PSA Crypto, retained the ESP-IDF 5 hardware AES-GCM path, and updated linker archive discovery for both Mbed TLS layouts. Host tests and a native-toolchain ESP-IDF 6.0.1 ESP32 build pass. The PlatformIO IDF 6.0.1 build also passes after manually running ESP-IDF's `bootloader_ld_in_preprocess` Ninja target on a clean build; the firmware link then runs `post_build.py` and links `tfpsacrypto`.
- Clarified that encrypted telegram support is experimental and that the hardware smoke test covered unencrypted P1 data only.
- Closed issue #10 without a component parser change; splitting multi-value OBIS text remains a Home Assistant configuration task.

### Added
- Added host checks for AES-GCM wrapper behavior, decryption-key parsing, and encrypted frame size validation.
- Added compile coverage for ESP32 Arduino alongside ESP8266 Arduino, ESP32 ESP-IDF, and ESP32-C6 ESP-IDF.
- Added local test instructions and compatibility research/status notes.

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
