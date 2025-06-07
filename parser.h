/**
 * @file parser.h
 * @brief Vendored and modified P1 telegram parser core from glmnet/arduino-dsmr.
 * @details Defines the main parsing logic, including the P1Parser class and supporting
 * structures for handling DSMR telegram data. This version includes modifications
 * for enhanced compatibility (e.g., lenient P1 header parsing) and integration
 * within the ESPHome dsmr_custom component. All accesses to ParseResult members
 * have been updated to use trailing underscores (e.g., result_, next_, err_).
 *
 * @note This file is part of a vendored copy of the glmnet/arduino-dsmr parser,
 * which itself is based on matthijskooijman/arduino-dsmr.
 * Original library (glmnet): https://github.com/glmnet/arduino-dsmr
 * It has been included locally in the ESPHome dsmr_custom component.
 * Modifications are marked and explained.
 *
 * Original Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 * Licensed under the MIT License. (Assumed, as per base library)
 */

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#ifndef DSMR_CUSTOM_PARSER_LIB_PARSER_H
#define DSMR_CUSTOM_PARSER_LIB_PARSER_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM

#include "crc16.h"
#include "util.h" // This now defines ParseResult with result_, next_, err_, ctx_, and ObisId with v_
#include "esphome/core/log.h"
#include <ctype.h> // For isalnum

