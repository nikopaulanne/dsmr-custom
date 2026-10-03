# Testing dsmr-custom

The automated checks cover configuration/code generation, firmware compilation,
key and encrypted-frame validation, and the ESP-IDF AES-GCM wrapper. They do not
replace testing with a real encrypted meter.

The runtime host test also compiles production `dsmr.cpp` and the parser against
small ESPHome/UART/sensor stubs. It verifies rejected-CRC publication, custom
overrides in both registration orders, multiple sensors for one code,
configured M-Bus channels, wrapped values, numeric validity, frame size limits,
nonblocking partial reception, timeouts, key changes and allocation failures.
OpenSSL generates synthetic encrypted frames for the receive-path test;
this checks the implementation's framing assumptions, not a real meter format.

## Local tool setup

Keep Python packages, ESPHome build output, and PlatformIO downloads outside the
Git checkout. With `uv` installed, create an environment beside the repository:

```bash
uv venv --python 3.12 ../.venv-dsmr-custom
uv pip install --python ../.venv-dsmr-custom/bin/python esphome==2026.9.0
```

Python 3.12 or newer is required by current ESPHome. To match the latest
ESPHome/Python versions used during development, Python 3.14 also works with
ESPHome 2026.9.0. The minimum-version checks used a separate Python 3.12
environment with `esphome==2025.5.0`. Keep separate environments and build
directories when comparing releases; run PlatformIO builds sequentially if
they share package storage.

Set ESPHome and PlatformIO storage outside the checkout before compiling:

```bash
export ESPHOME_DATA_DIR="$(cd .. && pwd)/.esphome-data"
export ESPHOME_BUILD_PATH="build"
export PLATFORMIO_CORE_DIR="$(cd .. && pwd)/.platformio"
```

The examples below use the isolated executable. Adjust its path if you chose a
different environment:

```bash
ESPHOME="../.venv-dsmr-custom/bin/esphome"
```

## Host tests

The host tests need CMake, a C++ compiler, and OpenSSL development headers. They
run without ESP hardware:

```bash
cmake -S tests -B ../.dsmr-custom-host-tests
cmake --build ../.dsmr-custom-host-tests --parallel
ctest --test-dir ../.dsmr-custom-host-tests --output-on-failure
```

To repeat the sanitizer checks with a compiler that supports them:

```bash
cmake -S tests -B ../.dsmr-custom-host-sanitized \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build ../.dsmr-custom-host-sanitized --parallel
ctest --test-dir ../.dsmr-custom-host-sanitized --output-on-failure
```

The AES-GCM test compiles the component's ESP-IDF 5 hardware AES-GCM and
ESP-IDF 6+ PSA Crypto branches against host shims backed by OpenSSL. It checks a
published AES-128-GCM known-answer vector and rejects a modified authentication
tag. These shims check the source branches, not actual ESP-IDF 6+ compilation;
the target matrix currently compiles against the ESPHome-provided ESP-IDF.

Run configuration and source-generation regressions using the installed
ESPHome environment:

```bash
../.venv-dsmr-custom/bin/python tests/test_configuration.py
```

These tests use temporary directories outside the checkout and perform no
toolchain downloads. They validate keys and native sensor properties, then
generate and check C++ field lists for custom-only, telegram-only,
numeric-plus-telegram and repeated standard sensor/text sensor blocks. They also
check archive discovery for IDF 5's port-wrapper/upstream TLS layout and the
IDF 5/6 crypto archive names. CI runs
them with both ESPHome versions before the ESP8266 matrix build. The compile
fixtures also include custom numeric/text sensors, filters, an automation, and
multiple standard sensor blocks.

## ESPHome configuration and compile matrix

Each configuration uses dummy Wi-Fi/API credentials and a synthetic decryption
key; these values are for compile-only tests and must never be used on a real
network. They build the encrypted receive path for every framework:

| Configuration | Target |
|---|---|
| `test-arduino-esp8266.yaml` | ESP8266 Arduino |
| `test-arduino-esp32.yaml` | ESP32 Arduino |
| `test-espidf-esp32.yaml` | ESP32 ESP-IDF |
| `test-espidf-esp32c6.yaml` | ESP32-C6 ESP-IDF |

Run validation and compile all four:

```bash
for config in test-configs/*.yaml; do
  "$ESPHOME" config "$config" || exit 1
  "$ESPHOME" compile "$config" || exit 1
done
```

The same four builds run in GitHub Actions against the documented minimum
ESPHome release (2025.5.0) and the current release. CI uses Python 3.12 and runs
weekly so new ESPHome releases are checked without waiting for a project change.

Local verification of the 2026-10-02 fixes passed all eight matrix builds with
ESPHome 2025.5.0 and 2026.9.0. Host tests passed 4/4, including under AddressSanitizer
and UndefinedBehaviorSanitizer; configuration regressions passed 6/6 with each
ESPHome version. The example configuration generated with both versions and its
ESP8266 firmware compiled with 2026.9.0 using dummy local secrets and a local
component source.

An additional local check, repeated for the runtime fixes on 2026-10-02, used
ESPHome 2026.9.0's native ESP-IDF toolchain with ESP-IDF 6.0.1 on ESP32. The
firmware compiled and linked. The
reproducible test configuration is `test-configs/manual/test-espidf6-native.yaml`:

