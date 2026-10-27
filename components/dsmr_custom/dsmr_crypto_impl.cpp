#ifdef USE_ESP_IDF

#include "dsmr_crypto.h"

#include <stdint.h>
#include <string.h>
#include <esp_idf_version.h>

#if ESP_IDF_VERSION_MAJOR >= 6
#include <psa/crypto.h>
#else
#include <aes/esp_aes_gcm.h>
#endif

namespace {

bool is_valid_gcm_tag_length(size_t tag_len) {
  return tag_len == 4 || tag_len == 8 || (tag_len >= 12 && tag_len <= 16);
}

}  // namespace

extern "C" int dsmr_aes_gcm_decrypt(const unsigned char *key, size_t key_len,
                                    const unsigned char *iv, size_t iv_len,
                                    const unsigned char *ciphertext_and_tag,
                                    size_t ciphertext_len, size_t tag_len,
                                    unsigned char *output) {
#if ESP_IDF_VERSION_MAJOR >= 6
  if (key == nullptr || key_len != 16 || iv == nullptr || iv_len == 0 ||
      ciphertext_and_tag == nullptr || !is_valid_gcm_tag_length(tag_len) ||
      output == nullptr || ciphertext_len > SIZE_MAX - tag_len) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  const psa_status_t init_status = psa_crypto_init();
  if (init_status != PSA_SUCCESS) {
    return init_status;
  }

  const psa_algorithm_t algorithm =
      PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, tag_len);
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&attributes, key_len * 8);
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
  psa_set_key_algorithm(&attributes, algorithm);

  mbedtls_svc_key_id_t key_id;
  psa_status_t status =
      psa_import_key(&attributes, key, key_len, &key_id);
  psa_reset_key_attributes(&attributes);
  if (status != PSA_SUCCESS) {
    return status;
  }

  size_t plaintext_len = 0;
  status = psa_aead_decrypt(key_id, algorithm, iv, iv_len, nullptr, 0,
                            ciphertext_and_tag, ciphertext_len + tag_len,
                            output, ciphertext_len, &plaintext_len);
  const psa_status_t destroy_status = psa_destroy_key(key_id);

  if (status == PSA_SUCCESS && plaintext_len != ciphertext_len) {
    status = PSA_ERROR_GENERIC_ERROR;
  }
  if (status == PSA_SUCCESS && destroy_status != PSA_SUCCESS) {
    status = destroy_status;
  }
  if (status != PSA_SUCCESS && ciphertext_len != 0) {
    memset(output, 0, ciphertext_len);
  }
  return status;
#else
  if (key == nullptr || key_len != 16 || iv == nullptr || iv_len == 0 ||
      ciphertext_and_tag == nullptr || !is_valid_gcm_tag_length(tag_len) ||
      output == nullptr) {
    return -1;
  }

  esp_gcm_context ctx;
  esp_aes_gcm_init(&ctx);

  int ret = esp_aes_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, key_len * 8);
  if (ret != 0) {
    esp_aes_gcm_free(&ctx);
    return ret;
  }

  // esp_aes_gcm_auth_decrypt argument order:
  // ctx, length, iv, iv_len, add, add_len, tag, tag_len, input, output
  ret = esp_aes_gcm_auth_decrypt(
      &ctx, ciphertext_len, iv, iv_len, NULL, 0,
      ciphertext_and_tag + ciphertext_len, tag_len, ciphertext_and_tag,
      output);

  esp_aes_gcm_free(&ctx);
  return ret;
#endif
}

#endif // USE_ESP_IDF