namespace dsmr {

template <typename... Ts>
struct ParsedData;

template <>
struct ParsedData<> {
  ParseResult<void> __attribute__((__always_inline__))
  parse_line_inlined(const ObisId & /* id */, const char *str, const char * /* end */) {
    return ParseResult<void>().until(str);
  }
  template <typename F>
  void __attribute__((__always_inline__)) applyEach_inlined(F && /* f */) {}
  bool all_present_inlined() { return true; }
};

static constexpr char DUPLICATE_FIELD[] DSMR_PROGMEM = "Duplicate field";

template <typename T, typename... Ts>
struct ParsedData<T, Ts...> : public T, ParsedData<Ts...> {
  ParseResult<void> parse_line(const ObisId &id, const char *str, const char *end) {
    return parse_line_inlined(id, str, end);
  }
  ParseResult<void> __attribute__((__always_inline__))
  parse_line_inlined(const ObisId &id, const char *str, const char *end) {
    // Assuming T::id_ is the primary OBIS ID member from modified DEFINE_FIELD in fields.h
    if (id == T::id_) {
      if (T::present())
        return ParseResult<void>().fail(F("Duplicate field"), str);
      T::present() = true;
      return T::parse(str, end);
    }
    return ParsedData<Ts...>::parse_line_inlined(id, str, end);
  }
  template <typename F> void applyEach(F &&f) { applyEach_inlined(f); }
  template <typename F> void __attribute__((__always_inline__)) applyEach_inlined(F &&f) {
    T::apply(f);
    return ParsedData<Ts...>::applyEach_inlined(f);
  }
  bool all_present() { return all_present_inlined(); }
  bool all_present_inlined() { return T::present() && ParsedData<Ts...>::all_present_inlined(); }
};

struct StringParser {
  static ParseResult<String> parse_string(size_t min, size_t max, const char *str, const char *end) {
    ParseResult<String> res;
    if (str >= end || *str != '(')
      return res.fail(F("Missing ("), str);
    const char *str_start = str + 1;
    const char *str_end_ptr = str_start;
    while (str_end_ptr < end && *str_end_ptr != ')')
      ++str_end_ptr;
    if (str_end_ptr == end)
      return res.fail(F("Missing )"), str_end_ptr);
    size_t len = str_end_ptr - str_start;
    if (len < min || len > max)
      return res.fail(F("Invalid string length"), str_start);
    // CORRECTED: Access result_ member
    concat_hack(res.result_, str_start, len);
    return res.until(str_end_ptr + 1);
  }
};

static constexpr char INVALID_NUMBER[] DSMR_PROGMEM = "Invalid number";
static constexpr char INVALID_UNIT[] DSMR_PROGMEM = "Invalid unit";

struct NumParser {
  static ParseResult<uint32_t> parse(size_t max_decimals, const char *unit, const char *str, const char *end) {
    ParseResult<uint32_t> res;
    if (str >= end || *str != '(')
      return res.fail(F("Missing ("), str);
    const char *num_start = str + 1;
    const char *num_end_ptr = num_start;
    uint32_t value = 0;
    while (num_end_ptr < end && !strchr("*.)", *num_end_ptr)) {
      if (*num_end_ptr < '0' || *num_end_ptr > '9')
        return res.fail(F("Invalid number"), num_end_ptr);
      value *= 10;
      value += *num_end_ptr - '0';
      ++num_end_ptr;
    }
    size_t decimals_to_scale = max_decimals;
    if (max_decimals > 0 && num_end_ptr < end && *num_end_ptr == '.') {
      ++num_end_ptr;
      while (num_end_ptr < end && !strchr("*)", *num_end_ptr) && decimals_to_scale > 0) {
        if (*num_end_ptr < '0' || *num_end_ptr > '9')
          return res.fail(F("Invalid number"), num_end_ptr);
        value *= 10;
        value += *num_end_ptr - '0';
        ++num_end_ptr;
        decimals_to_scale--;
      }
    }
    while (decimals_to_scale-- > 0) {
      value *= 10;
    }
    if (unit && *unit) {
      if (num_end_ptr >= end || *num_end_ptr != '*')
        return res.fail(F("Missing unit separator *"), num_end_ptr);
      const char *unit_start = ++num_end_ptr;
      const char *expected_unit_char = unit;
      while (num_end_ptr < end && *num_end_ptr != ')' && *expected_unit_char) {
        if (tolower(static_cast<unsigned char>(*num_end_ptr++)) != tolower(static_cast<unsigned char>(*expected_unit_char++)))
          return res.fail(F("Invalid unit"), unit_start);
      }
      if (*expected_unit_char)
        return res.fail(F("Invalid unit"), unit_start);
    }
    if (num_end_ptr >= end || *num_end_ptr != ')')
      return res.fail(F("Missing ) or extra data"), num_end_ptr);
    return res.succeed(value).until(num_end_ptr + 1);
  }
};

struct ObisIdParser {
  static ParseResult<ObisId> parse(const char *str, const char *end) {
    ParseResult<ObisId> res;
    // CORRECTED: Access result_ and v_ members
    ObisId &id = res.result_;
    res.next_ = str;
    uint8_t part = 0;
    for (uint8_t i = 0; i < 6; ++i) id.v_[i] = 0;
    while (res.next_ < end) {
      char c = *res.next_;
      if (c >= '0' && c <= '9') {
        uint8_t digit = c - '0';
        if (id.v_[part] > 25 || (id.v_[part] == 25 && digit > 5))
          return res.fail(F("Obis ID part > 255"), res.next_);
        id.v_[part] = id.v_[part] * 10 + digit;
      } else if (part == 0 && c == '-') { part++;
      } else if (part == 1 && c == ':') { part++;
      } else if (part > 1 && part < 5 && c == '.') { part++;
      } else { break; }
      ++res.next_;
    }
    if (res.next_ == str)
      return res.fail(F("Empty OBIS id string"), str);
    for (++part; part < 6; ++part)
      id.v_[part] = 255;
    return res;
  }
};

struct CrcParser {
  static const size_t CRC_LEN = 4;
  static ParseResult<uint16_t> parse(const char *str, const char *end) {
    ParseResult<uint16_t> res;
    if (str + CRC_LEN > end)
      return res.fail(F("Insufficient data for checksum"), str);
    char buf[CRC_LEN + 1];
    memcpy(buf, str, CRC_LEN);
    buf[CRC_LEN] = '\0';
    char *endp;
    uint16_t check = static_cast<uint16_t>(std::strtoul(buf, &endp, 16));
    if (endp != buf + CRC_LEN)
      return res.fail(F("Malformed checksum string"), str);
    // CORRECTED: Access next_ member
    res.next_ = str + CRC_LEN;
    return res.succeed(check);
  }
};

struct P1Parser {
  template <typename... Ts>
  static ParseResult<void> parse(ParsedData<Ts...> *data, const char *str, size_t n, bool unknown_error = false,
                                   bool check_crc = true) {
    ParseResult<void> res;
    if (!n || str[0] != '/')
      return res.fail(F("Data should start with /"), str);
    const char *data_start = str + 1;
    const char *data_end_ptr = data_start;
    if (check_crc) {
      uint16_t calculated_crc = _crc16_update(0, *str);
      while (data_end_ptr < str + n && *data_end_ptr != '!') {
        calculated_crc = _crc16_update(calculated_crc, *data_end_ptr);
        ++data_end_ptr;
      }
      if (data_end_ptr >= str + n)
        return res.fail(F("Missing '!' telegram terminator (CRC check)"), data_end_ptr);
      calculated_crc = _crc16_update(calculated_crc, *data_end_ptr);
      ParseResult<uint16_t> crc_from_telegram_res = CrcParser::parse(data_end_ptr + 1, str + n);
      // CORRECTED: Access err_ member
      if (crc_from_telegram_res.err_)
        return crc_from_telegram_res;
      // CORRECTED: Access result_ member
      if (crc_from_telegram_res.result_ != calculated_crc) {
        ESP_LOGW("dsmr_parser", "Checksum mismatch! Expected: %04X, Received: %04X", calculated_crc, crc_from_telegram_res.result_);
        return res.fail(F("Checksum mismatch"), data_end_ptr + 1);
      }
      // CORRECTED: Access next_ member
      res.next_ = crc_from_telegram_res.next_;
    } else {
      while (data_end_ptr < str + n && *data_end_ptr != '!') {
        ++data_end_ptr;
      }
      if (data_end_ptr >= str + n)
        return res.fail(F("Missing '!' telegram terminator (no CRC check)"), data_end_ptr);
      // CORRECTED: Access next_ member
      res.next_ = data_end_ptr + 1;
    }
    ParseResult<void> data_parse_res = parse_data(data, data_start, data_end_ptr, unknown_error);
    // CORRECTED: Access err_ member
    if (data_parse_res.err_) return data_parse_res;
    return res;
  }

