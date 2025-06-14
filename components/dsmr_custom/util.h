/**
 * NOTE: This is a vendored and modified version of a file from the
 * glmnet/Dsmr project. The original license is preserved below.
 *
 * Modifications are Copyright (c) 2025 (Niko Paulanne).
 * These modifications are licensed under the GPLv3, as part of the
 * dsmr_custom ESPHome component.
 *
 * Summary of modifications:
 * - Added unique include guards for the dsmr_custom component.
 * - Modified ParseResult and ObisId structs to use ESPHome-style member names
 * with trailing underscores (e.g., result_, next_, err_, ctx_, v_).
 * - Refined DSMR_PROGMEM macro for ESP32/ESP8266 compatibility.
 */

/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file util.h
 * @brief Vendored and modified utility functions and structures from glmnet/arduino-dsmr.
 * @details Provides ObisId, ParseResult, and other helpers. This version has been
 * modified for dsmr_custom to use trailing underscores for member variables for
 * ESPHome C++ style compliance.
 * @author Niko Paulanne
 */

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#ifndef DSMR_CUSTOM_UTIL_H // Changed from DSMR_CUSTOM_PARSER_LIB_UTIL_H for flattened structure
#define DSMR_CUSTOM_UTIL_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#define DSMR_PROGMEM
#else
#define DSMR_PROGMEM PROGMEM
#endif

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <utility> // For std::move

namespace dsmr {

template <typename T, unsigned int sz>
inline unsigned int lengthof(const T (&/*arr*/)[sz]) { return sz; }

static void concat_hack(String& s, const char *append, size_t n) {
  if (n == 0) return;
  char buf[n + 1];
  memcpy(buf, append, n);
  buf[n] = '\0';
  s.concat(buf);
}

template <typename T> struct ParseResult;

template <typename P, typename T>
struct _ParseResult {
  T result_{}; // MODIFIED: Has trailing underscore

  P &succeed(const T& result_val) { // Changed to const T& for safety
    this->result_ = result_val;
    return *static_cast<P *>(this);
  }
  P &succeed(T&& result_val) {
    this->result_ = std::move(result_val);
    return *static_cast<P *>(this);
  }
};

template <typename P>
struct _ParseResult<P, void> { /* No result_ member */ };

template <typename T>
struct ParseResult : public _ParseResult<ParseResult<T>, T> {
  const char *next_{nullptr};                 // MODIFIED: Has trailing underscore
  const __FlashStringHelper *err_{nullptr};   // MODIFIED: Has trailing underscore (and type change)
  const char *ctx_{nullptr};                  // MODIFIED: Has trailing underscore

  ParseResult &fail(const __FlashStringHelper *error_msg, const char* context = nullptr) {
    this->err_ = error_msg;
    this->ctx_ = context;
    return *this;
  }
  ParseResult &until(const char *next_char) {
    this->next_ = next_char;
    return *this;
  }
  ParseResult() = default;
  ParseResult(const ParseResult& other) = default;
  ParseResult(ParseResult&& other) = default;
  ParseResult& operator=(const ParseResult& other) = default;
  ParseResult& operator=(ParseResult&& other) = default;

  template <typename T2>
  ParseResult(const ParseResult<T2>& other): next_(other.next_), err_(other.err_), ctx_(other.ctx_) {}

  String fullError(const char* start, const char* input_end) const {
    String error_string;
    if (this->ctx_ && start && input_end && this->ctx_ >= start && this->ctx_ < input_end) {
      const char *line_end_ptr = this->ctx_;
      while (line_end_ptr < input_end && *line_end_ptr != '\r' && *line_end_ptr != '\n') {
        ++line_end_ptr;
      }
      const char *line_start_ptr = this->ctx_;
      while (line_start_ptr > start && *(line_start_ptr - 1) != '\r' && *(line_start_ptr - 1) != '\n') {
        --line_start_ptr;
      }
      concat_hack(error_string, line_start_ptr, line_end_ptr - line_start_ptr);
      error_string += "\r\n";
      for (const char *p = line_start_ptr; p < this->ctx_; ++p) {
        error_string += ' ';
      }
      error_string += '^';
      error_string += "\r\n";
    }
    if (this->err_) {
      error_string += this->err_;
    }
    return error_string;
  }
};

struct ObisId {
  uint8_t v_[6]; // MODIFIED: Has trailing underscore

  constexpr ObisId(uint8_t a, uint8_t b = 255, uint8_t c = 255, uint8_t d = 255, uint8_t e = 255, uint8_t f = 255)
      : v_{a, b, c, d, e, f} {};
  constexpr ObisId() : v_{0, 0, 0, 0, 0, 0} {}

  bool operator==(const ObisId &other) const {
    return memcmp(v_, other.v_, sizeof(v_)) == 0;
  }
};

} // namespace dsmr

#endif // DSMR_CUSTOM_UTIL_H
