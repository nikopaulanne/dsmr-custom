#include "crypto_helpers.h"

#include <stdio.h>
#include <string>

namespace {

bool test_key_parsing() {
  const std::string input = "00112233445566778899aAbBcCdDeEfF";
  uint8_t key[esphome::dsmr_custom::DSMR_AES128_KEY_SIZE] = {};
  if (!esphome::dsmr_custom::parse_aes128_key(input, key))
    return false;
  for (size_t i = 0; i < sizeof(key); i++) {
    if (key[i] != i * 0x11)
      return false;
  }
  return true;
}

bool test_invalid_keys_are_rejected() {
  uint8_t key[esphome::dsmr_custom::DSMR_AES128_KEY_SIZE] = {};
  return !esphome::dsmr_custom::parse_aes128_key("0011", key) &&
         !esphome::dsmr_custom::parse_aes128_key(
             "00112233445566778899aabbccddeefg", key) &&
         !esphome::dsmr_custom::parse_aes128_key(
             "00112233445566778899aabbccddeeff", nullptr);
}

bool test_frame_size_bounds() {
  size_t frame_size = 0;
  const size_t max_frame = 1700;
  return esphome::dsmr_custom::encrypted_frame_size(1, 31, &frame_size) &&
         frame_size == 31 &&
         !esphome::dsmr_custom::encrypted_frame_size(1, 30, &frame_size) &&
         !esphome::dsmr_custom::encrypted_frame_size(0, max_frame, &frame_size) &&
         esphome::dsmr_custom::encrypted_frame_size(
             max_frame - esphome::dsmr_custom::DSMR_ENCRYPTED_FRAME_HEADER_SIZE -
                 esphome::dsmr_custom::DSMR_ENCRYPTED_FRAME_TAG_SIZE,
             max_frame, &frame_size) &&
         frame_size == max_frame;
}

}  // namespace

int main() {
  if (!test_key_parsing() || !test_invalid_keys_are_rejected() ||
      !test_frame_size_bounds()) {
    fprintf(stderr, "Crypto helper test failed\n");
    return 1;
  }
  puts("Crypto helper tests passed");
  return 0;
}
