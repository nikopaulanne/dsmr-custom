# Changelog

All notable changes to this project will be documented in this file.

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
