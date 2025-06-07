/*
 * This file is part of the dsmr_custom ESPHome component.
 *
 * Copyright (c) 2025 (Niko Paulanne)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
 * @file fields.h
 * @brief Vendored and modified field definitions from glmnet/arduino-dsmr.
 * @details Defines structures and macros for representing DSMR P1 telegram data fields.
 * This version includes modifications to the DEFINE_FIELD macro for ESPHome
 * compatibility and uses custom preprocessor defines for M-Bus channel IDs.
 * @author Niko Paulanne
 * @note This file is part of a vendored copy of the glmnet/arduino-dsmr parser.
 * It has been included and modified for the dsmr_custom ESPHome component.
 */

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#ifndef DSMR_CUSTOM_FIELDS_H // Changed from DSMR_CUSTOM_PARSER_LIB_FIELDS_H
#define DSMR_CUSTOM_FIELDS_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM

#include "util.h"   // For ObisId, ParseResult (with result_, next_, err_, ctx_), String, DSMR_PROGMEM.
#include "parser.h" // For StringParser, NumParser, needed by field type templates.

#ifndef DSMR_CUSTOM_GAS_MBUS_ID
#define DSMR_CUSTOM_GAS_MBUS_ID 1
#endif
#ifndef DSMR_CUSTOM_WATER_MBUS_ID
#define DSMR_CUSTOM_WATER_MBUS_ID 2
#endif
#ifndef DSMR_CUSTOM_THERMAL_MBUS_ID
#define DSMR_CUSTOM_THERMAL_MBUS_ID 3
#endif
#ifndef DSMR_CUSTOM_SUB_MBUS_ID
#define DSMR_CUSTOM_SUB_MBUS_ID 4
#endif

