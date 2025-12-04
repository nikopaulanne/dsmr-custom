# AES-GCM Decryption Implementation Notes

This document details the technical journey and architectural decisions behind implementing AES-GCM decryption for ESP-IDF platforms in this component. It serves as a reference for contributors to understand why certain approaches were taken and others abandoned.

## The Challenge

The goal was to enable encrypted P1 telegram decryption on ESP-IDF platforms (e.g., ESP32-C6, ESP32-H2) to match the functionality available on Arduino builds.

- **Arduino:** Uses `rweather/Crypto` library (works out of the box).
- **ESP-IDF:** Has built-in `mbedtls`, but integrating it via ESPHome's custom component system proved difficult due to linkage and symbol visibility issues.

## Attempted Approaches

We explored six distinct tracks before arriving at a working solution.

### 1. mbedTLS Wrapper Component (Failed)
**Strategy:** Create a local ESP-IDF component (`dsmr_crypto_wrapper`) that wraps the system `mbedtls` with a clean C API.
**Outcome:** ❌ Failed.
**Reason:** ESPHome's build system (via PlatformIO) does not strictly honor ESP-IDF component dependencies (`REQUIRES mbedtls`) for custom components. This resulted in persistent `undefined reference` linker errors for `mbedtls_*` symbols.

### 2. Hardware AES-GCM (Skipped)
**Strategy:** Use the hardware crypto accelerator on ESP32-C6/H2 directly.
**Outcome:** ⏸️ Skipped initially.
**Reason:** Prioritized a guaranteed software fallback. (Note: The final solution actually uses the hardware-accelerated API provided by ESP-IDF).

### 3. TinyCrypt Integration (Abandoned)
**Strategy:** Use Intel's TinyCrypt library as a lightweight, pure C dependency.
**Outcome:** ❌ Abandoned.
**Reason:** TinyCrypt lacks GCM support.

### 4. rweather/Crypto for ESP-IDF (Abandoned)
**Strategy:** Use the existing `rweather/Crypto` library for ESP-IDF builds too.
**Outcome:** ❌ Abandoned.
**Reason:** The library has a hard dependency on `Arduino.h`.

### 5. Vendored MbedTLS Subset (Failed)
**Strategy:** Vendor a minimal subset of MbedTLS (AES + GCM source files) directly into the component, wrapped in a custom C++ namespace.
**Outcome:** ❌ Failed.
**Reason:** **Header Contamination.** The vendored configuration included standard headers which transitively pulled in the system's MbedTLS headers. This caused conflicts between the vendored and system types/macros, leading to massive compilation errors. Wrapping C code in a C++ namespace proved insufficient to isolate it from system libraries.

### 6. System MbedTLS with Extra Scripts (Successful)
**Strategy:** Use ESP-IDF's built-in MbedTLS library, resolving linking issues via PlatformIO `extra_scripts` and using the correct hardware-accelerated API.
**Outcome:** ✅ Success.

**Implementation Details:**
1.  **Bootloader Isolation:** Used SCons `env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", ...)` hook to inject linker flags *only* for the firmware build. This prevents the bootloader build from failing (it doesn't need or support MbedTLS).
2.  **Linker Configuration:** Manually added `libmbedtls.a`, `libmbedx509.a`, and `libmbedcrypto.a` to the linker path (`LIBPATH`) and libraries (`LIBS`).
3.  **Library Order:** Enforced the correct dependency order for the GNU linker: `mbedtls` -> `mbedx509` -> `mbedcrypto`.
4.  **Hardware API:** Discovered that ESP-IDF uses `esp_aes_gcm_*` functions (hardware-accelerated) instead of the standard `mbedtls_gcm_*` API. Switched the implementation to use `<aes/esp_aes_gcm.h>`.

**Benefits:**
- **Performance:** Uses ESP32 hardware AES acceleration (10x faster than software).
- **Size:** No vendored code overhead.
- **Stability:** Uses the official, tested system library.

## Build System Workarounds

To make this work within ESPHome/PlatformIO:
1.  **`post_build.py`:** A Python script registered in `__init__.py` handles the complex SCons environment manipulation required to link the system libraries correctly without breaking the bootloader.
2.  **`sdkconfig.defaults`:** Ensures `CONFIG_MBEDTLS_GCM_C` and `CONFIG_MBEDTLS_AES_C` are enabled.

## Future Considerations

If ESPHome improves support for native ESP-IDF component dependencies in the future (e.g., properly handling `idf_component_register`), the `post_build.py` script might become unnecessary. For now, it provides a robust solution.

---

## Quick Start for Developers

If you're implementing similar ESP-IDF crypto functionality in an ESPHome custom component:

1. **Use System Libraries:** Don't vendor crypto code. Use ESP-IDF's built-in libraries.
2. **Create `post_build.py`:** Register it via `cg.add_platformio_option("extra_scripts", ...)` in `__init__.py`.
3. **Use `AddPreAction`:** Hook into firmware linking, not global environment:
   ```python
   env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", your_linker_function)
   ```
4. **Check Symbol Names:** Use `nm` tool to verify actual symbols in `.a` files:
   ```bash
   riscv32-esp-elf-nm libmbedcrypto.a | grep your_function
   ```
5. **Hardware API:** ESP-IDF often wraps standard APIs (e.g., `esp_aes_gcm_*` instead of `mbedtls_gcm_*`).

## Troubleshooting

### "undefined reference to mbedtls_*"
- **Cause:** Libraries not linked.
- **Fix:** Check that `post_build.py` is registered and runs. Add debug logging to verify execution.

### "Bootloader build fails with mbedtls errors"
- **Cause:** Libraries added globally instead of only for firmware.
- **Fix:** Use `AddPreAction` hook, not `env.Append` at module level.

### "cannot find -lmbedcrypto"
- **Cause:** Wrong library path or library in different directory.
- **Fix:** MbedTLS libraries are split across directories in ESP-IDF. Check both:
  - `esp-idf/mbedtls/` (libmbedtls.a)
  - `esp-idf/mbedtls/mbedtls/library/` (libmbedcrypto.a, libmbedx509.a)

### "Header contamination / type conflicts"
- **Cause:** Trying to vendor MbedTLS while system version exists.
- **Fix:** Don't vendor - use system library with proper linking.

## Key Files Reference

- [`post_build.py`](../components/dsmr_custom/post_build.py) - SCons linker script
- [`dsmr_crypto_impl.cpp`](../components/dsmr_custom/dsmr_crypto_impl.cpp) - ESP-IDF crypto implementation
- [`__init__.py`](../components/dsmr_custom/__init__.py) - Component registration
- [`sdkconfig.defaults`](../components/dsmr_custom/sdkconfig.defaults) - ESP-IDF config

