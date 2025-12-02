# ESP-IDF Framework Limitations

## Encrypted Telegram Decryption

**Status**: ❌ Not Supported

### Problem

ESP-IDF builds cannot decrypt AES-GCM encrypted P1 telegrams due to `mbedtls` library linkage issues in ESPHome's PlatformIO build system.

### Technical Details

The `mbedtls` library is part of ESP-IDF but is not automatically linked to custom ESPHome components. Multiple linkage methods were attempted:

1. ❌ Build flag `-lmbedtls` → Bootloader compilation error  
2. ❌ `MBEDTLS_GCM_C` define → Unresolved symbols
3. ❌ CMakeLists.txt `idf_component_register(REQUIRES mbedtls)` → Not honored by ESPHome
4. ❌ `sdkconfig_options` → Config applied but linkage fails
5. ❌ `noise-c` library dependency → Different symbols, doesn't help

**Root Cause**: ESPHome generates PlatformIO builds that don't fully expose ESP-IDF's CMake component system. Custom components cannot declare ESP-IDF component dependencies.

### Workarounds

**Option 1: Use Arduino Framework** (Recommended)
```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
```

**Option 2: Disable Encryption on Smart Meter**
- Check your meter's manual for instructions
- Not all meters support this

**Option 3: Community Fix**
- Track issue: https://github.com/nikopaulanne/dsmr-custom/issues/[TBD]
- Requires upstream ESPHome changes

### Future  

This limitation may be resolved if:
1. ESPHome improves ESP-IDF component dependency system
2. Community contributes working mbedtls linkage method
3. Alternative crypto library with pure C++ implementation emerges

**Contributions welcome!** 🙏