```bash
"$ESPHOME" --toolchain esp-idf compile test-configs/manual/test-espidf6-native.yaml
```

The PlatformIO route was also checked with ESP-IDF 6.0.1 using
`test-configs/manual/test-espidf6-pio.yaml`. Its first clean build compiled the
component but stopped at bootloader linking because the generated `bootloader.ld`
script had not been preprocessed. Run this command once to generate the Ninja
files; the first attempt is expected to stop at the bootloader linker-script
error:

```bash
"$ESPHOME" --toolchain platformio compile test-configs/manual/test-espidf6-pio.yaml
```

Then preprocess the generated linker script and retry the build:

```bash
ninja -C "$ESPHOME_DATA_DIR/build/test-espidf6-pio/.pioenvs/test-espidf6-pio/bootloader" bootloader_ld_in_preprocess
"$ESPHOME" --toolchain platformio compile test-configs/manual/test-espidf6-pio.yaml
```

The retry compiled and linked the full firmware. It also ran `post_build.py`,
which found and linked IDF 6's `tfpsacrypto` archive. The workaround affects
only the generated build directory; the native `esp-idf` toolchain builds IDF
6.0.1 without it. ESPHome has deprecated the PlatformIO toolchain and plans to
remove it in 2027.2.0, so native `esp-idf` remains the preferred path. The
2026-10-02 PlatformIO recheck reused the previously preprocessed bootloader
script; it passed the updated component and archive-linking code, but does not
establish that a fresh PlatformIO IDF 6 build no longer needs this workaround.

## Hardware smoke test

On 2026-09-27, the local component was installed over the air on a Slimmelezer
running the Arduino framework. The build and OTA install completed, the device
reconnected to Home Assistant, and live P1 data updated energy, power, voltage,
and current sensors. Consecutive telegrams showed changing live power values.

This check used Home Assistant Core 2026.9.3 and ESPHome Device Builder
2026.9.0. Earlier Home Assistant releases are outside the current support
target. The meter stream was unencrypted, so this does not validate decryption.
### Repeat test on 2026-10-02 — passed

After the parser/publication and receive-loop fixes, the maintainer repeated
installation of the updated local component on the Slimmelezer D1 Mini
(ESP8266, Arduino) using ESPHome Device Builder 2026.9.1. Compilation succeeded;
the maintainer confirmed that the upload completed and the device ran correctly
with unencrypted meter data. This verifies the local working-tree changes,
not an installation from the future `v1.3.0` Git tag. The Home Assistant Core
version was not recorded again for this repeat test.

The supplied install log contains the successful build and start of OTA; the
excerpt ends during upload. Upload completion and runtime operation are based
on the maintainer's confirmation. No post-install runtime log or extended
stability measurement is included in that excerpt.

The initial warning reported an existing PlatformIO HTTP cache directory during
package prefetch. ESPHome fell back to serial downloading for the unresolved
package, and the build completed successfully. This was a package-cache warning
rather than a component compilation failure. The OTA transport was encrypted;
the P1 meter data in this test was unencrypted.

### Procedure for local Slimmelezer checks

1. Keep a copy of the working device YAML and firmware. Copy the updated
   `components/dsmr_custom` directory into the Device Builder configuration
   directory as `components/dsmr_custom`.
2. In the existing device YAML, replace its Git external-component source with
   the local source below. Keep the device name, sensor names/IDs, UART pins,
   network/API settings and existing secrets.

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [dsmr_custom]
```

3. Build and install wirelessly on the existing Slimmelezer. The local source
   above selects the changed component files.
4. Confirm that the device reconnects to the ESPHome API and Home Assistant.
   Watch several consecutive telegrams: energy values should be plausible and
   power/current values should update as the load changes.
5. Check the logs for CRC failures, receive timeouts, buffer overflow/allocation
   errors or reboots. Verify the expected Home Assistant entities and Energy
   dashboard sources still receive values. Record the commit, ESPHome version,
   framework, Home Assistant version and result.
6. After the release tag exists, repeat installation from `ref: v1.3.0` before
   announcing that tag as verified on the device.

This unencrypted-device check does not validate encrypted-meter operation.

## What these tests establish

- ESPHome can validate, generate, compile, and link the component for each
  listed board/framework combination.
- The C++ key parser rejects malformed hexadecimal input; encrypted frame sizing
  rejects empty and oversized payloads.
- The ESP-IDF 5 and PSA Crypto source branches compile in the host harness,
  which also checks an AES-GCM known-answer vector and rejects a bad tag. This
  supplements the successful native ESP-IDF 6.0.1 ESP32 target build above.
- A real Slimmelezer Arduino device received and published unencrypted P1 data
  after the local OTA update, as described above.
- No real meter telegram or decryption key is used by CI.

The Arduino `rweather/Crypto` implementation is compile-tested here, but the
host AES-GCM known-answer test targets the ESP-IDF wrapper. Physical encrypted
meter operation remains experimental and field-unverified because the project
has no hardware that receives encrypted telegrams. Users who test it should
report their board, ESPHome version, framework, meter family, and result.