namespace dsmr {

template <typename T>
struct ParsedField {
  template <typename F>
  void apply(F &f) { f.apply(*static_cast<T *>(this)); }
  static const char *unit() { return ""; }
};

template <typename T, size_t minlen, size_t maxlen>
struct StringField : ParsedField<T> {
  ParseResult<void> parse(const char *str, const char *end) {
    ParseResult<String> res = StringParser::parse_string(minlen, maxlen, str, end);
    if (!res.err_) // CORRECTED: Access err_
      static_cast<T *>(this)->val() = res.result_; // CORRECTED: Access result_
    return res;
  }
};

template <typename T>
struct TimestampField : StringField<T, 13, 13> {};

struct FixedValue {
  operator float() const { return val(); }
  float val() const { return _value_ / 1000.0f; }
  uint32_t int_val() const { return _value_; }
  uint32_t _value_{0};
};

template <typename T, const char *_unit, const char *_int_unit>
struct FixedField : ParsedField<T> {
  ParseResult<void> parse(const char *str, const char *end) {
    ParseResult<uint32_t> res_float = NumParser::parse(3, _unit, str, end);
    if (!res_float.err_) { // CORRECTED: Access err_
      static_cast<T *>(this)->val()._value_ = res_float.result_; // CORRECTED: Access result_
      return res_float;
    }
    ParseResult<uint32_t> res_int = NumParser::parse(0, _int_unit, str, end);
    if (!res_int.err_) { // CORRECTED: Access err_
      static_cast<T *>(this)->val()._value_ = res_int.result_; // CORRECTED: Access result_
      return res_int;
    }
    return res_float;
  }
  static const char *unit() { return _unit; }
  static const char *int_unit() { return _int_unit; }
};

struct TimestampedFixedValue : public FixedValue {
  String timestamp_;
};

template <typename T, const char *_unit, const char *_int_unit>
struct TimestampedFixedField : public FixedField<T, _unit, _int_unit> {
  ParseResult<void> parse(const char *str, const char *end) {
    ParseResult<String> ts_res = StringParser::parse_string(13, 13, str, end);
    if (ts_res.err_) // CORRECTED: Access err_
      return ts_res;
    static_cast<T *>(this)->val().timestamp_ = ts_res.result_; // CORRECTED: Access result_
    return FixedField<T, _unit, _int_unit>::parse(ts_res.next_, end); // CORRECTED: Access next_
  }
};

template <typename T, const char *_unit, const char *_int_unit>
struct LastFixedField : public FixedField<T, _unit, _int_unit> {
  ParseResult<void> parse(const char *str, const char *end) {
    const char *last_value_start = str;
    ParseResult<String> temp_res;
    temp_res.next_ = str; // CORRECTED: Access next_
    while (temp_res.next_ < end && *temp_res.next_ == '(') { // CORRECTED: Access next_ (twice)
        last_value_start = temp_res.next_; // CORRECTED: Access next_
        temp_res = StringParser::parse_string(1, 20, temp_res.next_, end); // CORRECTED: Access next_
        if (temp_res.err_) { // CORRECTED: Access err_
            break;
        }
    }
    return FixedField<T, _unit, _int_unit>::parse(last_value_start, end);
  }
};

template <typename T, const char *_unit>
struct IntField : ParsedField<T> {
  ParseResult<void> parse(const char *str, const char *end) {
    ParseResult<uint32_t> res = NumParser::parse(0, _unit, str, end);
    if (!res.err_) // CORRECTED: Access err_
      static_cast<T *>(this)->val() = res.result_; // CORRECTED: Access result_
    return res;
  }
  static const char *unit() { return _unit; }
};

template <typename T>
struct RawField : ParsedField<T> {
  ParseResult<void> parse(const char *str, const char *end) {
    if (str > end) return ParseResult<void>().fail(F("Invalid raw field range"));
    concat_hack(static_cast<T *>(this)->val(), str, end - str);
    return ParseResult<void>().until(end);
  }
};

namespace fields {
struct units { /* ... (units content as in ver3_parser_lib_fields_h, copied for brevity) ... */
  static constexpr char none[] DSMR_PROGMEM = "";
  static constexpr char kWh[] DSMR_PROGMEM = "kWh";
  static constexpr char Wh[] DSMR_PROGMEM = "Wh";
  static constexpr char kW[] DSMR_PROGMEM = "kW";
  static constexpr char W[] DSMR_PROGMEM = "W";
  static constexpr char kV[] DSMR_PROGMEM = "kV";
  static constexpr char V[] DSMR_PROGMEM = "V";
  static constexpr char mV[] DSMR_PROGMEM = "mV";
  static constexpr char kA[] DSMR_PROGMEM = "kA";
  static constexpr char A[] DSMR_PROGMEM = "A";
  static constexpr char mA[] DSMR_PROGMEM = "mA";
  static constexpr char m3[] DSMR_PROGMEM = "m3";
  static constexpr char dm3[] DSMR_PROGMEM = "dm3";
  static constexpr char GJ[] DSMR_PROGMEM = "GJ";
  static constexpr char MJ[] DSMR_PROGMEM = "MJ";
  static constexpr char kvar[] DSMR_PROGMEM = "kvar";
  static constexpr char kvarh[] DSMR_PROGMEM = "kvarh";
  static constexpr char kVA[] DSMR_PROGMEM = "kVA";
  static constexpr char VA[] DSMR_PROGMEM = "VA";
  static constexpr char s[] DSMR_PROGMEM = "s";
  static constexpr char Hz[] DSMR_PROGMEM ="Hz";
  static constexpr char kHz[] DSMR_PROGMEM ="kHz";
};

const uint8_t GAS_MBUS_ID = DSMR_CUSTOM_GAS_MBUS_ID;
const uint8_t WATER_MBUS_ID = DSMR_CUSTOM_WATER_MBUS_ID;
const uint8_t THERMAL_MBUS_ID = DSMR_CUSTOM_THERMAL_MBUS_ID;
const uint8_t SUB_MBUS_ID = DSMR_CUSTOM_SUB_MBUS_ID;

#define DEFINE_FIELD(fieldname, value_t, obis, field_t, field_args...) \
  struct fieldname : field_t<fieldname, ##field_args> { \
    value_t fieldname##_; \
    bool fieldname##_present_{false}; \
    static constexpr ObisId id_{obis}; \
    static constexpr char name_progmem_[] DSMR_PROGMEM = #fieldname; \
    static const __FlashStringHelper* name() { \
        return reinterpret_cast<const __FlashStringHelper*>(name_progmem_); \
    } \
    value_t& val() { return fieldname##_; } \
    bool& present() { return fieldname##_present_; } \
    static constexpr ObisId id = obis; \
  }

// --- Field Definitions (as per ver3_parser_lib_fields_h, copied for brevity) ---
DEFINE_FIELD(identification, String, ObisId(255, 255, 255, 255, 255, 255), RawField);
// ... ALL OTHER DEFINE_FIELD calls from ver3_parser_lib_fields_h ...
DEFINE_FIELD(p1_version, String, ObisId(1, 3, 0, 2, 8), StringField, 2, 2);
DEFINE_FIELD(p1_version_be, String, ObisId(0, 0, 96, 1, 4), StringField, 2, 96);
DEFINE_FIELD(p1_version_ch, String, ObisId(0, 0, 96, 1, 4), StringField, 2, 96);
DEFINE_FIELD(timestamp, String, ObisId(0, 0, 1, 0, 0), TimestampField);
DEFINE_FIELD(equipment_id, String, ObisId(0, 0, 96, 1, 1), StringField, 0, 96);
DEFINE_FIELD(energy_delivered_lux, FixedValue, ObisId(1, 0, 1, 8, 0), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_delivered_tariff1, FixedValue, ObisId(1, 0, 1, 8, 1), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_delivered_tariff2, FixedValue, ObisId(1, 0, 1, 8, 2), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_delivered_tariff3, FixedValue, ObisId(1, 0, 1, 8, 3), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_delivered_tariff4, FixedValue, ObisId(1, 0, 1, 8, 4), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(reactive_energy_delivered_tariff1, FixedValue, ObisId(1,0,3,8,1), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_delivered_tariff2, FixedValue, ObisId(1,0,3,8,2), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_delivered_tariff3, FixedValue, ObisId(1,0,3,8,3), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_delivered_tariff4, FixedValue, ObisId(1,0,3,8,4), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(energy_delivered_tariff1_ch, FixedValue, ObisId(1,1,1,8,1), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_delivered_tariff2_ch, FixedValue, ObisId(1,1,1,8,2), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_lux, FixedValue, ObisId(1, 0, 2, 8, 0), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_tariff1, FixedValue, ObisId(1, 0, 2, 8, 1), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_tariff2, FixedValue, ObisId(1, 0, 2, 8, 2), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_tariff3, FixedValue, ObisId(1, 0, 2, 8, 3), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_tariff4, FixedValue, ObisId(1, 0, 2, 8, 4), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(reactive_energy_returned_tariff1, FixedValue, ObisId(1,0,4,8,1), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_returned_tariff2, FixedValue, ObisId(1,0,4,8,2), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_returned_tariff3, FixedValue, ObisId(1,0,4,8,3), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(reactive_energy_returned_tariff4, FixedValue, ObisId(1,0,4,8,4), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(energy_returned_tariff1_ch, FixedValue, ObisId(1,1,2,8,1), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(energy_returned_tariff2_ch, FixedValue, ObisId(1,1,2,8,2), FixedField, units::kWh, units::Wh);
DEFINE_FIELD(total_imported_energy, FixedValue, ObisId(1, 0, 3, 8, 0), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(total_exported_energy, FixedValue, ObisId(1, 0, 4, 8, 0), FixedField, units::kvarh, units::kvarh);
DEFINE_FIELD(electricity_tariff, String, ObisId(0, 0, 96, 14, 0), StringField, 4, 4);
DEFINE_FIELD(power_delivered, FixedValue, ObisId(1, 0, 1, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_returned, FixedValue, ObisId(1, 0, 2, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(reactive_power_delivered, FixedValue, ObisId(1, 0, 3, 7, 0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_returned, FixedValue, ObisId(1, 0, 4, 7, 0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(power_delivered_ch, FixedValue, ObisId(1,1,1,7,0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_returned_ch, FixedValue, ObisId(1,1,2,7,0), FixedField, units::kW, units::W);
DEFINE_FIELD(electricity_threshold, FixedValue, ObisId(0, 0, 17, 0, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(electricity_switch_position, uint8_t, ObisId(0, 0, 96, 3, 10), IntField, units::none);
DEFINE_FIELD(electricity_failures, uint32_t, ObisId(0, 0, 96, 7, 21), IntField, units::none);
DEFINE_FIELD(electricity_long_failures, uint32_t, ObisId(0, 0, 96, 7, 9), IntField, units::none);
DEFINE_FIELD(electricity_failure_log, String, ObisId(1, 0, 99, 97, 0), RawField);
DEFINE_FIELD(electricity_sags_l1, uint32_t, ObisId(1, 0, 32, 32, 0), IntField, units::none);
DEFINE_FIELD(voltage_sag_time_l1, uint32_t, ObisId(1,0,32,33,0), IntField, units::s);
DEFINE_FIELD(voltage_sag_l1, uint32_t, ObisId(1,0,32,34,0), IntField, units::V);
DEFINE_FIELD(electricity_sags_l2, uint32_t, ObisId(1, 0, 52, 32, 0), IntField, units::none);
DEFINE_FIELD(voltage_sag_time_l2, uint32_t, ObisId(1,0,52,33,0), IntField, units::s);
DEFINE_FIELD(voltage_sag_l2, uint32_t, ObisId(1,0,52,34,0), IntField, units::V);
DEFINE_FIELD(electricity_sags_l3, uint32_t, ObisId(1, 0, 72, 32, 0), IntField, units::none);
DEFINE_FIELD(voltage_sag_time_l3, uint32_t, ObisId(1,0,72,33,0), IntField, units::s);
DEFINE_FIELD(voltage_sag_l3, uint32_t, ObisId(1,0,72,34,0), IntField, units::V);
DEFINE_FIELD(electricity_swells_l1, uint32_t, ObisId(1, 0, 32, 36, 0), IntField, units::none);
DEFINE_FIELD(voltage_swell_time_l1, uint32_t, ObisId(1,0,32,37,0), IntField, units::s);
DEFINE_FIELD(voltage_swell_l1, uint32_t, ObisId(1,0,32,38,0), IntField, units::V);
DEFINE_FIELD(electricity_swells_l2, uint32_t, ObisId(1, 0, 52, 36, 0), IntField, units::none);
DEFINE_FIELD(voltage_swell_time_l2, uint32_t, ObisId(1,0,52,37,0), IntField, units::s);
DEFINE_FIELD(voltage_swell_l2, uint32_t, ObisId(1,0,52,38,0), IntField, units::V);
DEFINE_FIELD(electricity_swells_l3, uint32_t, ObisId(1, 0, 72, 36, 0), IntField, units::none);
DEFINE_FIELD(voltage_swell_time_l3, uint32_t, ObisId(1,0,72,37,0), IntField, units::s);
DEFINE_FIELD(voltage_swell_l3, uint32_t, ObisId(1,0,72,38,0), IntField, units::V);
DEFINE_FIELD(message_short, String, ObisId(0, 0, 96, 13, 1), StringField, 0, 16);
DEFINE_FIELD(message_long, String, ObisId(0, 0, 96, 13, 0), StringField, 0, 2048);
DEFINE_FIELD(voltage_l1, FixedValue, ObisId(1, 0, 32, 7, 0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage_avg_l1, FixedValue, ObisId(1,0,32,24,0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage_l2, FixedValue, ObisId(1, 0, 52, 7, 0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage_avg_l2, FixedValue, ObisId(1,0,52,24,0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage_l3, FixedValue, ObisId(1, 0, 72, 7, 0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage_avg_l3, FixedValue, ObisId(1,0,72,24,0), FixedField, units::V, units::mV);
DEFINE_FIELD(voltage, FixedValue, ObisId(1,0,12,7,0), FixedField, units::V, units::mV);
DEFINE_FIELD(frequency, FixedValue, ObisId(1,0,14,7,0), FixedField, units::kHz, units::Hz);
DEFINE_FIELD(abs_power, FixedValue, ObisId(1,0,15,7,0), FixedField, units::kW, units::W);
DEFINE_FIELD(current_l1, FixedValue, ObisId(1, 0, 31, 7, 0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_fuse_l1, FixedValue, ObisId(1,0,31,4,0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_l2, FixedValue, ObisId(1, 0, 51, 7, 0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_fuse_l2, FixedValue, ObisId(1,0,51,4,0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_l3, FixedValue, ObisId(1, 0, 71, 7, 0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_fuse_l3, FixedValue, ObisId(1,0,71,4,0), FixedField, units::A, units::mA);
DEFINE_FIELD(current, FixedValue, ObisId(1,0,11,7,0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_n, FixedValue, ObisId(1,0,91,7,0), FixedField, units::A, units::mA);
DEFINE_FIELD(current_sum, FixedValue, ObisId(1,0,90,7,0), FixedField, units::A, units::mA);
DEFINE_FIELD(power_delivered_l1, FixedValue, ObisId(1, 0, 21, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_delivered_l2, FixedValue, ObisId(1, 0, 41, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_delivered_l3, FixedValue, ObisId(1, 0, 61, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_returned_l1, FixedValue, ObisId(1, 0, 22, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_returned_l2, FixedValue, ObisId(1, 0, 42, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(power_returned_l3, FixedValue, ObisId(1, 0, 62, 7, 0), FixedField, units::kW, units::W);
DEFINE_FIELD(reactive_power_delivered_l1, FixedValue, ObisId(1,0,23,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_delivered_l2, FixedValue, ObisId(1,0,43,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_delivered_l3, FixedValue, ObisId(1,0,63,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_returned_l1, FixedValue, ObisId(1,0,24,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_returned_l2, FixedValue, ObisId(1,0,44,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_power_returned_l3, FixedValue, ObisId(1,0,64,7,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(apparent_delivery_power, FixedValue, ObisId(1,0,9,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_delivery_power_l1, FixedValue, ObisId(1,0,29,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_delivery_power_l2, FixedValue, ObisId(1,0,49,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_delivery_power_l3, FixedValue, ObisId(1,0,69,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_return_power, FixedValue, ObisId(1,0,10,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_return_power_l1, FixedValue, ObisId(1,0,30,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_return_power_l2, FixedValue, ObisId(1,0,50,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_return_power_l3, FixedValue, ObisId(1,0,70,7,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(active_demand_power, FixedValue, ObisId(1,0,1,24,0), FixedField, units::kW, units::W);
DEFINE_FIELD(active_demand_abs, FixedValue, ObisId(1,0,15,24,0), FixedField, units::kW, units::W);
DEFINE_FIELD(gas_device_type, uint16_t, ObisId(0, GAS_MBUS_ID, 24, 1, 0), IntField, units::none);
DEFINE_FIELD(gas_equipment_id, String, ObisId(0, GAS_MBUS_ID, 96, 1, 0), StringField, 0, 96);
DEFINE_FIELD(gas_equipment_id_be, String, ObisId(0, GAS_MBUS_ID, 96, 1, 1), StringField, 0, 96);
DEFINE_FIELD(gas_valve_position, uint8_t, ObisId(0, GAS_MBUS_ID, 24, 4, 0), IntField, units::none);
DEFINE_FIELD(gas_delivered, TimestampedFixedValue, ObisId(0, GAS_MBUS_ID, 24, 2, 1), TimestampedFixedField, units::m3, units::dm3);
DEFINE_FIELD(gas_delivered_be, TimestampedFixedValue, ObisId(0, GAS_MBUS_ID, 24, 2, 3), TimestampedFixedField, units::m3, units::dm3);
DEFINE_FIELD(gas_delivered_text, String, ObisId(0, GAS_MBUS_ID, 24,3,0), RawField);
DEFINE_FIELD(thermal_device_type, uint16_t, ObisId(0, THERMAL_MBUS_ID, 24, 1, 0), IntField, units::none);
DEFINE_FIELD(thermal_equipment_id, String, ObisId(0, THERMAL_MBUS_ID, 96, 1, 0), StringField, 0, 96);
DEFINE_FIELD(thermal_valve_position, uint8_t, ObisId(0, THERMAL_MBUS_ID, 24, 4, 0), IntField, units::none);
DEFINE_FIELD(thermal_delivered, TimestampedFixedValue, ObisId(0, THERMAL_MBUS_ID, 24, 2, 1), TimestampedFixedField, units::GJ, units::MJ);
DEFINE_FIELD(water_device_type, uint16_t, ObisId(0, WATER_MBUS_ID, 24, 1, 0), IntField, units::none);
DEFINE_FIELD(water_equipment_id, String, ObisId(0, WATER_MBUS_ID, 96, 1, 0), StringField, 0, 96);
DEFINE_FIELD(water_valve_position, uint8_t, ObisId(0, WATER_MBUS_ID, 24, 4, 0), IntField, units::none);
DEFINE_FIELD(water_delivered, TimestampedFixedValue, ObisId(0, WATER_MBUS_ID, 24, 2, 1), TimestampedFixedField, units::m3, units::dm3);
DEFINE_FIELD(sub_device_type, uint16_t, ObisId(0, SUB_MBUS_ID, 24, 1, 0), IntField, units::none);
DEFINE_FIELD(sub_equipment_id, String, ObisId(0, SUB_MBUS_ID, 96, 1, 0), StringField, 0, 96);
DEFINE_FIELD(sub_valve_position, uint8_t, ObisId(0, SUB_MBUS_ID, 24, 4, 0), IntField, units::none);
DEFINE_FIELD(sub_delivered, TimestampedFixedValue, ObisId(0, SUB_MBUS_ID, 24, 2, 1), TimestampedFixedField, units::m3, units::dm3);
DEFINE_FIELD(active_energy_import_current_average_demand, FixedValue, ObisId(1,0,1,4,0), FixedField, units::kW, units::W);
DEFINE_FIELD(active_energy_export_current_average_demand, FixedValue, ObisId(1,0,2,4,0), FixedField, units::kW, units::W);
DEFINE_FIELD(reactive_energy_import_current_average_demand, FixedValue, ObisId(1,0,3,4,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_energy_export_current_average_demand, FixedValue, ObisId(1,0,4,4,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(apparent_energy_import_current_average_demand, FixedValue, ObisId(1,0,9,4,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_energy_export_current_average_demand, FixedValue, ObisId(1,0,10,4,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(active_energy_import_last_completed_demand, FixedValue, ObisId(1,0,1,5,0), FixedField, units::kW, units::W);
DEFINE_FIELD(active_energy_export_last_completed_demand, FixedValue, ObisId(1,0,2,5,0), FixedField, units::kW, units::W);
DEFINE_FIELD(reactive_energy_import_last_completed_demand, FixedValue, ObisId(1,0,3,5,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(reactive_energy_export_last_completed_demand, FixedValue, ObisId(1,0,4,5,0), FixedField, units::kvar, units::kvar);
DEFINE_FIELD(apparent_energy_import_last_completed_demand, FixedValue, ObisId(1,0,9,5,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(apparent_energy_export_last_completed_demand, FixedValue, ObisId(1,0,10,5,0), FixedField, units::kVA, units::VA);
DEFINE_FIELD(active_energy_import_maximum_demand_running_month, TimestampedFixedValue, ObisId(1,0,1,6,0), TimestampedFixedField, units::kW, units::W);
DEFINE_FIELD(active_energy_import_maximum_demand_last_13_months, FixedValue, ObisId(0,0,98,1,0), LastFixedField, units::kW, units::W);
DEFINE_FIELD(fw_core_version, FixedValue, ObisId(1,0,0,2,0), FixedField, units::none, units::none);
DEFINE_FIELD(fw_core_checksum, String, ObisId(1,0,0,2,8), StringField, 0, 8);
DEFINE_FIELD(fw_module_version, FixedValue, ObisId(1,1,0,2,0), FixedField, units::none, units::none);
DEFINE_FIELD(fw_module_checksum, String, ObisId(1,1,0,2,8), StringField, 0, 8);
// END Field Definitions
} // namespace fields
} // namespace dsmr

// BEGIN MODIFICATION FOR ESPHOME DSMR_CUSTOM
#endif // DSMR_CUSTOM_PARSER_LIB_FIELDS_H
// END MODIFICATION FOR ESPHOME DSMR_CUSTOM

