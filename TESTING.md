# Testing Guide for dsmr_custom

## Automated Testing

### Compile Tests (Required Before Release)

Run all compile tests locally:

```bash
# Test Arduino ESP8266
esphome compile test-configs/test-arduino-esp8266.yaml

# Test ESP-IDF ESP32
esphome compile test-configs/test-espidf-esp32.yaml

# Test ESP-IDF ESP32-C6
esphome compile test-configs/test-espidf-esp32c6.yaml
```

**Success Criteria:**
- ✅ All three configurations compile without errors
- ✅ No compiler warnings related to framework compatibility
- ✅ Binary files are generated successfully

### GitHub Actions

All pull requests automatically run compile tests. Check the Actions tab for results.

## Manual Runtime Testing

### Minimum Testing (Required)

Test on your D1 Mini hardware:

1. Flash the Arduino ESP8266 test configuration
2. Verify device boots and connects to WiFi
3. Verify P1 telegrams are received
4. Verify sensors update correctly in Home Assistant
5. Check logs for any errors or warnings
6. Monitor heap memory for stability (no leaks)

**Success Criteria:**
- ✅ Device boots successfully
- ✅ WiFi connects
- ✅ P1 data is received and parsed
- ✅ No error messages in logs
- ✅ Heap memory remains stable over 1 hour

### Community Testing (Recommended)

For ESP-IDF validation, community testing is essential:

1. Create GitHub Issue with testing request
2. Provide clear instructions (see template below)
3. Wait for at least 2 successful ESP-IDF test reports
4. Address any issues found before stable release

## Release Checklist

### Pre-Release

- [ ] All compile tests pass locally
- [ ] GitHub Actions CI passes
- [ ] D1 Mini runtime testing completed successfully
- [ ] No regression from previous version
- [ ] CHANGELOG.md updated
- [ ] README.md updated with framework information
- [ ] Version number bumped in code/documentation

### Beta Release (v1.1.0-beta.1)

- [ ] Tagged as pre-release on GitHub
- [ ] Community testing issue created
- [ ] Testing period: minimum 1-2 weeks
- [ ] At least 2 community testers recruited

### Release Candidate (v1.1.0-rc.1)

- [ ] At least 2 successful ESP-IDF community tests
- [ ] At least 1 ESP32-C6 test report
- [ ] All critical bugs from beta fixed
- [ ] Documentation reviewed

### Stable Release (v1.1.0)

- [ ] No critical issues reported for 1 week
- [ ] All tests passed
- [ ] Community feedback positive
- [ ] Release notes finalized

## Community Testing Issue Template

Use this template when requesting community testing:

```markdown
# 🧪 Beta Testing: ESP-IDF Framework Support (v1.1.0-beta.1)

## Overview

I've implemented ESP-IDF framework support but can only test on ESP8266 D1 Mini. 
Community testing is needed to validate ESP-IDF builds.

## Testing Needed

If you have any of these devices, please help test:
- ✅ ESP32 (any variant) with ESP-IDF
- ✅ ESP32-C6
- ✅ ESP32-H2
- ✅ ESP32-S3

## How to Test

1. Add to your YAML:
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/nikopaulanne/dsmr-custom
      ref: v1.1.0-beta.1
    components: [ dsmr_custom ]
    refresh: 0s
```

2. Set your framework:
```yaml
esp32:
  board: your-board-here
  framework:
    type: esp-idf
```

3. Compile, flash, and test
4. Report results below

## What to Report

Please provide:
- Board type: (e.g., ESP32-C6-DevKitC-1)
- Framework: (esp-idf)
- ESPHome version: (e.g., 2025.11.2)
- Compile result: ✅ Success / ❌ Failed
- Flash result: ✅ Success / ❌ Failed
- Runtime result: ✅ Working / ❌ Issues
- Any errors or warnings:

## Timeline

- Beta period: 2 weeks from today
- Target stable release: [DATE]

Thank you for helping improve dsmr_custom! 🙏
```
