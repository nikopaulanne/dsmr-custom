#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decrypt AES-128-GCM data through the framework-specific implementation.
 *
 * @param key Decryption key (16 bytes for AES-128)
 * @param key_len Length of key in bytes
 * @param iv Initialization Vector
 * @param iv_len Length of IV in bytes
 * @param ciphertext_and_tag Ciphertext followed immediately by its
 * authentication tag
 * @param ciphertext_len Length of ciphertext, excluding the tag
 * @param tag_len Length of tag
 * @param output Buffer to store decrypted data (must be at least
 * ciphertext_len)
 * @return 0 on success, nonzero framework error code on failure
 */
int dsmr_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                         const unsigned char *iv, size_t iv_len,
                         const unsigned char *ciphertext_and_tag,
                         size_t ciphertext_len, size_t tag_len,
                         unsigned char *output);

#ifdef __cplusplus
}
#endif
