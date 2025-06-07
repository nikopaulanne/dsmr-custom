/**
 * NOTE: This is a vendored version of a file with origins in avr-libc,
 * adapted for the dsmr_custom component. The original license is preserved below.
 *
 * Modifications are Copyright (c) 2025 (Niko Paulanne).
 * These modifications are licensed under the GPLv3, as part of the
 * dsmr_custom ESPHome component.
 *
 * Summary of modifications:
 * - Added unique include guards for the dsmr_custom component.
 */

/* CRC compatibility, adapted from the Teensy 3 core at:
   https://github.com/PaulStoffregen/cores/tree/master/teensy3
   which was in turn adapted by Paul Stoffregen from the C-only comments here:
   http://svn.savannah.nongnu.org/viewvc/trunk/avr-libc/include/util/crc16.h?revision=933&root=avr-libc&view=markup */

/* Copyright (c) 2002, 2003, 2004  Marek Michalkiewicz
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */

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
