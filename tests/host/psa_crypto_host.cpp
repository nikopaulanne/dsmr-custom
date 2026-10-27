#include <psa/crypto.h>

#include <openssl/evp.h>

#include <string.h>

namespace {

unsigned char imported_key[16] = {};
bool key_imported = false;
constexpr mbedtls_svc_key_id_t HOST_KEY_ID = 1;

}  // namespace

extern "C" psa_status_t psa_crypto_init(void) { return PSA_SUCCESS; }

extern "C" void psa_set_key_type(psa_key_attributes_t *attributes,
                                  psa_key_type_t type) {
  attributes->type = type;
}

extern "C" void psa_set_key_bits(psa_key_attributes_t *attributes,
                                  size_t bits) {
  attributes->bits = bits;
}

extern "C" void psa_set_key_usage_flags(psa_key_attributes_t *attributes,
                                        psa_key_usage_t usage) {
  attributes->usage = usage;
}

extern "C" void psa_set_key_algorithm(psa_key_attributes_t *attributes,
                                      psa_algorithm_t algorithm) {
  attributes->algorithm = algorithm;
}

extern "C" psa_status_t psa_import_key(
    const psa_key_attributes_t *attributes, const uint8_t *data,
    size_t data_length, mbedtls_svc_key_id_t *key) {
  if (attributes == nullptr || data == nullptr || key == nullptr ||
      attributes->type != PSA_KEY_TYPE_AES || attributes->bits != 128 ||
      attributes->usage != PSA_KEY_USAGE_DECRYPT || data_length != 16 ||
      (attributes->algorithm & ~0xffu) != PSA_ALG_GCM || key_imported) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }
  memcpy(imported_key, data, sizeof(imported_key));
  key_imported = true;
  *key = HOST_KEY_ID;
  return PSA_SUCCESS;
}

extern "C" void psa_reset_key_attributes(psa_key_attributes_t *attributes) {
  if (attributes != nullptr)
    memset(attributes, 0, sizeof(*attributes));
}

extern "C" psa_status_t psa_aead_decrypt(
    mbedtls_svc_key_id_t key, psa_algorithm_t algorithm,
    const uint8_t *nonce, size_t nonce_length, const uint8_t *additional_data,
    size_t additional_data_length, const uint8_t *ciphertext,
    size_t ciphertext_length, uint8_t *plaintext, size_t plaintext_size,
    size_t *plaintext_length) {
  const size_t tag_length = algorithm & 0xffu;
  if (key != HOST_KEY_ID || !key_imported ||
      (algorithm & ~0xffu) != PSA_ALG_GCM || tag_length < 4 || tag_length > 16 ||
      nonce == nullptr || nonce_length == 0 || ciphertext == nullptr ||
      ciphertext_length < tag_length || plaintext == nullptr ||
      plaintext_length == nullptr ||
      plaintext_size < ciphertext_length - tag_length ||
      (additional_data_length != 0 && additional_data == nullptr)) {
    return PSA_ERROR_INVALID_ARGUMENT;
  }

  const size_t encrypted_length = ciphertext_length - tag_length;
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (ctx == nullptr)
    return PSA_ERROR_GENERIC_ERROR;

  psa_status_t status = PSA_ERROR_GENERIC_ERROR;
  int written = 0;
  int total_written = 0;
  if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                          static_cast<int>(nonce_length), nullptr) != 1 ||
      EVP_DecryptInit_ex(ctx, nullptr, nullptr, imported_key, nonce) != 1) {
    goto cleanup;
  }
  if (additional_data_length != 0 &&
      EVP_DecryptUpdate(ctx, nullptr, &written, additional_data,
                        static_cast<int>(additional_data_length)) != 1) {
    goto cleanup;
  }
  if (encrypted_length != 0) {
    if (EVP_DecryptUpdate(ctx, plaintext, &written, ciphertext,
                          static_cast<int>(encrypted_length)) != 1) {
      goto cleanup;
    }
    total_written = written;
  }
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                          static_cast<int>(tag_length),
                          const_cast<uint8_t *>(ciphertext + encrypted_length)) !=
      1) {
    goto cleanup;
  }
  if (EVP_DecryptFinal_ex(ctx, plaintext + total_written, &written) != 1)
    goto cleanup;

  *plaintext_length = static_cast<size_t>(total_written + written);
  status = PSA_SUCCESS;

cleanup:
  EVP_CIPHER_CTX_free(ctx);
  return status;
}

extern "C" psa_status_t psa_destroy_key(mbedtls_svc_key_id_t key) {
  if (key != HOST_KEY_ID || !key_imported)
    return PSA_ERROR_INVALID_ARGUMENT;
  memset(imported_key, 0, sizeof(imported_key));
  key_imported = false;
  return PSA_SUCCESS;
}
