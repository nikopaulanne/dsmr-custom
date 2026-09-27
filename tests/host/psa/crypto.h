#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int32_t psa_status_t;
typedef uint32_t psa_algorithm_t;
typedef uint32_t psa_key_type_t;
typedef uint32_t psa_key_usage_t;
typedef uint32_t mbedtls_svc_key_id_t;

typedef struct {
  psa_key_type_t type;
  size_t bits;
  psa_key_usage_t usage;
  psa_algorithm_t algorithm;
} psa_key_attributes_t;

#define PSA_SUCCESS ((psa_status_t)0)
#define PSA_ERROR_GENERIC_ERROR ((psa_status_t)-132)
#define PSA_ERROR_INVALID_ARGUMENT ((psa_status_t)-135)
#define PSA_KEY_ATTRIBUTES_INIT {0, 0, 0, 0}
#define PSA_KEY_TYPE_AES ((psa_key_type_t)1)
#define PSA_KEY_USAGE_DECRYPT ((psa_key_usage_t)1)
#define PSA_ALG_GCM ((psa_algorithm_t)0x10000)
#define PSA_ALG_AEAD_WITH_SHORTENED_TAG(alg, tag_length) \
  ((psa_algorithm_t)((alg) | ((psa_algorithm_t)(tag_length) & 0xff)))

#ifdef __cplusplus
extern "C" {
#endif

psa_status_t psa_crypto_init(void);
void psa_set_key_type(psa_key_attributes_t *attributes, psa_key_type_t type);
void psa_set_key_bits(psa_key_attributes_t *attributes, size_t bits);
void psa_set_key_usage_flags(psa_key_attributes_t *attributes,
                             psa_key_usage_t usage);
void psa_set_key_algorithm(psa_key_attributes_t *attributes,
                           psa_algorithm_t algorithm);
psa_status_t psa_import_key(const psa_key_attributes_t *attributes,
                            const uint8_t *data, size_t data_length,
                            mbedtls_svc_key_id_t *key);
void psa_reset_key_attributes(psa_key_attributes_t *attributes);
psa_status_t psa_aead_decrypt(mbedtls_svc_key_id_t key,
                              psa_algorithm_t algorithm,
                              const uint8_t *nonce, size_t nonce_length,
                              const uint8_t *additional_data,
                              size_t additional_data_length,
                              const uint8_t *ciphertext,
                              size_t ciphertext_length, uint8_t *plaintext,
                              size_t plaintext_size, size_t *plaintext_length);
psa_status_t psa_destroy_key(mbedtls_svc_key_id_t key);

#ifdef __cplusplus
}
#endif
