#ifndef DSMR_MBEDTLS_CONFIG_H
#define DSMR_MBEDTLS_CONFIG_H

// Enable AES and GCM
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C

// Enable Platform support (needed for calloc/free/zeroize)
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

// We use standard C library functions
#include <stdlib.h>
#include <string.h>
#define MBEDTLS_PLATFORM_STD_CALLOC calloc
#define MBEDTLS_PLATFORM_STD_FREE free
#define MBEDTLS_PLATFORM_STD_MEMSET memset
#define MBEDTLS_PLATFORM_STD_MEMCPY memcpy
#define MBEDTLS_PLATFORM_STD_MEMMOVE memmove
#define MBEDTLS_PLATFORM_STD_SNPRINTF snprintf
#define MBEDTLS_PLATFORM_STD_PRINTF printf
#define MBEDTLS_PLATFORM_STD_EXIT exit

#include "mbedtls/check_config.h"

#endif
