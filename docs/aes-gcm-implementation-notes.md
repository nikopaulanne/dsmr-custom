# AES-GCM Decryption Implementation Notes

This document details the technical journey and architectural decisions behind implementing AES-GCM decryption for ESP-IDF platforms in this component. It serves as a reference for contributors to understand why certain approaches were taken and others abandoned.

## The Challenge

The goal was to enable encrypted P1 telegram decryption on ESP-IDF platforms (e.g., ESP32-C6, ESP32-H2) to match the functionality available on Arduino builds.

- **Arduino:** Uses `rweather/Crypto` library (works out of the box).
- **ESP-IDF:** Has built-in `mbedtls`, but integrating it via ESPHome's custom component system proved difficult due to linkage and symbol visibility issues.

## Attempted Approaches

We explored five distinct tracks before arriving at a working solution.

### 1. mbedTLS Wrapper Component (Failed)
**Strategy:** Create a local ESP-IDF component (`dsmr_crypto_wrapper`) that wraps the system `mbedtls` with a clean C API.
**Outcome:** ❌ Failed.
**Reason:** ESPHome's build system (via PlatformIO) does not strictly honor ESP-IDF component dependencies (`REQUIRES mbedtls`) for custom components. This resulted in persistent `undefined reference` linker errors for `mbedtls_*` symbols, even though the code compiled successfully.

### 2. Hardware AES-GCM (Skipped)
**Strategy:** Use the hardware crypto accelerator on ESP32-C6/H2.
**Outcome:** ⏸️ Skipped.
**Reason:** Prioritized a guaranteed software fallback that works across all ESP-IDF chips over a hardware-specific implementation that might vary by chip revision.

### 3. TinyCrypt Integration (Abandoned)
**Strategy:** Use Intel's TinyCrypt library as a lightweight, pure C dependency.
**Outcome:** ❌ Abandoned.
**Reason:** TinyCrypt supports AES-CCM, CBC, and CTR, but **lacks GCM support**. Implementing GCM (GHASH) on top of it was deemed too complex and error-prone.

### 4. rweather/Crypto for ESP-IDF (Abandoned)
**Strategy:** Use the existing `rweather/Crypto` library for ESP-IDF builds too.
**Outcome:** ❌ Abandoned.
**Reason:** The library has a hard dependency on `Arduino.h` and the Arduino framework, making it incompatible with pure ESP-IDF builds.

### 5. Vendored MbedTLS Subset (Successful)
**Strategy:** Vendor a minimal subset of MbedTLS (AES + GCM source files) directly into the component, wrapped in a custom C++ namespace.
**Outcome:** ✅ Success.
**Implementation Details:**
- **Source:** MbedTLS v2.28.8 (LTS).
- **Files:** `aes.c`, `gcm.c`, `platform_util.c`, and related headers.
- **Namespace:** All code is wrapped in `namespace dsmr_crypto` to prevent symbol collisions with the ESP-IDF's built-in MbedTLS.
- **Config:** A minimal `dsmr_mbedtls_config.h` enables only AES and GCM to keep binary size low.

## Build System Workarounds

To make this work within ESPHome/PlatformIO:
1. **Include Paths:** We explicitly add the vendored include path in `__init__.py` using `cg.add_build_flag("-I...")`.
2. **Direct Compilation:** We do not rely on library linking. Instead, the implementation file `dsmr_crypto_impl.cpp` directly includes the `.c` source files. This ensures they are compiled with our custom configuration and namespace.

## Future Considerations

If ESPHome improves support for native ESP-IDF component dependencies in the future, we could revert to using the system `mbedtls` library. For now, the vendored approach provides the most stability and portability.
