import os
from SCons.Script import Import

Import("env")

def find_static_library(component_build_dir, library_name):
    """Find one ESP-IDF component archive and return its containing directory."""
    archive_name = f"lib{library_name}.a"
    matches = []
    for current_dir, _, files in os.walk(component_build_dir):
        if archive_name in files:
            matches.append(current_dir)

    if len(matches) > 1:
        raise RuntimeError(
            f"DSMR Custom: found multiple {archive_name} archives under "
            f"{component_build_dir}: {matches}"
        )
    return matches[0] if matches else None


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
    
    mbedtls_build_dir = os.path.join(build_dir, "esp-idf", "mbedtls")
    required_libraries = ["mbedtls", "mbedx509"]
    crypto_library = next(
        (
            name
            for name in ("tfpsacrypto", "mbedcrypto")
            if find_static_library(mbedtls_build_dir, name) is not None
        ),
        None,
    )
    if crypto_library is None:
        raise RuntimeError(
            "DSMR Custom: could not find libtfpsacrypto.a or "
            f"libmbedcrypto.a under {mbedtls_build_dir}"
        )
    required_libraries.append(crypto_library)

    library_paths = {}
    for library in required_libraries:
        library_path = find_static_library(mbedtls_build_dir, library)
        if library_path is None:
            raise RuntimeError(
                f"DSMR Custom: could not find lib{library}.a under "
                f"{mbedtls_build_dir}"
            )
        library_paths[library] = library_path

    library_dirs = list(dict.fromkeys(library_paths.values()))
    
    # Debug logging
    debug_file = os.path.join(build_dir, "debug_post_build.txt")
    with open(debug_file, "w") as f:
        f.write("Running post_build.py via AddPreAction\n")
        f.write(f"Target: {target}\n")
        f.write(f"BUILD_DIR: {build_dir}\n")
        f.write(f"mbedtls_build_dir: {mbedtls_build_dir}\n")
    
    # ESP-IDF 5 calls its crypto archive mbedcrypto; ESP-IDF 6 uses tfpsacrypto.
    env.AppendUnique(LIBPATH=library_dirs)
    
    # CRITICAL: Library order matters for GNU linker!
    # Therefore: mbedtls -> mbedx509 -> crypto implementation (dependency last).
    env.Append(LIBS=required_libraries)
    
    with open(debug_file, "a") as f:
        f.write(f"Added LIBPATH: {', '.join(library_dirs)}\n")
        f.write(f"Added LIBS: {', '.join(required_libraries)}\n")
    
    print(f"DSMR Custom: Added LIBPATH: {', '.join(library_dirs)}")
    print(f"DSMR Custom: Added LIBS: {', '.join(required_libraries)}")

# Use AddPreAction to hook into the firmware linking stage only
# This ensures the libraries are only added when linking firmware.elf, not bootloader.elf
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", add_mbedtls_linker_flags)
