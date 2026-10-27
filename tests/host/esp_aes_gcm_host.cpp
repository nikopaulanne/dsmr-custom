#include "aes/esp_aes_gcm.h"

#include <openssl/evp.h>

#include <string.h>

extern "C" void esp_aes_gcm_init(esp_gcm_context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
}

extern "C" int esp_aes_gcm_setkey(esp_gcm_context *ctx, int cipher_id,
                                   const unsigned char *key,
                                   unsigned int key_bits) {
#if ESP_IDF_VERSION_MAJOR == 5
  constexpr int expected_cipher_id = MBEDTLS_CIPHER_ID_AES;
#elif ESP_IDF_VERSION_MAJOR == 6
  constexpr int expected_cipher_id = 0;
#endif
  if (cipher_id != expected_cipher_id || key == nullptr || key_bits != 128)
    return -1;
  memcpy(ctx->key, key, 16);
  ctx->key_bits = key_bits;
  return 0;
}

extern "C" int esp_aes_gcm_auth_decrypt(
    esp_gcm_context *ctx, size_t length, const unsigned char *iv,
    size_t iv_len, const unsigned char *aad, size_t aad_len,
    const unsigned char *tag, size_t tag_len, const unsigned char *input,
    unsigned char *output) {
  if (ctx == nullptr || ctx->key_bits != 128 || iv == nullptr || iv_len == 0 ||
      tag == nullptr || tag_len < 4 || (length != 0 && (input == nullptr || output == nullptr)) ||
      (aad_len != 0 && aad == nullptr))
    return -1;

  EVP_CIPHER_CTX *cipher = EVP_CIPHER_CTX_new();
  if (cipher == nullptr)
    return -1;

  int result = -1;
  int written = 0;
  int total_written = 0;
  if (EVP_DecryptInit_ex(cipher, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv_len), nullptr) != 1 ||
      EVP_DecryptInit_ex(cipher, nullptr, nullptr, ctx->key, iv) != 1)
    goto cleanup;

  if (aad_len != 0 &&
      EVP_DecryptUpdate(cipher, nullptr, &written, aad, static_cast<int>(aad_len)) != 1)
    goto cleanup;

  if (length != 0) {
    if (EVP_DecryptUpdate(cipher, output, &written, input,
                          static_cast<int>(length)) != 1)
      goto cleanup;
    total_written = written;
  }

  if (EVP_CIPHER_CTX_ctrl(cipher, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag_len),
                          const_cast<unsigned char *>(tag)) != 1)
    goto cleanup;
  if (EVP_DecryptFinal_ex(cipher, output == nullptr ? nullptr : output + total_written,
                          &written) != 1)
    goto cleanup;

  result = 0;

cleanup:
  EVP_CIPHER_CTX_free(cipher);
  return result;
}

extern "C" void esp_aes_gcm_free(esp_gcm_context *ctx) {
  if (ctx != nullptr)
    memset(ctx, 0, sizeof(*ctx));
}
