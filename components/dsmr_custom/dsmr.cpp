/*
 * This file is part of the dsmr_custom ESPHome component.
 *
 * This file is inspired by or based on the original ESPHome DSMR component,
 * available at: https://github.com/esphome/esphome/tree/dev/esphome/components/dsmr
 *
 * Modifications and new code are Copyright (c) 2025 (Niko Paulanne).
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
 * @file dsmr.cpp
 * @brief Implementation of the Dsmr class for the dsmr_custom ESPHome component.
 * @details This file contains the core logic for the DSMR hub. Its architecture is
 * inspired by the native ESPHome DSMR component, but it has been specifically
 * implemented to support custom OBIS sensors and to interact with a modified
 * local parser for enhanced compatibility.
 * @author Niko Paulanne
 * @date June 7, 2025
 */

#ifdef USE_ARDUINO // Guard for Arduino-based platforms

#include "dsmr.h" // Header for this component's Dsmr class
#include "esphome/core/log.h"
#include "esphome/core/helpers.h" // For YESNO, etc.

// Cryptography libraries for AES-GCM decryption
#include <AES.h>
#include <GCM.h>

#include <string>
#include <algorithm> // For std::remove_if, std::min
#include <cctype>    // For ::isspace, ::isalnum
#include <cstring>   // For memchr, memcpy, strlen, strchr
#include <cstdlib>   // For std::strtoul, std::strtof