  template <typename... Ts>
  static ParseResult<void> parse_data(ParsedData<Ts...> *data, const char *str, const char *end,
                                      bool unknown_error = false) {
    ParseResult<void> res;
    const char *current_pos = str;
    const char *line_start = str;
    const char *id_line_end = current_pos;
    while (id_line_end < end && *id_line_end != '\r' && *id_line_end != '\n') {
        id_line_end++;
    }
    if (current_pos == id_line_end && current_pos < end) {
        return res.fail(F("Empty identification line"), current_pos);
    }
    // BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM (Lenient Header Parsing)
    if (line_start + 3 >= id_line_end) {
         if (line_start >= id_line_end) {
            return res.fail(F("Identification line too short or missing"), line_start);
         }
    } else {
        if (!isalnum(static_cast<unsigned char>(line_start[0])) ||
            !isalnum(static_cast<unsigned char>(line_start[1])) ||
            !isalnum(static_cast<unsigned char>(line_start[2]))) {
            return res.fail(F("Invalid Manufacturer ID in identification"), line_start);
        }
        if (!isalnum(static_cast<unsigned char>(line_start[3]))) {
            return res.fail(F("Invalid char after Manufacturer ID in identification"), line_start + 3);
        }
    }
    if (line_start < id_line_end) {
        ParseResult<void> id_line_parse_res = data->parse_line(ObisId(255, 255, 255, 255, 255, 255), line_start, id_line_end);
        // CORRECTED: Access err_ member
        if (id_line_parse_res.err_) {
            return id_line_parse_res;
        }
    }
    // END MODIFICATION FOR ESPHOME DSMR_CUSTOM
    current_pos = id_line_end;
    while (current_pos < end && (*current_pos == '\r' || *current_pos == '\n')) {
        current_pos++;
    }
    line_start = current_pos;
    while (line_start < end) {
        const char *line_content_end = line_start;
        while (line_content_end < end && *line_content_end != '\r' && *line_content_end != '\n') {
            line_content_end++;
        }
        if (line_start < line_content_end) {
            ParseResult<void> single_line_res = parse_line_content(data, line_start, line_content_end, unknown_error);
            // CORRECTED: Access err_ member
            if (single_line_res.err_) {
                return single_line_res;
            }
        }
        current_pos = line_content_end;
        while (current_pos < end && (*current_pos == '\r' || *current_pos == '\n')) {
            current_pos++;
        }
        line_start = current_pos;
        if (line_start >= end) break;
    }
    if (line_start != end) {
        return res.fail(F("Last data line not properly terminated or unexpected data before '!'"), line_start);
    }
    return res.until(end);
  }

  template <typename Data>
  static ParseResult<void> parse_line_content(Data *data, const char *line, const char *end, bool unknown_error) {
    ParseResult<void> res;
    if (line == end) return res;
    ParseResult<ObisId> id_res = ObisIdParser::parse(line, end);
    // CORRECTED: Access err_, result_, next_ members
    if (id_res.err_) return id_res;
    ParseResult<void> data_res = data->parse_line(id_res.result_, id_res.next_, end);
    if (data_res.err_) return data_res;
    if (data_res.next_ != id_res.next_ && data_res.next_ != end) {
      return res.fail(F("Trailing characters on data line"), data_res.next_);
    } else if (data_res.next_ == id_res.next_ && unknown_error) {
      return res.fail(F("Unknown OBIS field"), line);
    }
    return res.until(end);
  }
};

}  // namespace dsmr

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#endif  // DSMR_CUSTOM_PARSER_LIB_PARSER_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM
