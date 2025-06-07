/**
 * @file crc16.h
 * @brief Vendored CRC16 calculation functions, adapted for ESPHome dsmr_custom.
 * @details Provides functions for CRC16 checksum calculation, essential for validating
 * DSMR P1 telegram integrity. This version is for direct inclusion in the component.
 *
 * @note This file contains CRC routines originally from avr-libc, adapted via
 * Teensy 3 core and subsequently by Matthijs Kooijman for arduino-dsmr,
 * and then used in glmnet/arduino-dsmr.
 */

/* Original Copyrights and License apply */

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#ifndef DSMR_CUSTOM_CRC16_H // Changed from DSMR_CUSTOM_PARSER_LIB_CRC16_H
#define DSMR_CUSTOM_CRC16_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM

#include <stdint.h>

static inline uint16_t _crc16_update(uint16_t crc, uint8_t data) __attribute__((always_inline, unused));
static inline uint16_t _crc16_update(uint16_t crc, uint8_t data)
{
  unsigned int i;
  crc ^= data;
  for (i = 0; i < 8; ++i) {
    if (crc & 1) {
      crc = (crc >> 1) ^ 0xA001;
    } else {
      crc = (crc >> 1);
    }
  }
  return crc;
}

// Other CRC functions (_crc_xmodem_update, etc.) are omitted for brevity
// if not used by P1Parser, but can be kept if they were in your glmnet/Dsmr version.
// Assuming only _crc16_update is strictly needed by P1Parser.
// If you had the others, they would be here.

#endif // DSMR_CUSTOM_CRC16_H
