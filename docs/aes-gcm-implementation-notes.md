# AES-GCM Decryption Notes (Experimental)

Encrypted telegram support remains experimental. The maintainer has not
independently tested it with a real encrypted meter. A previous community
report described Arduino decryption working on a D1 Mini with v1.2.0, but that
result has not been reproduced in the current test setup. Successful compilation
and synthetic test vectors do not establish that a meter's framing, key, nonce,
or authentication tag handling works in the field.

## Implementation by framework

| Framework | Implementation | Evidence | Field status |
|---|---|---|---|
| Arduino | `rweather/Crypto` 0.4.0 | ESP8266 and ESP32 compile tests pass. A previous community report described successful decryption on a D1 Mini with v1.2.0. | Experimental; the community result has not been reproduced by the maintainer, and the current Slimmelezer hardware test used an unencrypted telegram. |
| ESP-IDF 5 | ESP-IDF `esp_aes_gcm_auth_decrypt()` API, with libraries linked by `post_build.py` | ESP32 and ESP32-C6 target builds pass. An OpenSSL-backed host shim exercises the production wrapper against an AES-GCM known-answer vector and a modified tag. | Experimental; no real encrypted meter has been tested. |
| ESP-IDF 6+ | PSA Crypto `psa_aead_decrypt()` with AES-GCM and the configured 12-byte tag | ESPHome 2026.9.0 native-toolchain ESP32 build with IDF 6.0.1 compiled and linked. The PlatformIO attempt compiled the component but stopped at bootloader linking because `bootloader.ld` was missing. Host tests also check a known-answer vector and modified-tag rejection. | Experimental; no real encrypted meter has been tested. |

ESP-IDF provides an AES-GCM API that may use platform acceleration. This project
has not benchmarked it, so no speedup claim is made.

## ESP-IDF integration details

`dsmr_crypto_impl.cpp` uses ESP-IDF's `esp_aes_gcm_auth_decrypt()` on IDF 5 and
PSA Crypto's `psa_aead_decrypt()` on IDF 6 and newer. Espressif moved to PSA as
the primary cryptography interface with Mbed TLS 4 in IDF 6, and PSA drivers can
provide hardware acceleration where available. The PSA path uses
`PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, 12)` to match the meter's
ciphertext format. Both APIs authenticate the tag before the component accepts
the plaintext.

The host suite compiles the IDF 5 and PSA source branches against OpenSSL-backed
API shims. An ESPHome 2026.9.0 native-toolchain build also compiled and linked
the ESP32 firmware against IDF 6.0.1. A separate PlatformIO attempt compiled
`dsmr_crypto_impl.cpp` but failed during bootloader linking, before the main
firmware link and `post_build.py` hook ran. The PSA source and native IDF 6 link
are verified; the PlatformIO IDF 6 path is not.

The current `post_build.py` script locates the Mbed TLS archives under
PlatformIO's generated ESP-IDF build directory. It accepts `mbedcrypto` on IDF 5
and `tfpsacrypto` on IDF 6. This workaround still depends on the generated build
tree. The native IDF 6 build does not use this SCons hook, and the PlatformIO
attempt did not reach it, so the hook has not yet been verified with IDF 6.

## What the tests establish

- Host tests check AES-128-GCM output against a published known-answer vector
  and verify that a modified authentication tag is rejected. The host test
  supplies OpenSSL-backed shims for the IDF 5 and PSA APIs; it does not run
  ESP-IDF crypto code on an ESP target.
- ESPHome compile tests verify the Arduino implementation on ESP8266 and ESP32,
  and the IDF 5 implementation on ESP32 and ESP32-C6. The native ESPHome
  toolchain also compiled and linked ESP32 firmware with IDF 6.0.1.
- A live Slimmelezer test verified unencrypted P1 parsing and sensor updates.
  It did not test decryption.

These checks cover code paths and API compatibility. They do not verify
encrypted meter framing, key provisioning, real-meter interoperability,
long-term stability, or crypto performance. Keep encrypted operation marked
experimental until users report results from real encrypted meters.

## Key handling

Store keys in ESPHome `secrets.yaml` and reference them with `!secret`. Never
print a key in logs, include a real key in an example, or attach an unredacted
telegram or key to a public issue.

## Future work

- Replace the generated-path linker workaround with a supported dependency/link
  mechanism if ESPHome exposes one for external components; verify or retire
  the PlatformIO IDF 6 path.
- Keep a target compile and link check against the native ESP-IDF 6+ toolchain.
- Keep synthetic-key tests and target compile checks in CI.
- Request user field reports that include board, framework, meter family, and
  success or failure, but do not request production keys or unredacted meter
  identifiers.
