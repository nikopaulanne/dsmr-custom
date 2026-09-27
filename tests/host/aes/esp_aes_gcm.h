#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef ESP_IDF_VERSION_MAJOR
#define ESP_IDF_VERSION_MAJOR 5
#endif

#if ESP_IDF_VERSION_MAJOR == 5
#define MBEDTLS_CIPHER_ID_AES 0
#endif

typedef struct {
  uint8_t key[32];
  size_t key_bits;
} esp_gcm_context;

#ifdef __cplusplus
extern "C" {
#endif

void esp_aes_gcm_init(esp_gcm_context *ctx);
int esp_aes_gcm_setkey(esp_gcm_context *ctx, int cipher_id,
                       const unsigned char *key, unsigned int key_bits);
int esp_aes_gcm_auth_decrypt(esp_gcm_context *ctx, size_t length,
                             const unsigned char *iv, size_t iv_len,
                             const unsigned char *aad, size_t aad_len,
                             const unsigned char *tag, size_t tag_len,
                             const unsigned char *input,
                             unsigned char *output);
void esp_aes_gcm_free(esp_gcm_context *ctx);

#ifdef __cplusplus
}
#endif