namespace esphome {
namespace dsmr_custom {

// Logging tag for the main component
static const char *const TAG = "dsmr_custom";
// Specific logging tag for custom sensor processing, for finer-grained log control.
static const char *const TAG_CUSTOM_SENSORS = "dsmr_custom.sensor";

/**
 * @brief Constructor for the Dsmr class.
 * @param uart Pointer to the UART component this hub will use for communication.
 * @param crc_check Boolean indicating if CRC check should be performed on telegrams.
 */
Dsmr::Dsmr(uart::UARTComponent *uart, bool crc_check) : uart::UARTDevice(uart), crc_check_(crc_check) {
  this->initialize_standard_sensor_obis_map_();
}

/**
 * @brief Initializes the standard_sensor_to_obis_map_.
 * @details This map links symbolic names of standard sensors (from Python platform files)
 * to their corresponding OBIS code strings as understood by the vendored parser.
 * CRUCIAL for the custom sensor override mechanism.
 */
void Dsmr::initialize_standard_sensor_obis_map_() {
  ESP_LOGV(TAG, "Initializing standard sensor to OBIS code map...");
  // This list MUST align with the symbolic names used in sensor.py/text_sensor.py
  // and the actual OBIS codes defined in the vendored fields.h
  standard_sensor_to_obis_map_["energy_delivered_lux"] = "1-0:1.8.0";
  standard_sensor_to_obis_map_["energy_delivered_tariff1"] = "1-0:1.8.1";
  standard_sensor_to_obis_map_["energy_delivered_tariff2"] = "1-0:1.8.2";
  // ... (Ensure this map is complete for ALL standard sensors defined in your fields.h and Python platforms) ...
  standard_sensor_to_obis_map_["energy_returned_lux"] = "1-0:2.8.0";
  standard_sensor_to_obis_map_["energy_returned_tariff1"] = "1-0:2.8.1";
  standard_sensor_to_obis_map_["energy_returned_tariff2"] = "1-0:2.8.2";
  standard_sensor_to_obis_map_["total_imported_energy"] = "1-0:3.8.0";
  standard_sensor_to_obis_map_["total_exported_energy"] = "1-0:4.8.0";
  standard_sensor_to_obis_map_["power_delivered"] = "1-0:1.7.0";
  standard_sensor_to_obis_map_["power_returned"] = "1-0:2.7.0";
  standard_sensor_to_obis_map_["reactive_power_delivered"] = "1-0:3.7.0";
  standard_sensor_to_obis_map_["reactive_power_returned"] = "1-0:4.7.0";
  standard_sensor_to_obis_map_["electricity_failures"] = "0-0:96.7.21";
  standard_sensor_to_obis_map_["electricity_long_failures"] = "0-0:96.7.9";
  standard_sensor_to_obis_map_["current_l1"] = "1-0:31.7.0";
  standard_sensor_to_obis_map_["current_l2"] = "1-0:51.7.0";
  standard_sensor_to_obis_map_["current_l3"] = "1-0:71.7.0";
  standard_sensor_to_obis_map_["power_delivered_l1"] = "1-0:21.7.0";
  standard_sensor_to_obis_map_["power_delivered_l2"] = "1-0:41.7.0";
  standard_sensor_to_obis_map_["power_delivered_l3"] = "1-0:61.7.0";
  standard_sensor_to_obis_map_["power_returned_l1"] = "1-0:22.7.0";
  standard_sensor_to_obis_map_["power_returned_l2"] = "1-0:42.7.0";
  standard_sensor_to_obis_map_["power_returned_l3"] = "1-0:62.7.0";
  standard_sensor_to_obis_map_["reactive_power_delivered_l1"] = "1-0:23.7.0";
  standard_sensor_to_obis_map_["reactive_power_delivered_l2"] = "1-0:43.7.0";
  standard_sensor_to_obis_map_["reactive_power_delivered_l3"] = "1-0:63.7.0";
  standard_sensor_to_obis_map_["reactive_power_returned_l1"] = "1-0:24.7.0";
  standard_sensor_to_obis_map_["reactive_power_returned_l2"] = "1-0:44.7.0";
  standard_sensor_to_obis_map_["reactive_power_returned_l3"] = "1-0:64.7.0";
  standard_sensor_to_obis_map_["voltage_l1"] = "1-0:32.7.0";
  standard_sensor_to_obis_map_["voltage_l2"] = "1-0:52.7.0";
  standard_sensor_to_obis_map_["voltage_l3"] = "1-0:72.7.0";
  standard_sensor_to_obis_map_["gas_delivered"] = "0-1:24.2.1";    // Example for M-Bus ID 1
  standard_sensor_to_obis_map_["gas_delivered_be"] = "0-1:24.2.3"; // Example for M-Bus ID 1
  standard_sensor_to_obis_map_["active_energy_import_maximum_demand_running_month"] = "1-0:1.6.0";
  standard_sensor_to_obis_map_["identification"] = "identification"; // Special key
  standard_sensor_to_obis_map_["p1_version"] = "1-3:0.2.8";
  standard_sensor_to_obis_map_["p1_version_be"] = "0-0:96.1.4";
  standard_sensor_to_obis_map_["timestamp"] = "0-0:1.0.0";
  standard_sensor_to_obis_map_["electricity_tariff"] = "0-0:96.14.0";
  standard_sensor_to_obis_map_["message_long"] = "0-0:96.13.0";
  standard_sensor_to_obis_map_["message_short"] = "0-0:96.13.1";
  standard_sensor_to_obis_map_["gas_equipment_id"] = "0-1:96.1.0"; // Example for M-Bus ID 1

  ESP_LOGD(TAG, "Standard sensor OBIS map initialized with %zu entries.", standard_sensor_to_obis_map_.size());
}

void Dsmr::setup() {
  ESP_LOGCONFIG(TAG, "Setting up dsmr_custom component...");
  this->telegram_ = new char[this->max_telegram_len_ + 1];
  if (this->telegram_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate telegram_ buffer!");
      this->mark_failed();
      return;
  }
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
    this->request_pin_->digital_write(false);
    ESP_LOGCONFIG(TAG, "DSMR Request Pin initialized: %s", this->request_pin_->get_pin_name_friendly().c_str());
  }
}

void Dsmr::loop() {
  if (this->ready_to_request_data_()) {
    if (this->decryption_key_.empty()) {
      this->receive_telegram_();
    } else {
      if (this->crypt_telegram_ == nullptr && !this->decryption_key_.empty()) {
          ESP_LOGW(TAG, "Decryption key is set, but crypt_telegram_ buffer is null. Allocating now.");
          this->crypt_telegram_ = new uint8_t[this->max_telegram_len_ + 1];
          if (this->crypt_telegram_ == nullptr) {
             ESP_LOGE(TAG, "Failed to allocate crypt_telegram_ buffer in loop! Disabling requests.");
             this->stop_requesting_data_();
             return;
          }
      }
      if (this->crypt_telegram_ != nullptr) {
        this->receive_encrypted_telegram_();
      } else {
          ESP_LOGE(TAG, "Decryption key set, but crypt_telegram_ buffer is still null. Skipping encrypted receive. Disabling requests.");
          this->stop_requesting_data_();
      }
    }
  }
}

// ... (ready_to_request_data_, request_interval_reached_, receive_timeout_reached_, available_within_timeout_,
//      start_requesting_data_, stop_requesting_data_, reset_telegram_,
//      receive_telegram_, receive_encrypted_telegram_ methods copied from ver3_dsmr_custom_dsmr_cpp as they are largely okay,
//      with minor logging adjustments or checks if needed)

bool Dsmr::ready_to_request_data_() {
  if (this->request_pin_ != nullptr) {
    if (!this->requesting_data_ && this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
  } else {
    if (this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
    if (!this->requesting_data_) {
      uint32_t discarded_bytes = 0;
      while (this->available()) {
        this->read();
        discarded_bytes++;
      }
      if (discarded_bytes > 0) {
        ESP_LOGVV(TAG, "Discarded %u bytes from UART buffer while not actively reading.", discarded_bytes);
      }
    }
  }
  return this->requesting_data_;
}

bool Dsmr::request_interval_reached_() {
  if (this->request_interval_ == 0 && this->request_pin_ == nullptr) {
      return true;
  }
  if (this->last_request_time_ == 0) {
    return true;
  }
  return (millis() - this->last_request_time_) >= this->request_interval_;
}

bool Dsmr::receive_timeout_reached_() {
    if (this->receive_timeout_ == 0) return false;
    return (millis() - this->last_read_time_) > this->receive_timeout_;
}

bool Dsmr::available_within_timeout_() {
  if (this->available()) {
    this->last_read_time_ = millis();
    return true;
  }
  if (!this->header_found_) {
    if (this->receive_timeout_ > 0 && (millis() - this->last_request_time_ > this->receive_timeout_)) {
        if (this->requesting_data_) {
             ESP_LOGV(TAG, "Timeout waiting for telegram header (since last request: %ums > %ums).",
                      (unsigned long)(millis() - this->last_request_time_), (unsigned long)this->receive_timeout_);
        }
        this->reset_telegram_();
        this->stop_requesting_data_();
        return false;
    }
    return false;
  }
  if (this->parent_->get_rx_buffer_size() < this->max_telegram_len_) {
    uint32_t wait_entry_time = millis();
    while (!this->receive_timeout_reached_()) {
      yield();
      if (this->available()) {
        this->last_read_time_ = millis();
        return true;
      }
      if (this->receive_timeout_ > 0 && (millis() - wait_entry_time > this->receive_timeout_ + 100)) break;
      if (this->receive_timeout_ == 0 && (millis() - wait_entry_time > 2000)) break;
    }
  } else {
    if (!this->receive_timeout_reached_()) {
        delay(1);
        if (this->available()) {
            this->last_read_time_ = millis();
            return true;
        }
    }
  }
  if (this->receive_timeout_reached_()) {
    ESP_LOGW(TAG, "Timeout while reading data for telegram (header_found_: %s, bytes_read_: %zu, last_read_ago: %ums > %ums)",
             YESNO(this->header_found_), this->bytes_read_,
             (unsigned long)(millis() - this->last_read_time_), (unsigned long)this->receive_timeout_);
    this->reset_telegram_();
    this->stop_requesting_data_();
  }
  return false;
}

void Dsmr::start_requesting_data_() {
  if (!this->requesting_data_) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Starting data request from P1 port (request pin HIGH).");
      this->request_pin_->digital_write(true);
    } else {
      ESP_LOGV(TAG, "Starting P1 port read attempt (no request pin).");
    }
    this->requesting_data_ = true;
    this->last_request_time_ = millis();
    this->last_read_time_ = millis();
    this->reset_telegram_();
  }
}

void Dsmr::stop_requesting_data_() {
  if (this->requesting_data_ || (this->request_pin_ != nullptr && this->request_pin_->digital_read())) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Stopping data request from P1 port (request pin LOW).");
      this->request_pin_->digital_write(false);
    } else {
      ESP_LOGV(TAG, "Stopping P1 port read attempt (no request pin).");
    }
    this->requesting_data_ = false;
  }
}

