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
| ESP-IDF 6+ | PSA Crypto `psa_aead_decrypt()` with AES-GCM and the configured 12-byte tag | ESPHome 2026.9.0 native-toolchain ESP32 build with IDF 6.0.1 compiled and linked. A PlatformIO IDF 6.0.1 build also compiled and linked after manually running the generated bootloader linker-script preprocessing target; `post_build.py` linked `tfpsacrypto`. Host tests check a known-answer vector and modified-tag rejection. | Experimental; no real encrypted meter has been tested. |

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
API shims. An ESPHome 2026.9.0 native-toolchain build compiled and linked the
ESP32 firmware against IDF 6.0.1. On PlatformIO, the first clean build stopped
at bootloader linking because the generated linker script had not been
preprocessed. Running the generated Ninja target
`bootloader_ld_in_preprocess` and retrying allowed the full firmware link and
`post_build.py` hook to pass; the hook selected `tfpsacrypto` as expected. This
is a generated-build workaround for the legacy PlatformIO path, not a change to
the component crypto source.

The current `post_build.py` script locates the Mbed TLS archives under
PlatformIO's generated ESP-IDF build directory. It accepts `mbedcrypto` on IDF 5
and `tfpsacrypto` on IDF 6. On IDF 5, the component/port wrapper and
upstream TLS archive can both be named `libmbedtls.a`; the hook selects the
upstream archive and links its file explicitly to avoid wrapper shadowing.
The hook passed an IDF 6.0.1 firmware link after the
bootloader linker-script preprocessing target was run. It still depends on the
generated build-tree layout. The native IDF 6 toolchain links through CMake and
does not use this SCons hook.

## What the tests establish

- Host tests check AES-128-GCM output against a published known-answer vector
  and verify that a modified authentication tag is rejected. The host test
  supplies OpenSSL-backed shims for the IDF 5 and PSA APIs; it does not run
  ESP-IDF crypto code on an ESP target.
- ESPHome compile tests verify the Arduino implementation on ESP8266 and ESP32,
  and the IDF 5 implementation on ESP32 and ESP32-C6. The native ESPHome
  toolchain also compiled and linked ESP32 firmware with IDF 6.0.1.
- The Slimmelezer D1 Mini repeat test on 2026-10-02 used the updated local
  component and ESPHome 2026.9.1. The maintainer confirmed successful build,
  upload and unencrypted operation. It did not test meter decryption.
- The production receive-path host test decrypts synthetic authenticated frames
  and rejects a modified tag, checks key/mode changes and injects allocation
  failures. It checks this implementation's framing assumptions rather than
  interoperability with a physical encrypted meter.

The 2026-10-02 verification passed the eight minimum/current firmware builds,
both IDF 6.0.1 ESP32 toolchains, and the host suite with and without sanitizers.
The PlatformIO IDF 6 recheck used the previously preprocessed bootloader script.
See [TESTING.md](../TESTING.md) for the commands and the completed local hardware
smoke test. Installation from the release tag remains to be checked once it exists.

These checks cover code paths and API compatibility. They do not verify
encrypted meter framing, key provisioning, real-meter interoperability,
long-term stability, or crypto performance. Keep encrypted operation marked
experimental until users report results from real encrypted meters.

## Key handling

Store compile-time keys in ESPHome `secrets.yaml` and reference them with
`!secret`; this keeps them out of the YAML file in Git, but does not by itself
encrypt a key embedded in firmware. The Slimmelezer example also accepts a
runtime key through the encrypted ESPHome API and persists it in a global with
`restore_value: true`; ESPHome preferences are not encrypted by this example.
Keys contain exactly 32 ASCII hexadecimal characters (16 bytes), without spaces
or prefixes. `set_decryption_key()` returns `false` for malformed input or a
failed runtime buffer allocation and preserves the previous key. An empty
string explicitly clears the key. Accepted key changes reset partial reception;
the example persists the replacement only after acceptance. Reapplying the
same valid key leaves the pending receive frame intact.

Never print a key in logs, include a real key in an example, or attach an
unredacted telegram or key to a public issue.

## Future work

- Replace the generated-path linker workaround with a supported dependency/link
  mechanism if ESPHome exposes one for external components. Prefer the native
  ESP-IDF toolchain; the legacy PlatformIO IDF 6 path requires a one-time
  bootloader linker-script preprocessing workaround on a clean build.
- Keep a target compile and link check against the native ESP-IDF 6+ toolchain.
- Keep synthetic-key tests and target compile checks in CI.
- Request user field reports that include board, framework, meter family, and
  success or failure, but do not request production keys or unredacted meter
  identifiers.
