# dsmr-custom: compatibility research and repair plan

**Prepared:** 2026-09-27
**Release target:** `v1.3.0`
**Research baseline:** `main`, `v1.2.0-2-g7a873e8` (when this research began)
**Purpose:** plan compatibility work for current and future ESPHome/Home Assistant releases while preserving Nordic P1/SESKO support.

## Recommendation

Keep `dsmr-custom` as the parser for Nordic P1 telegrams. ESPHome's built-in `dsmr` component targets DSMR P1 field definitions and does not provide arbitrary OBIS-to-sensor mappings for the Nordic P1 code sets this project needs. ESPHome's `dlms_meter` component is a different protocol path: it parses DLMS/COSEM AXDR frames. Its dynamic OBIS mapping does not make it a drop-in parser for Nordic ASCII P1 telegrams.

Use the built-in components only as comparison references for overlapping fields. Any migration proposal must first pass a telegram fixture comparison that includes the Nordic codes, identification line, formatting, CRC, and the exact Home Assistant entities in use.

## Evidence and known issue history

### Current GitHub issues

| Issue | What it reports | Planned disposition |
|---|---|---|
| [#14](https://github.com/nikopaulanne/dsmr-custom/issues/14) | ESPHome removed `CORE.using_esp_idf`; `to_code()` fails during code generation. | The working tree uses `CORE.using_arduino`; the 2025.5.0 and current ESPHome build matrix passes. The issue remains open pending publication of the fix. |
| [#13](https://github.com/nikopaulanne/dsmr-custom/issues/13) | ELGAMA GAMA 350 / Stoen sends an encrypted frame with a nonstandard-looking header; decryption and frame boundaries are unresolved. | Treat as a distinct encrypted frame format. Request/retain a sanitized full-frame fixture and documented key semantics; do not alter standard decryption based on guesses. |

### Closed reports to retain as regression coverage

- [#10](https://github.com/nikopaulanne/dsmr-custom/issues/10) was closed without a component parser change, as requested. Users who need separate entities can expose the OBIS value as a text sensor and split it in their Home Assistant template configuration.
- [#6](https://github.com/nikopaulanne/dsmr-custom/issues/6) and [#7](https://github.com/nikopaulanne/dsmr-custom/issues/7) reported framework/library include and build failures (`Arduino.h`, generated sensor headers). Keep both Arduino and ESP-IDF compilation in CI.
- [#12](https://github.com/nikopaulanne/dsmr-custom/issues/12) initially looked like encrypted/corrupt P1 data; follow-up identified wiring/grounding as the cause and the issue was closed. Keep diagnostics in documentation, but do not treat it as a parser defect.

The repository's example configuration and Home Assistant entities show active use of Nordic meter identification, Finnish OBIS sensor mappings, and energy entities with Home Assistant energy metadata. Preserve entity names/unique IDs where possible during parser changes; do not copy live meter identifiers or readings into public fixtures.

## Current implementation status

- **Compatibility baseline: complete.** The declared minimum (ESPHome 2025.5.0) and current release compile on ESP8266 Arduino, ESP32 Arduino, ESP32 ESP-IDF, and ESP32-C6 ESP-IDF. The local Slimmelezer smoke test also passed on Home Assistant Core 2026.9.3 and ESPHome Device Builder 2026.9.0.
- **Issue #10 decision: complete.** The issue is closed without a component change; value splitting belongs in the user's Home Assistant configuration.
- **Crypto verification: partial.** Key/frame helpers, a host AES-GCM known-answer vector, and framework compile paths are checked. Encrypted-meter operation remains experimental and has not been independently verified on hardware.
- **Parser fixture suite: pending.** Existing CI compiles the parser, but sanitized Nordic and ordinary DSMR telegram fixtures and malformed-frame boundary tests still need to be added.
- **ESP-IDF 6 build verification: complete for both tested toolchains.** The PSA implementation compiled and linked with ESPHome 2026.9.0's native ESP-IDF 6.0.1 ESP32 toolchain. The PlatformIO IDF 6.0.1 build also passed after manually running the generated `bootloader_ld_in_preprocess` Ninja target; the full firmware link then exercised `post_build.py` and selected `tfpsacrypto`. The archive lookup still depends on generated build-tree layout.
- **Issue #13: pending evidence.** GAMA 350 support needs a sanitized, reproducible frame fixture before implementation changes.

## Findings from the code review

1. The custom parser's key capability is line-by-line OBIS matching from `custom_obis_sensors`, so users can define codes that do not exist in ESPHome's fixed DSMR schema.
2. The current value parser expects one parenthesized value and parses numeric values with `strtof`. It cannot correctly represent an OBIS line with multiple parenthesized fields and units as reported in #10.
3. The lenient identification-line handling is intentional for Nordic meters. Do not tighten it to the Dutch DSMR identification grammar without captured Nordic regression fixtures.
4. ESP32 review found no confirmed UART buffer overwrite from the inspected paths, but the configured telegram length, terminator byte, vector/string allocations, and long frames should get explicit boundary and memory tests.
5. ESP-IDF crypto integration has two separate compatibility concerns. `post_build.py` discovers Mbed TLS archives under PlatformIO's generated component build tree and selects `mbedcrypto` on IDF 5 or `tfpsacrypto` on IDF 6, but still depends on that build-tree layout. IDF 6+ uses PSA Crypto for AES-GCM, avoiding the legacy Mbed TLS cipher-ID dependency. The PSA branch passes host tests and an ESPHome 2026.9.0 native-toolchain ESP32 build against IDF 6.0.1. The PlatformIO build also passed after manually running the generated `bootloader_ld_in_preprocess` target; its initial failure was in generated bootloader-script preprocessing before the component hook. Espressif documents PSA as the primary cryptography interface in its [ESP-IDF 6 migration guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-6.x/6.0/security.html), and the [PSA AEAD API](https://arm-software.github.io/psa-api/crypto/1.1/api/ops/aead.html) defines the authenticated decrypt call used here.
6. The supplied `slimmelezer` YAML logs only fixed status messages when the runtime key is applied or updated; it does not log the key value. It does persist the API-provided key in an ESPHome global with `restore_value: true`, and the example does not configure encryption at rest. Keep this storage behavior clear in the example and never log key material.

## Repair plan

### Phase 1 — Establish the compatibility baseline (complete)

- `CORE.using_arduino` selects the Arduino crypto dependency; ESP-IDF builds use the ESP-IDF implementation.
- CI compiles the exact ESPHome 2025.5.0 minimum and current release. The current release check also runs weekly.
- The matrix covers ESP8266 Arduino, ESP32 Arduino, ESP32 ESP-IDF, and ESP32-C6 ESP-IDF. A scheduled development/nightly job remains a possible follow-up.
- Local configuration validation and full builds passed for the same four targets on the minimum and current versions.

### Phase 2 — Make parser behavior fixture-driven (incomplete)

- Add sanitized telegram fixtures for at least one Finnish/Nordic meter, one ordinary DSMR meter, and a header variation. Store no keys or personally identifying meter data.
- Extract parser decisions into host-testable helpers where feasible: identification line, OBIS extraction, parenthesis/token parsing, number plus unit parsing, CRC boundary, and malformed/truncated frame handling.
- Add boundary cases for empty lines, CRLF/LF, missing footer, bad CRC, exact maximum frame length, one byte over the limit, and repeated telegrams.
- Keep arbitrary configured OBIS mapping and standard sensor mapping behavior covered, including override behavior and text sensors.

### Phase 3 — Keep multi-value conversion in Home Assistant (#10, complete)

- Do not add meter-specific multi-value numeric parsing to `dsmr-custom`. The reported line combines a timestamp and measurement, and the project has no physical meter fixture to validate a generic parser change.
- Users who need separate values can configure the OBIS code as a `text_sensor` and split its text in a Home Assistant template sensor. The current text path preserves the inner `)(` separator for this example.
- Issue #10 is closed without an additional comment; treat the template as user-owned Home Assistant configuration, not a beta component feature.

### Phase 4 — Make ESP32 and crypto support sustainable (in progress)

- Continue the existing experimental-release practice: invite users to test encrypted telegrams on their own meters and report the exact board, framework, meter family, and result. The maintainer has no hardware that receives encrypted meter telegrams.
- Replace assumptions about PlatformIO's private MbedTLS directory layout with ESP-IDF-supported dependency/link configuration. If that is not possible for the supported ESPHome toolchain, isolate the linking shim and fail with a clear diagnostic.
- Retain the ESP-IDF hardware AES-GCM path for IDF 5 and use PSA AEAD for IDF 6+. Native ESPHome 2026.9.0 / IDF 6.0.1 ESP32 compile and link passed. PlatformIO IDF 6.0.1 also passed after manually running `bootloader_ld_in_preprocess`; prefer native ESP-IDF, and track the clean-build preprocessing gap as an upstream build-tool issue.
- Verify decryption and authentication failure handling using published AES-GCM test vectors and synthetic test keys, plus compile checks for each ESP32 framework target. Do not claim encrypted-meter field testing: there is no hardware available that receives encrypted meter telegrams.
- Test buffer limits, allocation paths, and parser behavior against representative test data. Mark ESP32 encrypted-meter operation as compile/test-vector verified but field-unverified until a user can test it with a real encrypted meter.
- Keep the ESP8266 build as a supported, separately tested profile; do not let ESP32-specific crypto headers leak into Arduino ESP8266 builds.

### Phase 5 — Investigate ELGAMA GAMA 350 (#13) separately (waiting for a fixture)

- Ask for a complete sanitized byte fixture including framing and length fields, plus confirmation of whether the supplied EK/AK are production credentials. Never request or store live keys in GitHub issues or fixtures.
- Document the frame structure from meter/provider specifications or reproducible test data: length semantics, nonce/system-title/invocation counter, authentication tag, CRC, and payload encoding.
- Build an offline decryption/parser test using synthetic test keys and a known plaintext/ciphertext pair before touching the runtime parser.
- Implement support behind a clearly identified framing strategy only after the format is established; keep generic DSMR encryption behavior unchanged.

### Phase 6 — Docs, examples, and release process (documentation updated; release pending)

- Keep examples free of key values in logs, show secrets-backed configuration for compile-time keys, and document that the runtime-key example persists its key in ESPHome preferences without configuring encryption at rest.
- Correct compatibility claims and separate compile-tested support from runtime-tested support.
- Explain the native ESPHome component boundary: built-in `dsmr` is not a replacement for Nordic P1 OBIS definitions; `dlms_meter` parses DLMS/COSEM and does not consume these ASCII P1 telegrams.
- Add migration notes for entity identity, encryption, `custom_obis_sensors`, and raw telegram diagnostics.
- Release only after the version matrix and fixture suite pass; publish any ESP-IDF experimental status precisely.

## Acceptance criteria

- Existing Finnish/Nordic fixture produces the same configured entities and values as the current release for supported single-value records.
- Issue #10 requires no component change; multi-value splitting is left to Home Assistant configuration.
- ESP8266 Arduino, ESP32 Arduino, ESP32 ESP-IDF, and ESP32-C6 ESP-IDF compile on the declared supported versions; native and PlatformIO ESP-IDF 6.0.1 ESP32 compile and link pass. The PlatformIO path needs the generated bootloader linker script preprocessed on a clean build.
- AES-GCM implementation passes published test vectors and synthetic-key tamper/failure cases; encrypted-meter behavior on physical hardware remains explicitly field-unverified.
- Telegram truncation, bad CRC, and maximum-length cases fail safely and report a useful diagnostic.
- No key is printed in logs; no real decryption keys appear in code, tests, issue attachments, or CI output.
- GAMA 350 support is marked unsupported/experimental until a reproducible fixture and decryption test pass.

## Research summary

- Public documentation and issue research supports keeping `dsmr-custom` for Nordic ASCII P1. `dlms_meter`'s dynamic OBIS mapping applies to DLMS/COSEM frames and does not make it an ASCII P1 parser.
- Issue #10 exposes a gap for multiple parenthesized values on one OBIS line; issue #14 reports the removed `CORE.using_esp_idf` check. Keep framework-specific crypto selection and validate Arduino and ESP-IDF builds.
- ESP32 encryption can be checked against test vectors and synthetic fixtures, but there is no encrypted-meter hardware for field testing. Keep that distinction visible in support claims.
- GAMA 350 frame details and decryption remain unverified. Do not change generic decryption until a sanitized fixture and reproducible test establish the format.
- The web research also returned community posts and unrelated search results. Treat them as leads; use official ESPHome documentation, project issues, and reproducible code/tests for implementation decisions.
- The v1.2.0 community announcement already classified ESP-IDF encryption as compile-tested and requested field testing; it also asked users to continue testing Arduino encryption. README support claims should state the tested board/platform and evidence level precisely.

### Source list

- ESPHome DSMR docs: https://esphome.io/components/sensor/dsmr/
- ESPHome DLMS meter docs: https://esphome.io/components/sensor/dlms_meter/
- ESPHome 2026.6 release: https://github.com/esphome/esphome/releases/tag/2026.6.0
- dsmr-custom issue #14: https://github.com/nikopaulanne/dsmr-custom/issues/14
- dsmr-custom issue #13: https://github.com/nikopaulanne/dsmr-custom/issues/13
- dsmr-custom issue #10: https://github.com/nikopaulanne/dsmr-custom/issues/10
- dsmr-custom issue #12: https://github.com/nikopaulanne/dsmr-custom/issues/12
- dsmr-custom issue #7: https://github.com/nikopaulanne/dsmr-custom/issues/7
- v1.2.0 community release and encryption test request: https://community.home-assistant.io/t/release-dsmr-custom-universal-esphome-component-for-dsmr-compatible-smart-meters/899172
- ESPHome PR #15458 (`dlms_parser`): https://github.com/esphome/esphome/pull/15458
- Current project implementation and sample configuration: this repository's `components/dsmr_custom/`, `slimmelezer-example.yaml`, and `README.md`.
