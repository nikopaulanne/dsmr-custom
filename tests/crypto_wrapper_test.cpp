#include "dsmr_crypto.h"

#include <stdio.h>
#include <string.h>

namespace {

bool test_nist_aes128_gcm_vector() {
  const unsigned char key[16] = {};
  const unsigned char iv[12] = {};
  const unsigned char ciphertext_and_tag[28] = {
      0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
      0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
      0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec,
      0x13, 0xbd, 0xf5, 0x3a, 0x67, 0xb2,
  };
  const unsigned char expected[16] = {};
  unsigned char plaintext[sizeof(ciphertext_and_tag) - 12] = {};

  const int result = dsmr_aes_gcm_decrypt(
      key, sizeof(key), iv, sizeof(iv), ciphertext_and_tag, 16, 12, plaintext);
  return result == 0 && memcmp(plaintext, expected, sizeof(expected)) == 0;
}

bool test_rejects_modified_tag() {
  const unsigned char key[16] = {};
  const unsigned char iv[12] = {};
  unsigned char ciphertext_and_tag[28] = {
      0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
      0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
      0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec,
      0x13, 0xbd, 0xf5, 0x3a, 0x67, 0xb2,
  };
  unsigned char plaintext[16] = {};
  ciphertext_and_tag[16] ^= 1;

  return dsmr_aes_gcm_decrypt(key, sizeof(key), iv, sizeof(iv),
                              ciphertext_and_tag, 16, 12, plaintext) != 0;
}

}  // namespace

int main() {
  if (!test_nist_aes128_gcm_vector()) {
    fprintf(stderr, "AES-GCM NIST known-answer test failed\n");
    return 1;
  }
  if (!test_rejects_modified_tag()) {
    fprintf(stderr, "AES-GCM accepted a modified authentication tag\n");
    return 1;
  }
  puts("AES-GCM wrapper known-answer and tag-rejection tests passed");
  return 0;
}
