#include "dsmr_crypto.h"

// Define our custom config file
#define MBEDTLS_CONFIG_FILE "dsmr_mbedtls/dsmr_mbedtls_config.h"

// Wrap MbedTLS implementation in a namespace to avoid symbol conflicts
namespace dsmr_crypto {
extern "C" {
// Include source files directly
// We use relative paths to crypto_bundled/src
// Note: These files will include headers.
// We added crypto_bundled/include to include path, so <dsmr_mbedtls/aes.h>
// works. They also include "common.h", which we copied to src/, so it works.

#include "crypto_bundled/src/aes.c"
#include "crypto_bundled/src/gcm.c"
#include "crypto_bundled/src/platform_util.c"
}
} // namespace dsmr_crypto

// Implementation of our wrapper function using the namespaced mbedtls
extern "C" int dsmr_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                                    const unsigned char *iv, size_t iv_len,
                                    const unsigned char *ciphertext,
                                    size_t ciphertext_len,
                                    const unsigned char *tag, size_t tag_len,
                                    unsigned char *output) {
  dsmr_crypto::mbedtls_gcm_context ctx;
  dsmr_crypto::mbedtls_gcm_init(&ctx);

  int ret = dsmr_crypto::mbedtls_gcm_setkey(
      &ctx, dsmr_crypto::MBEDTLS_CIPHER_ID_AES, key, key_len * 8);
  if (ret != 0) {
    dsmr_crypto::mbedtls_gcm_free(&ctx);
    return ret;
  }

  // mbedtls_gcm_auth_decrypt argument order:
  // ctx, length, iv, iv_len, add, add_len, tag, tag_len, input, output
  ret = dsmr_crypto::mbedtls_gcm_auth_decrypt(&ctx, ciphertext_len, iv, iv_len,
                                              NULL, 0, tag, tag_len, ciphertext,
                                              output);

  dsmr_crypto::mbedtls_gcm_free(&ctx);
  return ret;
}
