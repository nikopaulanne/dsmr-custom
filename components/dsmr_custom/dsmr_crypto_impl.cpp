#ifdef USE_ESP_IDF

#include "dsmr_crypto.h"

// ESP-IDF's MbedTLS uses hardware-accelerated esp_aes_gcm functions
// Include the ESP32-specific header
#include <aes/esp_aes_gcm.h>

extern "C" int dsmr_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                                    const unsigned char *iv, size_t iv_len,
                                    const unsigned char *ciphertext,
                                    size_t ciphertext_len,
                                    const unsigned char *tag, size_t tag_len,
                                    unsigned char *output) {
  esp_gcm_context ctx;
  esp_aes_gcm_init(&ctx);

  int ret = esp_aes_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, key_len * 8);
  if (ret != 0) {
    esp_aes_gcm_free(&ctx);
    return ret;
  }

  // esp_aes_gcm_auth_decrypt argument order:
  // ctx, length, iv, iv_len, add, add_len, tag, tag_len, input, output
  ret = esp_aes_gcm_auth_decrypt(&ctx, ciphertext_len, iv, iv_len, NULL, 0, tag,
                                 tag_len, ciphertext, output);

  esp_aes_gcm_free(&ctx);
  return ret;
}

#endif // USE_ESP_IDF