void Dsmr::reset_telegram_() {
  this->header_found_ = false;
  this->footer_found_ = false;
  this->bytes_read_ = 0;
  if (this->telegram_ != nullptr) {
    this->telegram_[0] = '\0';
  }
  this->crypt_bytes_read_ = 0;
  this->crypt_telegram_len_ = 0;
}

void Dsmr::receive_telegram_() {
  while (this->available_within_timeout_()) {
    const char c = static_cast<char>(this->read());
    if (!this->header_found_) {
      if (c == '/') {
        ESP_LOGV(TAG, "Header of plain telegram found ('/').");
        this->reset_telegram_();
        this->header_found_ = true;
        this->telegram_[this->bytes_read_++] = c;
        this->last_read_time_ = millis();
      }
      continue;
    }
    if (this->bytes_read_ >= this->max_telegram_len_) {
      ESP_LOGE(TAG, "Error: Plain telegram larger than buffer (%zu bytes). Discarding.", this->max_telegram_len_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    if (c == '(' && this->bytes_read_ > 0) {
      size_t temp_bytes_read = this->bytes_read_;
      while (temp_bytes_read > 0) {
        char prev_char = this->telegram_[temp_bytes_read - 1];
        if (prev_char == '\r' || prev_char == '\n') {
          temp_bytes_read--;
        } else {
          break;
        }
      }
      if (temp_bytes_read < this->bytes_read_) {
         ESP_LOGVV(TAG, "Removed %zu CR/LF chars before '('.", this->bytes_read_ - temp_bytes_read);
      }
      this->bytes_read_ = temp_bytes_read;
    }
    this->telegram_[this->bytes_read_++] = c;
    if (c == '!') {
      this->footer_found_ = true;
      ESP_LOGV(TAG, "Footer of plain telegram found ('!'). Expecting CRC and newline.");
    } else if (this->footer_found_ && (c == '\n' || c == '\r')) {
      this->telegram_[this->bytes_read_] = '\0';
      ESP_LOGV(TAG, "End of plain telegram detected (newline after CRC). Length: %zu", this->bytes_read_);
      this->parse_telegram();
      this->reset_telegram_();
      return;
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  if (this->crypt_telegram_ == nullptr) {
      ESP_LOGE(TAG, "Encrypted receive called, but crypt_telegram_ buffer is not allocated. Decryption key issue?");
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
  }
  while (this->available_within_timeout_()) {
    const uint8_t c_byte = static_cast<uint8_t>(this->read());
    if (!this->header_found_) {
      if (c_byte != 0xDB) {
        continue;
      }
      ESP_LOGV(TAG, "Start byte 0xDB of encrypted telegram found.");
      this->reset_telegram_();
      this->header_found_ = true;
      this->last_read_time_ = millis();
    }
    if (this->crypt_bytes_read_ == 0 && c_byte != 0xDB && this->header_found_) {
        ESP_LOGW(TAG, "Encrypted telegram reception: Expected 0xDB as first byte after header_found, got 0x%02X. Resetting.", c_byte);
        this->reset_telegram_();
        this->stop_requesting_data_();
        return;
    }
    if (this->crypt_bytes_read_ >= this->max_telegram_len_) {
      ESP_LOGE(TAG, "Error: Encrypted telegram frame larger than buffer (%zu bytes). Discarding.", this->max_telegram_len_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    this->crypt_telegram_[this->crypt_bytes_read_++] = c_byte;
    if (this->crypt_telegram_len_ == 0 && this->crypt_bytes_read_ >= 13) {
        if (this->crypt_telegram_[0] != 0xDB || this->crypt_telegram_[1] != 0x08) {
            ESP_LOGE(TAG, "Invalid encrypted frame header: %02X%02X. Expected DB08. Discarding.", this->crypt_telegram_[0], this->crypt_telegram_[1]);
            this->reset_telegram_(); this->stop_requesting_data_(); return;
        }
        size_t len_info = (static_cast<size_t>(this->crypt_telegram_[11]) << 8) | static_cast<size_t>(this->crypt_telegram_[12]);
        this->crypt_telegram_len_ = 18 + len_info + 12;
        ESP_LOGV(TAG, "Encrypted telegram expected total frame length: %zu bytes (LEN_INFO: %zu)", this->crypt_telegram_len_, len_info);
        if (this->crypt_telegram_len_ > this->max_telegram_len_) {
            ESP_LOGE(TAG, "Calculated encrypted frame length (%zu) exceeds buffer (%zu). Discarding.", this->crypt_telegram_len_, this->max_telegram_len_);
            this->reset_telegram_(); this->stop_requesting_data_(); return;
        }
    }
    if (this->crypt_telegram_len_ == 0 || this->crypt_bytes_read_ < this->crypt_telegram_len_) {
      continue;
    }
    ESP_LOGV(TAG, "End of encrypted telegram frame found (read %zu bytes, expected %zu).", this->crypt_bytes_read_, this->crypt_telegram_len_);
    size_t ciphertext_offset = 18;
    size_t gcm_tag_length = 12;
    size_t len_info_from_frame = (static_cast<size_t>(this->crypt_telegram_[11]) << 8) | static_cast<size_t>(this->crypt_telegram_[12]);
    if (this->crypt_bytes_read_ < (ciphertext_offset + gcm_tag_length) || len_info_from_frame == 0) {
        ESP_LOGE(TAG, "Encrypted data too short for IV, ciphertext, and GCM tag, or LEN_INFO is zero. Read: %zu, LEN_INFO: %zu", this->crypt_bytes_read_, len_info_from_frame);
        this->reset_telegram_(); this->stop_requesting_data_(); return;
    }
    size_t ciphertext_len = len_info_from_frame;
    if (ciphertext_offset + ciphertext_len + gcm_tag_length != this->crypt_telegram_len_) {
        ESP_LOGE(TAG, "Encrypted frame length mismatch. Expected based on LEN_INFO: %zu, Actual frame read: %zu",
                 ciphertext_offset + ciphertext_len + gcm_tag_length, this->crypt_bytes_read_);
        this->reset_telegram_(); this->stop_requesting_data_(); return;
    }
    GCM<AES128> gcmaes128;
    gcmaes128.setKey(this->decryption_key_.data(), gcmaes128.keySize());
    uint8_t iv[12];
    memcpy(iv, &this->crypt_telegram_[2], 8);
    memcpy(iv + 8, &this->crypt_telegram_[14], 4);
    gcmaes128.setIV(iv, sizeof(iv));
    ESP_LOGV(TAG, "Decryption IV (Hex): %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X",
             iv[0], iv[1], iv[2], iv[3], iv[4], iv[5], iv[6], iv[7], iv[8], iv[9], iv[10], iv[11]);
    uint8_t* ciphertext_ptr = &this->crypt_telegram_[ciphertext_offset];
    uint8_t* tag_ptr = &this->crypt_telegram_[ciphertext_offset + ciphertext_len];
    if (ciphertext_len > this->max_telegram_len_) {
        ESP_LOGE(TAG, "Decrypted data length (%zu) would exceed plain telegram_ buffer (%zu).", ciphertext_len, this->max_telegram_len_);
        this->reset_telegram_(); this->stop_requesting_data_(); return;
    }
    gcmaes128.decrypt(reinterpret_cast<uint8_t *>(this->telegram_), ciphertext_ptr, ciphertext_len);
    if (!gcmaes128.checkTag(tag_ptr, gcm_tag_length)) {
        ESP_LOGW(TAG, "Decryption failed! GCM tag mismatch.");
        this->reset_telegram_(); this->stop_requesting_data_(); return;
    }
    this->telegram_[ciphertext_len] = '\0';
    this->bytes_read_ = ciphertext_len;
    ESP_LOGD(TAG, "Decryption successful. Decrypted P1 telegram size: %zu bytes.", this->bytes_read_);
    ESP_LOGVV(TAG, "Decrypted P1 telegram content:\n%s", this->telegram_);
    this->parse_telegram();
    this->reset_telegram_();
    return;
  }
}

optional<float> Dsmr::parse_numeric_value_from_string(const std::string &value_str) {
    std::string temp_val = value_str;
    size_t star_pos = temp_val.find('*');
    if (star_pos != std::string::npos) {
        temp_val = temp_val.substr(0, star_pos);
    }
    if (!temp_val.empty() && temp_val.front() == '(' && temp_val.back() == ')') {
        if (temp_val.length() >= 2) {
          temp_val = temp_val.substr(1, temp_val.length() - 2);
        } else {
          return {};
        }
    }
    temp_val.erase(std::remove_if(temp_val.begin(), temp_val.end(), ::isspace), temp_val.end());
    if (temp_val.empty()) {
        return {};
    }
    char* end_ptr = nullptr;
    float val = std::strtof(temp_val.c_str(), &end_ptr);
    if (end_ptr == temp_val.c_str() || (end_ptr != nullptr && *end_ptr != '\0')) {
        ESP_LOGVV(TAG_CUSTOM_SENSORS, "strtof failed for '%s'. end_ptr points to '%c' (0x%02X)", temp_val.c_str(), (end_ptr ? *end_ptr : '0'), (end_ptr ? *end_ptr : 0) );
        return {};
    }
    return val;
}

std::string Dsmr::parse_text_value_from_string(const std::string &value_str) {
    std::string parsed_text = value_str;
    if (!parsed_text.empty() && parsed_text.front() == '(' && parsed_text.back() == ')') {
        if (parsed_text.length() >= 2) {
            parsed_text = parsed_text.substr(1, parsed_text.length() - 2);
        } else {
            return "";
        }
    }
    return parsed_text;
}

void Dsmr::process_line_for_custom_sensors(const char *line_buffer, size_t length) {
    if (length == 0) return;
    std::string line_str(line_buffer, length);
    size_t open_paren_pos = line_str.find('(');
    size_t close_paren_pos = line_str.rfind(')');

    if (open_paren_pos == std::string::npos || close_paren_pos == std::string::npos ||
        open_paren_pos >= close_paren_pos || (close_paren_pos == open_paren_pos + 1) ) {
        ESP_LOGVV(TAG_CUSTOM_SENSORS, "Line '%s' not a valid OBIS value format for custom parsing.", line_str.c_str());
        return;
    }

    std::string obis_code_str = line_str.substr(0, open_paren_pos);
    std::string value_part_str = line_str.substr(open_paren_pos + 1, close_paren_pos - (open_paren_pos + 1));
    obis_code_str.erase(std::remove_if(obis_code_str.begin(), obis_code_str.end(), ::isspace), obis_code_str.end());

    if (obis_code_str.empty()) {
        ESP_LOGVV(TAG_CUSTOM_SENSORS, "Empty OBIS code extracted from line '%s'.", line_str.c_str());
        return;
    }

    ESP_LOGVV(TAG_CUSTOM_SENSORS, "Processing line for custom sensors: OBIS '%s', ValuePart '%s'",
              obis_code_str.c_str(), value_part_str.c_str());

    for (auto &custom_def : this->custom_obis_definitions_) {
        if (custom_def.obis_code_str == obis_code_str) {
            if (custom_def.type == CustomObisSensorType::NUMERIC && custom_def.numeric_sensor_ptr != nullptr) {
                optional<float> val_opt = this->parse_numeric_value_from_string(value_part_str);
                if (val_opt.has_value()) {
                    float current_value = val_opt.value();
                    // Accessing static constexpr members via class name or this-> (if not shadowed)
                    bool value_changed_significantly = std::isnan(custom_def.last_published_float_value) ||
                                                       (std::fabs(current_value - custom_def.last_published_float_value) > Dsmr::CUSTOM_SENSOR_FLOAT_TOLERANCE);
                    bool interval_passed = (millis() - custom_def.last_publish_time) >= Dsmr::CUSTOM_SENSOR_MIN_PUBLISH_INTERVAL_MS;

                    if (value_changed_significantly || interval_passed) {
                        custom_def.numeric_sensor_ptr->publish_state(current_value);
                        custom_def.last_published_float_value = current_value;
                        custom_def.last_publish_time = millis();
                        ESP_LOGD(TAG_CUSTOM_SENSORS, "Published to custom numeric sensor '%s' (OBIS: %s): %.3f",
                                 custom_def.numeric_sensor_ptr->get_name().c_str(), obis_code_str.c_str(), current_value);
                    }
                } else {
                    ESP_LOGW(TAG_CUSTOM_SENSORS, "Failed to parse float for custom OBIS '%s' from value part '%s' on line '%s'",
                             obis_code_str.c_str(), value_part_str.c_str(), line_str.c_str());
                }
            } else if (custom_def.type == CustomObisSensorType::TEXT && custom_def.text_sensor_ptr != nullptr) {
                std::string parsed_text = this->parse_text_value_from_string(value_part_str);
                bool value_changed = custom_def.last_published_text_value != parsed_text;
                bool interval_passed = (millis() - custom_def.last_publish_time) >= Dsmr::CUSTOM_SENSOR_MIN_PUBLISH_INTERVAL_MS;

                if (value_changed || interval_passed) {
                    custom_def.text_sensor_ptr->publish_state(parsed_text);
                    custom_def.last_published_text_value = parsed_text;
                    custom_def.last_publish_time = millis();
                    ESP_LOGD(TAG_CUSTOM_SENSORS, "Published to custom text sensor '%s' (OBIS: %s): %s",
                             custom_def.text_sensor_ptr->get_name().c_str(), obis_code_str.c_str(), parsed_text.c_str());
                }
            }
            break;
        }
    }
}

bool Dsmr::parse_telegram() {
  MyData data_from_standard_parser;
  ESP_LOGV(TAG, "Attempting to parse P1 telegram of %zu bytes using vendored parser.", this->bytes_read_);

  if (this->bytes_read_ < this->max_telegram_len_) {
      this->telegram_[this->bytes_read_] = '\0';
  } else {
      this->telegram_[this->max_telegram_len_] = '\0';
      ESP_LOGW(TAG, "Telegram length at maximum buffer capacity (%zu). Ensure buffer is sufficient.", this->max_telegram_len_);
  }

  ::dsmr::ParseResult<void> standard_parse_result =
      ::dsmr::P1Parser::parse(&data_from_standard_parser, this->telegram_, this->bytes_read_,
                              false /* unknown_error */, this->crc_check_);

  if (standard_parse_result.err_) { // CORRECTED: Access err_ (from ver4_parser_lib_parser.h via ver3_parser_lib_util.h)
    auto err_str = standard_parse_result.fullError(this->telegram_, this->telegram_ + this->bytes_read_);
    ESP_LOGW(TAG, "DSMR P1 vendored parser error: %s", err_str.c_str());
    this->status_set_warning();
  } else {
    ESP_LOGD(TAG, "Successfully parsed P1 telegram using vendored parser for standard fields.");
    this->status_clear_warning();
    this->publish_sensors(data_from_standard_parser);
  }

  ESP_LOGV(TAG, "Processing telegram for custom OBIS sensors line by line.");
  const char *current_line_start = this->telegram_;
  const char *telegram_buffer_end = this->telegram_ + this->bytes_read_;

  while (current_line_start < telegram_buffer_end && *current_line_start != '\0') {
      const char *line_feed_pos = static_cast<const char*>(memchr(current_line_start, '\n', telegram_buffer_end - current_line_start));
      const char *carriage_return_pos = static_cast<const char*>(memchr(current_line_start, '\r', telegram_buffer_end - current_line_start));
      const char *actual_line_end_char = nullptr;

      if (line_feed_pos && carriage_return_pos) {
          actual_line_end_char = std::min(line_feed_pos, carriage_return_pos);
      } else if (line_feed_pos) {
          actual_line_end_char = line_feed_pos;
      } else if (carriage_return_pos) {
          actual_line_end_char = carriage_return_pos;
      }

      size_t current_line_length;
      if (actual_line_end_char != nullptr) {
          current_line_length = actual_line_end_char - current_line_start;
      } else {
          const char *exclamation_pos = static_cast<const char*>(memchr(current_line_start, '!', telegram_buffer_end - current_line_start));
          if (exclamation_pos && exclamation_pos < telegram_buffer_end) {
               current_line_length = exclamation_pos - current_line_start;
          } else {
               current_line_length = telegram_buffer_end - current_line_start;
          }
      }
      if (current_line_length > 0) {
          this->process_line_for_custom_sensors(current_line_start, current_line_length);
      }
      if (actual_line_end_char != nullptr) {
          current_line_start = actual_line_end_char + 1;
          while (current_line_start < telegram_buffer_end && (*current_line_start == '\r' || *current_line_start == '\n')) {
              current_line_start++;
          }
      } else {
          break;
      }
  }
  if (this->s_telegram_ != nullptr) {
    this->s_telegram_->publish_state(std::string(this->telegram_, this->bytes_read_));
    ESP_LOGV(TAG, "Published full telegram to s_telegram_ text_sensor.");
  }
  this->stop_requesting_data_();
  return !standard_parse_result.err_; // CORRECTED: Access err_
}

void Dsmr::publish_sensors(MyData &data) {
  ESP_LOGV(TAG, "Publishing states for standard DSMR sensors...");

  #define DSMR_PUBLISH_STANDARD_SENSOR(sensor_field_name) \
    if (data.sensor_field_name##_present_ && this->s_##sensor_field_name##_ != nullptr) { \
      this->s_##sensor_field_name##_->publish_state(data.sensor_field_name##_); \
      ESP_LOGD(TAG, "Published standard sensor '%s': %f", #sensor_field_name, static_cast<float>(data.sensor_field_name##_)); \
    } else if (data.sensor_field_name##_present_ && this->s_##sensor_field_name##_ == nullptr) { \
      ESP_LOGV(TAG, "Standard sensor '%s' was parsed but is overridden by a custom sensor. Not publishing.", #sensor_field_name); \
    }
  DSMR_CUSTOM_SENSOR_LIST(DSMR_PUBLISH_STANDARD_SENSOR, )

  #define DSMR_PUBLISH_STANDARD_TEXT_SENSOR(sensor_field_name) \
    if (data.sensor_field_name##_present_ && this->s_##sensor_field_name##_ != nullptr) { \
      this->s_##sensor_field_name##_->publish_state(data.sensor_field_name##_.c_str()); \
      ESP_LOGD(TAG, "Published standard text_sensor '%s': %s", #sensor_field_name, data.sensor_field_name##_.c_str()); \
    } else if (data.sensor_field_name##_present_ && this->s_##sensor_field_name##_ == nullptr) { \
      ESP_LOGV(TAG, "Standard text_sensor '%s' was parsed but is overridden by a custom sensor. Not publishing.", #sensor_field_name); \
    }
  DSMR_CUSTOM_TEXT_SENSOR_LIST(DSMR_PUBLISH_STANDARD_TEXT_SENSOR, )
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG, "DSMR Custom Component Configuration:");
  ESP_LOGCONFIG(TAG, "  UART Bus: Configured (details in UART component logs)");
  ESP_LOGCONFIG(TAG, "  Max Telegram Length: %zu bytes", this->max_telegram_len_);
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_);
  ESP_LOGCONFIG(TAG, "  CRC Check Enabled: %s", YESNO(this->crc_check_));
  if (this->request_pin_ != nullptr) {
    LOG_PIN("  Request Pin: ", this->request_pin_);
    ESP_LOGCONFIG(TAG, "  Request Interval: %u ms", this->request_interval_);
  } else {
    ESP_LOGCONFIG(TAG, "  Request Pin: Not configured");
    if (this->request_interval_ > 0) {
        ESP_LOGCONFIG(TAG, "  Passive Read Interval: %u ms", this->request_interval_);
    } else {
        ESP_LOGCONFIG(TAG, "  Passive Read Interval: Continuous attempt");
    }
  }
  if (!this->decryption_key_.empty()) {
      ESP_LOGCONFIG(TAG, "  Decryption: Enabled (key is set)");
  } else {
      ESP_LOGCONFIG(TAG, "  Decryption: Disabled (no key set)");
  }
  ESP_LOGCONFIG(TAG, "  Standard Sensors (pointers; null if overridden by custom or not configured):");
  #define DSMR_LOG_STANDARD_SENSOR_IMPL(s) \
    if (this->s_##s##_ != nullptr) { LOG_SENSOR("    ", #s, this->s_##s##_); } \
    else { ESP_LOGCONFIG(TAG, "    %s (numeric): Overridden by custom sensor or not configured.", #s); }
  DSMR_CUSTOM_SENSOR_LIST(DSMR_LOG_STANDARD_SENSOR_IMPL, )
  #define DSMR_LOG_STANDARD_TEXT_SENSOR_IMPL(s) \
    if (this->s_##s##_ != nullptr) { LOG_TEXT_SENSOR("    ", #s, this->s_##s##_); } \
    else { ESP_LOGCONFIG(TAG, "    %s (text): Overridden by custom sensor or not configured.", #s); }
  DSMR_CUSTOM_TEXT_SENSOR_LIST(DSMR_LOG_STANDARD_TEXT_SENSOR_IMPL, )
  LOG_TEXT_SENSOR("  ", "Full Telegram Text Sensor (s_telegram_)", this->s_telegram_);

  if (!this->custom_obis_definitions_.empty()) {
    ESP_LOGCONFIG(TAG_CUSTOM_SENSORS, "  Custom OBIS Sensors Registered (%zu total):", this->custom_obis_definitions_.size());
    for (const auto &def : this->custom_obis_definitions_) {
        if (def.type == CustomObisSensorType::NUMERIC && def.numeric_sensor_ptr != nullptr) {
            ESP_LOGCONFIG(TAG_CUSTOM_SENSORS, "    - OBIS: '%s', Name: '%s' (Numeric Sensor)",
                          def.obis_code_str.c_str(), def.numeric_sensor_ptr->get_name().c_str());
        } else if (def.type == CustomObisSensorType::TEXT && def.text_sensor_ptr != nullptr) {
            ESP_LOGCONFIG(TAG_CUSTOM_SENSORS, "    - OBIS: '%s', Name: '%s' (Text Sensor)",
                          def.obis_code_str.c_str(), def.text_sensor_ptr->get_name().c_str());
        }
    }
  } else {
    ESP_LOGCONFIG(TAG_CUSTOM_SENSORS, "  No Custom OBIS Sensors registered.");
  }
}

void Dsmr::set_decryption_key(const std::string &decryption_key_hex) {
  if (decryption_key_hex.empty()) {
    ESP_LOGI(TAG, "Disabling DSMR telegram decryption (key cleared).");
    this->decryption_key_.clear();
    if (this->crypt_telegram_ != nullptr) {
      delete[] this->crypt_telegram_;
      this->crypt_telegram_ = nullptr;
    }
    return;
  }
  if (decryption_key_hex.length() != 32) {
    ESP_LOGE(TAG, "Error: Decryption key must be 32 hexadecimal characters long (is %zu). Decryption disabled.", decryption_key_hex.length());
    this->decryption_key_.clear();
    if (this->crypt_telegram_ != nullptr) {
        delete[] this->crypt_telegram_;
        this->crypt_telegram_ = nullptr;
    }
    return;
  }
  this->decryption_key_.assign(16, 0);
  ESP_LOGI(TAG, "DSMR telegram decryption key is set.");
  ESP_LOGV(TAG, "Using decryption key (hex): %s", decryption_key_hex.c_str());
  for (int i = 0; i < 16; i++) {
    char temp_hex_pair[3] = {decryption_key_hex[i * 2], decryption_key_hex[i * 2 + 1], '\0'};
    this->decryption_key_[i] = static_cast<uint8_t>(std::strtoul(temp_hex_pair, nullptr, 16));
  }
  if (this->crypt_telegram_ == nullptr) {
    this->crypt_telegram_ = new uint8_t[this->max_telegram_len_ + 1];
    if (this->crypt_telegram_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate crypt_telegram_ buffer after setting key!");
    } else {
        ESP_LOGD(TAG, "Allocated crypt_telegram_ buffer (%zu bytes) for encrypted data.", this->max_telegram_len_ + 1);
    }
  }
}

void Dsmr::add_custom_numeric_sensor(const std::string &obis_code, esphome::sensor::Sensor *sens) {
  if (sens == nullptr) {
    ESP_LOGW(TAG_CUSTOM_SENSORS, "Attempted to register a null custom numeric sensor for OBIS '%s'. Skipping.", obis_code.c_str());
    return;
  }
  ESP_LOGD(TAG_CUSTOM_SENSORS, "Registering custom numeric sensor: OBIS '%s', Name '%s'",
           obis_code.c_str(), sens->get_name().c_str());

  for (const auto& map_entry : this->standard_sensor_to_obis_map_) {
      const std::string& standard_sensor_symbolic_name = map_entry.first;
      const std::string& standard_sensor_obis_code = map_entry.second;
      if (obis_code == standard_sensor_obis_code) {
          auto it_ptr_map = this->standard_numeric_sensor_pointers_.find(standard_sensor_symbolic_name);
          if (it_ptr_map != this->standard_numeric_sensor_pointers_.end()) {
              if (*(it_ptr_map->second) != nullptr) {
                  ESP_LOGI(TAG_CUSTOM_SENSORS, "Custom numeric sensor for OBIS '%s' (Name: '%s') overrides standard sensor '%s'. Standard sensor will be disabled.",
                           obis_code.c_str(), sens->get_name().c_str(), standard_sensor_symbolic_name.c_str());
                  *(it_ptr_map->second) = nullptr;
              } else {
                  ESP_LOGV(TAG_CUSTOM_SENSORS, "Standard numeric sensor '%s' (OBIS: %s) was already null (possibly overridden or not configured).",
                            standard_sensor_symbolic_name.c_str(), obis_code.c_str());
              }
          } else {
              ESP_LOGW(TAG_CUSTOM_SENSORS, "OBIS code '%s' matches standard sensor '%s', but its pointer-to-pointer was not found in standard_numeric_sensor_pointers_ map. Override might not work as expected.",
                        obis_code.c_str(), standard_sensor_symbolic_name.c_str());
          }
          break;
      }
  }
  CustomObisSensorDefinition def;
  def.obis_code_str = obis_code;
  def.numeric_sensor_ptr = sens;
  def.text_sensor_ptr = nullptr;
  def.type = CustomObisSensorType::NUMERIC;
  def.last_published_float_value = NAN;
  def.last_publish_time = 0;
  this->custom_obis_definitions_.push_back(def);
}

void Dsmr::add_custom_text_sensor(const std::string &obis_code, esphome::text_sensor::TextSensor *sens) {
  if (sens == nullptr) {
    ESP_LOGW(TAG_CUSTOM_SENSORS, "Attempted to register a null custom text sensor for OBIS '%s'. Skipping.", obis_code.c_str());
    return;
  }
  ESP_LOGD(TAG_CUSTOM_SENSORS, "Registering custom text sensor: OBIS '%s', Name '%s'",
           obis_code.c_str(), sens->get_name().c_str());
  for (const auto& map_entry : this->standard_sensor_to_obis_map_) {
      const std::string& standard_sensor_symbolic_name = map_entry.first;
      const std::string& standard_sensor_obis_code = map_entry.second;
      if (obis_code == standard_sensor_obis_code) {
          auto it_ptr_map = this->standard_text_sensor_pointers_.find(standard_sensor_symbolic_name);
          if (it_ptr_map != this->standard_text_sensor_pointers_.end()) {
               if (*(it_ptr_map->second) != nullptr) {
                  ESP_LOGI(TAG_CUSTOM_SENSORS, "Custom text sensor for OBIS '%s' (Name: '%s') overrides standard sensor '%s'. Standard sensor will be disabled.",
                           obis_code.c_str(), sens->get_name().c_str(), standard_sensor_symbolic_name.c_str());
                  *(it_ptr_map->second) = nullptr;
               } else {
                  ESP_LOGV(TAG_CUSTOM_SENSORS, "Standard text sensor '%s' (OBIS: %s) was already null.",
                            standard_sensor_symbolic_name.c_str(), obis_code.c_str());
               }
          } else {
              ESP_LOGW(TAG_CUSTOM_SENSORS, "OBIS code '%s' matches standard sensor '%s', but its pointer-to-pointer was not found in standard_text_sensor_pointers_ map.",
                        obis_code.c_str(), standard_sensor_symbolic_name.c_str());
          }
          break;
      }
  }
  CustomObisSensorDefinition def;
  def.obis_code_str = obis_code;
  def.numeric_sensor_ptr = nullptr;
  def.text_sensor_ptr = sens;
  def.type = CustomObisSensorType::TEXT;
  def.last_published_text_value = "";
  def.last_publish_time = 0;
  this->custom_obis_definitions_.push_back(def);
}

}  // namespace dsmr_custom
}  // namespace esphome

#endif  // USE_ARDUINO
