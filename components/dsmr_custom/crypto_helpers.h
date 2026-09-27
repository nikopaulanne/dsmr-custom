#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace esphome {
namespace dsmr_custom {

constexpr size_t DSMR_AES128_KEY_SIZE = 16;
constexpr size_t DSMR_ENCRYPTED_FRAME_HEADER_SIZE = 18;
constexpr size_t DSMR_ENCRYPTED_FRAME_TAG_SIZE = 12;

inline int hex_digit_value(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

inline bool parse_aes128_key(const std::string &hex, uint8_t *key) {
  if (key == nullptr || hex.size() != DSMR_AES128_KEY_SIZE * 2)
    return false;

  for (size_t i = 0; i < DSMR_AES128_KEY_SIZE; i++) {
    const int high = hex_digit_value(hex[i * 2]);
    const int low = hex_digit_value(hex[i * 2 + 1]);
    if (high < 0 || low < 0)
      return false;
    key[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

inline bool encrypted_frame_size(size_t ciphertext_size, size_t max_frame_size,
                                 size_t *frame_size) {
  const size_t overhead = DSMR_ENCRYPTED_FRAME_HEADER_SIZE +
                          DSMR_ENCRYPTED_FRAME_TAG_SIZE;
  if (frame_size == nullptr || ciphertext_size == 0 ||
      max_frame_size < overhead || ciphertext_size > max_frame_size - overhead)
    return false;

  *frame_size = overhead + ciphertext_size;
  return true;
}

}  // namespace dsmr_custom
}  // namespace esphome
