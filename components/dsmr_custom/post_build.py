import os
from SCons.Script import Import

Import("env")

def add_mbedtls_linker_flags(target, source, env):
    """
    Add mbedtls libraries to the linker for the main firmware only.
    This function is called via AddPreAction hook, which ensures it only runs
    when linking firmware.elf, not bootloader.elf.
    """
    print("DSMR Custom: Injecting mbedtls libraries into firmware linker...")
    
    # The build directory where ESP-IDF components are compiled
    # Use BUILD_DIR which points to .pioenvs/<env_name>/
    build_dir = env.subst("$BUILD_DIR")
    
    # MbedTLS libraries are in multiple directories:
    # - libmbedtls.a: esp-idf/mbedtls/
    # - libmbedcrypto.a, libmbedx509.a: esp-idf/mbedtls/mbedtls/library/
    mbedtls_main_dir = os.path.join(build_dir, "esp-idf", "mbedtls")
    mbedtls_lib_dir = os.path.join(build_dir, "esp-idf", "mbedtls", "mbedtls", "library")
    
    # Debug logging
    debug_file = os.path.join(build_dir, "debug_post_build.txt")
    with open(debug_file, "w") as f:
        f.write("Running post_build.py via AddPreAction\n")
        f.write(f"Target: {target}\n")
        f.write(f"BUILD_DIR: {build_dir}\n")
        f.write(f"mbedtls_main_dir: {mbedtls_main_dir}\n")
        f.write(f"mbedtls_lib_dir: {mbedtls_lib_dir}\n")
    
    # Add both directories to the linker search path
    env.Append(LIBPATH=[mbedtls_main_dir, mbedtls_lib_dir])
    
    # CRITICAL: Library order matters for GNU linker!
    # mbedtls depends on mbedx509 and mbedcrypto
    # mbedx509 depends on mbedcrypto
    # Therefore: mbedtls -> mbedx509 -> mbedcrypto (dependency last)
    env.Append(LIBS=["mbedtls", "mbedx509", "mbedcrypto"])
    
    with open(debug_file, "a") as f:
        f.write(f"Added LIBPATH: {mbedtls_main_dir}, {mbedtls_lib_dir}\n")
        f.write("Added LIBS: mbedtls, mbedx509, mbedcrypto (correct order)\n")
    
    print(f"DSMR Custom: Added LIBPATH: {mbedtls_main_dir}")
    print(f"DSMR Custom: Added LIBPATH: {mbedtls_lib_dir}")
    print(f"DSMR Custom: Added LIBS: mbedtls, mbedx509, mbedcrypto")

# Use AddPreAction to hook into the firmware linking stage only
# This ensures the libraries are only added when linking firmware.elf, not bootloader.elf
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", add_mbedtls_linker_flags)
