/*
 * This file is part of the dsmr_custom ESPHome component.
 *
 * This file is inspired by or based on the original ESPHome DSMR component,
 * available at:
 * https://github.com/esphome/esphome/tree/dev/esphome/components/dsmr
 *
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
 * @brief Implementation of the Dsmr class for the dsmr_custom ESPHome
 * component.
 * @details This file contains the core logic for the DSMR hub. Its architecture
 * is inspired by the native ESPHome DSMR component, but it has been
 * specifically implemented to support custom OBIS sensors and to interact with
 * a modified local parser for enhanced compatibility.
 * @date June 7, 2025
 */

#if defined(USE_ARDUINO) ||                                                    \
    defined(USE_ESP_IDF) // Guard for supported platforms

#include "dsmr.h"                 // Header for this component's Dsmr class
#include "crypto_helpers.h"
#include "esphome/core/helpers.h" // For YESNO, etc.
#include "esphome/core/log.h"

// Cryptography libraries for AES-GCM decryption
#ifdef USE_ARDUINO
#include <AES.h>
#include <GCM.h>
#else
// ESP-IDF: Use the ESP-IDF AES-GCM wrapper
#include "dsmr_crypto.h"
#endif

#include <algorithm> // For std::remove_if, std::min
#include <cctype>    // For ::isspace, ::isalnum
#include <cerrno>
#include <new>
#include <cstdlib>   // For std::strtoul, std::strtof
#include <cstring>   // For memchr, memcpy, strlen, strchr
#include <string>

namespace esphome {
namespace dsmr_custom {

// Logging tag for the main component
static const char *const TAG = "dsmr_custom";
// Specific logging tag for custom sensor processing, for finer-grained log
// control.
static const char *const TAG_CUSTOM_SENSORS = "dsmr_custom.sensor";

/**
 * @brief Constructor for the Dsmr class.
 * @param uart Pointer to the UART component this hub will use for
 * communication.
 * @param crc_check Boolean indicating if CRC check should be performed on
 * telegrams.
 */
Dsmr::Dsmr(uart::UARTComponent *uart, bool crc_check)
    : uart::UARTDevice(uart), crc_check_(crc_check) {}

namespace {

constexpr size_t MAX_BYTES_PER_LOOP = 256;

bool parse_custom_obis_id(const std::string &code, ::dsmr::ObisId *id) {
  std::string normalized = code;
  std::replace(normalized.begin(), normalized.end(), '*', '.');
  const auto result = ::dsmr::ObisIdParser::parse(
      normalized.data(), normalized.data() + normalized.size());
  if (result.err_ || result.next_ != normalized.data() + normalized.size())
    return false;
  *id = result.result_;
  return true;
}

}  // namespace

bool Dsmr::has_custom_sensor_for_(const ::dsmr::ObisId &id) const {
  for (const auto &definition : this->custom_obis_definitions_) {
    if (definition.has_obis_id && definition.obis_id == id)
      return true;
  }
  return false;
}

void Dsmr::setup() {
  ESP_LOGCONFIG(TAG, "Setting up dsmr_custom component...");
  this->telegram_ = new (std::nothrow) char[this->max_telegram_len_ + 1];
  if (this->telegram_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate telegram_ buffer!");
    this->mark_failed();
    return;
  }
  if (this->has_decryption_key_) {
    this->crypt_telegram_ = new (std::nothrow) uint8_t[this->max_telegram_len_ + 1];
    if (this->crypt_telegram_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate encrypted telegram buffer!");
      this->mark_failed();
      return;
    }
  }
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
    this->request_pin_->digital_write(false);
    LOG_PIN("  Request Pin: ", this->request_pin_);
  }
}

void Dsmr::loop() {
  if (this->telegram_ == nullptr || !this->ready_to_request_data_())
    return;
  if (this->has_decryption_key_) {
    if (this->crypt_telegram_ != nullptr)
      this->receive_encrypted_telegram_();
  } else {
    this->receive_telegram_();
  }
}

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
      while (this->available() && discarded_bytes < MAX_BYTES_PER_LOOP) {
        this->read();
        discarded_bytes++;
      }
      if (discarded_bytes > 0) {
        ESP_LOGVV(
            TAG,
            "Discarded %u bytes from UART buffer while not actively reading.",
            discarded_bytes);
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
  if (this->receive_timeout_ == 0)
    return false;
  return (millis() - this->last_read_time_) > this->receive_timeout_;
}

bool Dsmr::available_within_timeout_() {
  // An empty UART queue is normal between bytes. Leave partial frames intact
  // and let the next component loop continue reception without waiting here.
  if (this->available()) {
    this->last_read_time_ = millis();
    return true;
  }
  const bool timed_out = this->header_found_
      ? this->receive_timeout_reached_()
      : (this->receive_timeout_ > 0 &&
         millis() - this->last_request_time_ > this->receive_timeout_);
  if (timed_out) {
    ESP_LOGW(TAG, "Timeout receiving P1 telegram (header: %s, plain bytes: %zu, encrypted bytes: %zu).",
             YESNO(this->header_found_), this->bytes_read_, this->crypt_bytes_read_);
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
  if (this->requesting_data_ ||
      (this->request_pin_ != nullptr && this->request_pin_->digital_read())) {
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
  size_t processed = 0;
  while (processed++ < MAX_BYTES_PER_LOOP && this->available_within_timeout_()) {
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
      ESP_LOGE(
          TAG,
          "Error: Plain telegram larger than buffer (%zu bytes). Discarding.",
          this->max_telegram_len_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    this->telegram_[this->bytes_read_++] = c;
    if (c == '!') {
      this->footer_found_ = true;
      ESP_LOGV(
          TAG,
          "Footer of plain telegram found ('!'). Expecting CRC and newline.");
    } else if (this->footer_found_ && (c == '\n' || c == '\r')) {
      this->telegram_[this->bytes_read_] = '\0';
      ESP_LOGV(
          TAG,
          "End of plain telegram detected (newline after CRC). Length: %zu",
          this->bytes_read_);
      this->parse_telegram();
      this->reset_telegram_();
      return;
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  if (this->crypt_telegram_ == nullptr) {
    ESP_LOGE(TAG, "Encrypted receive called, but crypt_telegram_ buffer is not "
                  "allocated. Decryption key issue?");
    this->reset_telegram_();
    this->stop_requesting_data_();
    return;
  }
  size_t processed = 0;
  while (processed++ < MAX_BYTES_PER_LOOP && this->available_within_timeout_()) {
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
      ESP_LOGW(TAG,
               "Encrypted telegram reception: Expected 0xDB as first byte "
               "after header_found, got 0x%02X. Resetting.",
               c_byte);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    if (this->crypt_bytes_read_ >= this->max_telegram_len_) {
      ESP_LOGE(TAG,
               "Error: Encrypted telegram frame larger than buffer (%zu "
               "bytes). Discarding.",
               this->max_telegram_len_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    this->crypt_telegram_[this->crypt_bytes_read_++] = c_byte;
    if (this->crypt_telegram_len_ == 0 && this->crypt_bytes_read_ >= 13) {
      if (this->crypt_telegram_[0] != 0xDB ||
          this->crypt_telegram_[1] != 0x08) {
        ESP_LOGE(TAG,
                 "Invalid encrypted frame header: %02X%02X. Expected DB08. "
                 "Discarding.",
                 this->crypt_telegram_[0], this->crypt_telegram_[1]);
        this->reset_telegram_();
        this->stop_requesting_data_();
        return;
      }
      size_t len_info = (static_cast<size_t>(this->crypt_telegram_[11]) << 8) |
                        static_cast<size_t>(this->crypt_telegram_[12]);
      if (!encrypted_frame_size(len_info, this->max_telegram_len_,
                                &this->crypt_telegram_len_)) {
        ESP_LOGE(TAG,
                 "Encrypted frame payload length (%zu) is invalid for buffer "
                 "size (%zu). Discarding.",
                 len_info, this->max_telegram_len_);
        this->reset_telegram_();
        this->stop_requesting_data_();
        return;
      }
      ESP_LOGV(TAG,
               "Encrypted telegram expected total frame length: %zu bytes "
               "(LEN_INFO: %zu)",
               this->crypt_telegram_len_, len_info);
      if (this->crypt_telegram_len_ > this->max_telegram_len_) {
        ESP_LOGE(TAG,
                 "Calculated encrypted frame length (%zu) exceeds buffer "
                 "(%zu). Discarding.",
                 this->crypt_telegram_len_, this->max_telegram_len_);
        this->reset_telegram_();
        this->stop_requesting_data_();
        return;
      }
    }
    if (this->crypt_telegram_len_ == 0 ||
        this->crypt_bytes_read_ < this->crypt_telegram_len_) {
      continue;
    }
    ESP_LOGV(
        TAG,
        "End of encrypted telegram frame found (read %zu bytes, expected %zu).",
        this->crypt_bytes_read_, this->crypt_telegram_len_);
    const size_t ciphertext_offset = DSMR_ENCRYPTED_FRAME_HEADER_SIZE;
    const size_t gcm_tag_length = DSMR_ENCRYPTED_FRAME_TAG_SIZE;
    size_t len_info_from_frame =
        (static_cast<size_t>(this->crypt_telegram_[11]) << 8) |
        static_cast<size_t>(this->crypt_telegram_[12]);
    if (this->crypt_bytes_read_ < (ciphertext_offset + gcm_tag_length) ||
        len_info_from_frame == 0) {
      ESP_LOGE(TAG,
               "Encrypted data too short for IV, ciphertext, and GCM tag, or "
               "LEN_INFO is zero. Read: %zu, LEN_INFO: %zu",
               this->crypt_bytes_read_, len_info_from_frame);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    size_t ciphertext_len = len_info_from_frame;
    if (ciphertext_offset + ciphertext_len + gcm_tag_length !=
        this->crypt_telegram_len_) {
      ESP_LOGE(TAG,
               "Encrypted frame length mismatch. Expected based on LEN_INFO: "
               "%zu, Actual frame read: %zu",
               ciphertext_offset + ciphertext_len + gcm_tag_length,
               this->crypt_bytes_read_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    // Prepare IV and pointers
    uint8_t iv[12];
    memcpy(iv, &this->crypt_telegram_[2], 8);
    memcpy(iv + 8, &this->crypt_telegram_[14], 4);
    ESP_LOGV(TAG,
             "Decryption IV (Hex): %02X%02X%02X%02X%02X%02X%02X%02X "
             "%02X%02X%02X%02X",
             iv[0], iv[1], iv[2], iv[3], iv[4], iv[5], iv[6], iv[7], iv[8],
             iv[9], iv[10], iv[11]);
    uint8_t *ciphertext_ptr = &this->crypt_telegram_[ciphertext_offset];
    if (ciphertext_len > this->max_telegram_len_) {
      ESP_LOGE(TAG,
               "Decrypted data length (%zu) would exceed plain telegram_ "
               "buffer (%zu).",
               ciphertext_len, this->max_telegram_len_);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }

#ifdef USE_ARDUINO
    uint8_t *tag_ptr =
        &this->crypt_telegram_[ciphertext_offset + ciphertext_len];
    // Arduino: Use Crypto library
    GCM<AES128> gcmaes128;
    gcmaes128.setKey(this->decryption_key_.data(), gcmaes128.keySize());
    gcmaes128.setIV(iv, sizeof(iv));
    gcmaes128.decrypt(reinterpret_cast<uint8_t *>(this->telegram_),
                      ciphertext_ptr, ciphertext_len);

    if (!gcmaes128.checkTag(tag_ptr, gcm_tag_length)) {
      ESP_LOGW(TAG, "Decryption failed! GCM tag mismatch.");
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
#else
    // ESP-IDF: IDF 5 uses AES-GCM; IDF 6+ uses PSA Crypto
    int decrypt_result = dsmr_aes_gcm_decrypt(
        this->decryption_key_.data(), this->decryption_key_.size(), iv,
        sizeof(iv), ciphertext_ptr, ciphertext_len, gcm_tag_length,
        reinterpret_cast<unsigned char *>(this->telegram_));

    if (decrypt_result != 0) {
      ESP_LOGW(TAG, "Decryption failed! Error code: %d", decrypt_result);
      this->reset_telegram_();
      this->stop_requesting_data_();
      return;
    }
    ESP_LOGD(TAG, "ESP-IDF: AES-GCM decryption successful.");
#endif

    this->telegram_[ciphertext_len] = '\0';
    this->bytes_read_ = ciphertext_len;
    ESP_LOGD(TAG,
             "Decryption successful. Decrypted P1 telegram size: %zu bytes.",
             this->bytes_read_);
    ESP_LOGVV(TAG, "Decrypted P1 telegram content:\n%s", this->telegram_);
    this->parse_telegram();
    this->reset_telegram_();
    return;
  }
}

optional<float>
Dsmr::parse_numeric_value_from_string(const std::string &value_str) {
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
  temp_val.erase(std::remove_if(temp_val.begin(), temp_val.end(),
                               [](unsigned char c) { return std::isspace(c); }),
                 temp_val.end());
  if (temp_val.empty()) {
    return {};
  }
  char *end_ptr = nullptr;
  errno = 0;
  float val = std::strtof(temp_val.c_str(), &end_ptr);
  if (end_ptr == temp_val.c_str() || *end_ptr != '\0' ||
      errno == ERANGE || !std::isfinite(val)) {
    ESP_LOGVV(TAG_CUSTOM_SENSORS,
              "strtof failed for '%s'. end_ptr points to '%c' (0x%02X)",
              temp_val.c_str(), (end_ptr ? *end_ptr : '0'),
              (end_ptr ? *end_ptr : 0));
    return {};
  }
  return val;
}

std::string Dsmr::parse_text_value_from_string(const std::string &value_str) {
  std::string parsed_text = value_str;
  if (!parsed_text.empty() && parsed_text.front() == '(' &&
      parsed_text.back() == ')') {
    if (parsed_text.length() >= 2) {
      parsed_text = parsed_text.substr(1, parsed_text.length() - 2);
    } else {
      return "";
    }
  }
  return parsed_text;
}

void Dsmr::process_line_for_custom_sensors(const char *line_buffer,
                                           size_t length) {
  if (length == 0)
    return;
  std::string line_str(line_buffer, length);
  size_t open_paren_pos = line_str.find('(');
  size_t close_paren_pos = line_str.rfind(')');

  if (open_paren_pos == std::string::npos ||
      close_paren_pos == std::string::npos ||
      open_paren_pos >= close_paren_pos ||
      (close_paren_pos == open_paren_pos + 1)) {
    ESP_LOGVV(TAG_CUSTOM_SENSORS,
              "Line '%s' not a valid OBIS value format for custom parsing.",
              line_str.c_str());
    return;
  }

  std::string obis_code_str = line_str.substr(0, open_paren_pos);
  std::string value_part_str = line_str.substr(
      open_paren_pos + 1, close_paren_pos - (open_paren_pos + 1));
  obis_code_str.erase(
      std::remove_if(obis_code_str.begin(), obis_code_str.end(),
                     [](unsigned char c) { return std::isspace(c); }),
      obis_code_str.end());

  if (obis_code_str.empty()) {
    ESP_LOGVV(TAG_CUSTOM_SENSORS, "Empty OBIS code extracted from line '%s'.",
              line_str.c_str());
    return;
  }

  ESP_LOGVV(TAG_CUSTOM_SENSORS,
            "Processing line for custom sensors: OBIS '%s', ValuePart '%s'",
            obis_code_str.c_str(), value_part_str.c_str());

  ::dsmr::ObisId line_id;
  const bool has_line_id = parse_custom_obis_id(obis_code_str, &line_id);
  for (auto &custom_def : this->custom_obis_definitions_) {
    if (custom_def.obis_code_str == obis_code_str ||
        (has_line_id && custom_def.has_obis_id && custom_def.obis_id == line_id)) {
      if (custom_def.type == CustomObisSensorType::NUMERIC &&
          custom_def.numeric_sensor_ptr != nullptr) {
        optional<float> val_opt =
            this->parse_numeric_value_from_string(value_part_str);
        if (val_opt.has_value()) {
          float current_value = val_opt.value();
          // Accessing static constexpr members via class name or this-> (if
          // not shadowed)
          bool value_changed_significantly =
              std::isnan(custom_def.last_published_float_value) ||
              (std::fabs(current_value -
                         custom_def.last_published_float_value) >
               Dsmr::CUSTOM_SENSOR_FLOAT_TOLERANCE);
          bool interval_passed = (millis() - custom_def.last_publish_time) >=
                                 Dsmr::CUSTOM_SENSOR_MIN_PUBLISH_INTERVAL_MS;

          if (value_changed_significantly || interval_passed) {
            custom_def.numeric_sensor_ptr->publish_state(current_value);
            custom_def.last_published_float_value = current_value;
            custom_def.last_publish_time = millis();
            ESP_LOGD(TAG_CUSTOM_SENSORS,
                     "Published to custom numeric sensor '%s' (OBIS: %s): %.3f",
                     custom_def.numeric_sensor_ptr->get_name().c_str(),
                     obis_code_str.c_str(), current_value);
          }
        } else {
          ESP_LOGW(TAG_CUSTOM_SENSORS,
                   "Failed to parse float for custom OBIS '%s' from value part "
                   "'%s' on line '%s'",
                   obis_code_str.c_str(), value_part_str.c_str(),
                   line_str.c_str());
        }
      } else if (custom_def.type == CustomObisSensorType::TEXT &&
                 custom_def.text_sensor_ptr != nullptr) {
        std::string parsed_text =
            this->parse_text_value_from_string(value_part_str);
        bool value_changed =
            custom_def.last_published_text_value != parsed_text;
        bool interval_passed = (millis() - custom_def.last_publish_time) >=
                               Dsmr::CUSTOM_SENSOR_MIN_PUBLISH_INTERVAL_MS;

        if (value_changed || interval_passed) {
          custom_def.text_sensor_ptr->publish_state(parsed_text);
          custom_def.last_published_text_value = parsed_text;
          custom_def.last_publish_time = millis();
          ESP_LOGD(TAG_CUSTOM_SENSORS,
                   "Published to custom text sensor '%s' (OBIS: %s): %s",
                   custom_def.text_sensor_ptr->get_name().c_str(),
                   obis_code_str.c_str(), parsed_text.c_str());
        }
      }
    }
  }
}

void Dsmr::normalize_value_lines_() {
  // Preserve the existing wrapped-value compatibility, after CRC validation.
  size_t written = 0;
  for (size_t read = 0; read < this->bytes_read_; ++read) {
    const char c = this->telegram_[read];
    if (c == '(') {
      while (written > 0 && (this->telegram_[written - 1] == '\r' ||
                             this->telegram_[written - 1] == '\n'))
        --written;
    }
    this->telegram_[written++] = c;
  }
  this->bytes_read_ = written;
  this->telegram_[written] = '\0';
}

bool Dsmr::parse_telegram() {
  if (this->telegram_ == nullptr || this->bytes_read_ > this->max_telegram_len_)
    return false;
  this->telegram_[this->bytes_read_] = '\0';
  // Raw diagnostics retain the exact wire frame, including rejected frames.
  if (this->s_telegram_ != nullptr)
    this->s_telegram_->publish_state(std::string(this->telegram_, this->bytes_read_));

  const auto frame_result = ::dsmr::P1Parser::validate_telegram(
      this->telegram_, this->bytes_read_, this->crc_check_);
  if (frame_result.err_) {
    const auto error = frame_result.fullError(this->telegram_, this->telegram_ + this->bytes_read_);
    ESP_LOGW(TAG, "Rejected P1 frame: %s", error.c_str());
    this->status_set_warning();
    this->stop_requesting_data_();
    return false;
  }

  this->normalize_value_lines_();
  MyData data_from_standard_parser;
  // CRC was already checked on the original bytes. Unsupported standard fields
  // can fail independently without discarding valid custom OBIS measurements.
  const auto standard_parse_result = ::dsmr::P1Parser::parse(
      &data_from_standard_parser, this->telegram_, this->bytes_read_, false, false);

  if (standard_parse_result.err_) {
    auto err_str = standard_parse_result.fullError(
        this->telegram_, this->telegram_ + this->bytes_read_);
    ESP_LOGW(TAG, "DSMR P1 vendored parser error: %s", err_str.c_str());
    this->status_set_warning();
  } else {
    ESP_LOGD(TAG, "Successfully parsed P1 telegram using vendored parser for "
                  "standard fields.");
    this->status_clear_warning();
    this->publish_sensors(data_from_standard_parser);
  }

  ESP_LOGV(TAG, "Processing telegram for custom OBIS sensors line by line.");
  const char *current_line_start = this->telegram_;
  const char *telegram_buffer_end = static_cast<const char *>(
      memchr(this->telegram_, '!', this->bytes_read_));

  while (!this->custom_obis_definitions_.empty() &&
         current_line_start < telegram_buffer_end &&
         *current_line_start != '\0') {
    const char *line_feed_pos = static_cast<const char *>(memchr(
        current_line_start, '\n', telegram_buffer_end - current_line_start));
    const char *carriage_return_pos = static_cast<const char *>(memchr(
        current_line_start, '\r', telegram_buffer_end - current_line_start));
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
      const char *exclamation_pos = static_cast<const char *>(memchr(
          current_line_start, '!', telegram_buffer_end - current_line_start));
      if (exclamation_pos && exclamation_pos < telegram_buffer_end) {
        current_line_length = exclamation_pos - current_line_start;
      } else {
        current_line_length = telegram_buffer_end - current_line_start;
      }
    }
    if (current_line_length > 0) {
      this->process_line_for_custom_sensors(current_line_start,
                                            current_line_length);
    }
    if (actual_line_end_char != nullptr) {
      current_line_start = actual_line_end_char + 1;
      while (!this->custom_obis_definitions_.empty() &&
         current_line_start < telegram_buffer_end &&
             (*current_line_start == '\r' || *current_line_start == '\n')) {
        current_line_start++;
      }
    } else {
      break;
    }
  }
  this->stop_requesting_data_();
  return !standard_parse_result.err_; // CORRECTED: Access err_
}

void Dsmr::publish_sensors(MyData &data) {
#define DSMR_PUBLISH_STANDARD_SENSOR(s) \
  if (data.s##_present_ && this->s_##s##_ != nullptr && \
      !this->has_custom_sensor_for_(::dsmr::fields::s::id_)) { \
    this->s_##s##_->publish_state(data.s##_); \
  }
  DSMR_CUSTOM_SENSOR_LIST(DSMR_PUBLISH_STANDARD_SENSOR, )

#define DSMR_PUBLISH_STANDARD_TEXT_SENSOR(s) \
  if (data.s##_present_ && this->s_##s##_ != nullptr && \
      !this->has_custom_sensor_for_(::dsmr::fields::s::id_)) { \
    this->s_##s##_->publish_state(data.s##_.c_str()); \
  }
  DSMR_CUSTOM_TEXT_SENSOR_LIST(DSMR_PUBLISH_STANDARD_TEXT_SENSOR, )
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG, "DSMR Custom Component Configuration:");
  ESP_LOGCONFIG(TAG, "  UART Bus: Configured (details in UART component logs)");
  ESP_LOGCONFIG(TAG, "  Max Telegram Length: %zu bytes",
                this->max_telegram_len_);
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_);
  ESP_LOGCONFIG(TAG, "  CRC Check Enabled: %s", YESNO(this->crc_check_));
  if (this->request_pin_ != nullptr) {
    LOG_PIN("  Request Pin: ", this->request_pin_);
    ESP_LOGCONFIG(TAG, "  Request Interval: %u ms", this->request_interval_);
  } else {
    ESP_LOGCONFIG(TAG, "  Request Pin: Not configured");
    if (this->request_interval_ > 0) {
      ESP_LOGCONFIG(TAG, "  Passive Read Interval: %u ms",
                    this->request_interval_);
    } else {
      ESP_LOGCONFIG(TAG, "  Passive Read Interval: Continuous attempt");
    }
  }
  if (this->has_decryption_key_) {
    ESP_LOGCONFIG(TAG, "  Decryption: Enabled (key is set)");
  } else {
    ESP_LOGCONFIG(TAG, "  Decryption: Disabled (no key set)");
  }
  ESP_LOGCONFIG(TAG, "  Standard Sensors (matching custom OBIS sensors suppress publication):");
#define DSMR_LOG_STANDARD_SENSOR_IMPL(s)                                       \
  if (this->s_##s##_ != nullptr) {                                             \
    LOG_SENSOR("    ", #s, this->s_##s##_);                                    \
  } else {                                                                     \
    ESP_LOGCONFIG(                                                             \
        TAG,                                                                   \
        "    %s (numeric): Not configured.",    \
        #s);                                                                   \
  }
  DSMR_CUSTOM_SENSOR_LIST(DSMR_LOG_STANDARD_SENSOR_IMPL, )
#define DSMR_LOG_STANDARD_TEXT_SENSOR_IMPL(s)                                  \
  if (this->s_##s##_ != nullptr) {                                             \
    LOG_TEXT_SENSOR("    ", #s, this->s_##s##_);                               \
  } else {                                                                     \
    ESP_LOGCONFIG(                                                             \
        TAG, "    %s (text): Not configured.",  \
        #s);                                                                   \
  }
  DSMR_CUSTOM_TEXT_SENSOR_LIST(DSMR_LOG_STANDARD_TEXT_SENSOR_IMPL, )
  LOG_TEXT_SENSOR("  ", "Full Telegram Text Sensor (s_telegram_)",
                  this->s_telegram_);

  if (!this->custom_obis_definitions_.empty()) {
    ESP_LOGCONFIG(TAG_CUSTOM_SENSORS,
                  "  Custom OBIS Sensors Registered (%zu total):",
                  this->custom_obis_definitions_.size());
    for (const auto &def : this->custom_obis_definitions_) {
      if (def.type == CustomObisSensorType::NUMERIC &&
          def.numeric_sensor_ptr != nullptr) {
        ESP_LOGCONFIG(TAG_CUSTOM_SENSORS,
                      "    - OBIS: '%s', Name: '%s' (Numeric Sensor)",
                      def.obis_code_str.c_str(),
                      def.numeric_sensor_ptr->get_name().c_str());
      } else if (def.type == CustomObisSensorType::TEXT &&
                 def.text_sensor_ptr != nullptr) {
        ESP_LOGCONFIG(
            TAG_CUSTOM_SENSORS, "    - OBIS: '%s', Name: '%s' (Text Sensor)",
            def.obis_code_str.c_str(), def.text_sensor_ptr->get_name().c_str());
      }
    }
  } else {
    ESP_LOGCONFIG(TAG_CUSTOM_SENSORS, "  No Custom OBIS Sensors registered.");
  }
}

bool Dsmr::set_decryption_key(const std::string &decryption_key_hex) {
  uint8_t parsed_key[DSMR_AES128_KEY_SIZE] = {};
  if (!decryption_key_hex.empty() && !parse_aes128_key(decryption_key_hex, parsed_key)) {
    ESP_LOGE(TAG, "Decryption key must be exactly 32 hexadecimal characters; previous key retained.");
    return false;
  }
  if (!decryption_key_hex.empty() && this->has_decryption_key_ &&
      std::equal(parsed_key, parsed_key + DSMR_AES128_KEY_SIZE, this->decryption_key_.begin()))
    return true;

  // During code generation setup has not allocated the plain buffer yet.
  // Runtime activation allocates before replacing a working mode or key.
  if (!decryption_key_hex.empty() && this->telegram_ != nullptr && this->crypt_telegram_ == nullptr) {
    this->crypt_telegram_ = new (std::nothrow) uint8_t[this->max_telegram_len_ + 1];
    if (this->crypt_telegram_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate encrypted telegram buffer; previous key retained.");
      return false;
    }
  }
  if (this->telegram_ != nullptr)
    this->stop_requesting_data_();
  this->reset_telegram_();
  std::copy(parsed_key, parsed_key + DSMR_AES128_KEY_SIZE, this->decryption_key_.begin());
  this->has_decryption_key_ = !decryption_key_hex.empty();
  if (!this->has_decryption_key_) {
    delete[] this->crypt_telegram_;
    this->crypt_telegram_ = nullptr;
    ESP_LOGI(TAG, "DSMR telegram decryption disabled (key cleared).");
  } else {
    ESP_LOGI(TAG, "DSMR telegram decryption key is set.");
  }
  return true;
}

void Dsmr::add_custom_numeric_sensor(const std::string &obis_code,
                                     esphome::sensor::Sensor *sens) {
  if (sens == nullptr) {
    ESP_LOGW(TAG_CUSTOM_SENSORS,
             "Attempted to register a null custom numeric sensor for OBIS "
             "'%s'. Skipping.",
             obis_code.c_str());
    return;
  }
  ESP_LOGD(TAG_CUSTOM_SENSORS,
           "Registering custom numeric sensor: OBIS '%s', Name '%s'",
           obis_code.c_str(), sens->get_name().c_str());

  CustomObisSensorDefinition def;
  def.obis_code_str = obis_code;
  def.has_obis_id = parse_custom_obis_id(obis_code, &def.obis_id);
  def.numeric_sensor_ptr = sens;
  def.text_sensor_ptr = nullptr;
  def.type = CustomObisSensorType::NUMERIC;
  def.last_published_float_value = NAN;
  def.last_publish_time = 0;
  this->custom_obis_definitions_.push_back(def);
}

void Dsmr::add_custom_text_sensor(const std::string &obis_code,
                                  esphome::text_sensor::TextSensor *sens) {
  if (sens == nullptr) {
    ESP_LOGW(TAG_CUSTOM_SENSORS,
             "Attempted to register a null custom text sensor for OBIS '%s'. "
             "Skipping.",
             obis_code.c_str());
    return;
  }
  ESP_LOGD(TAG_CUSTOM_SENSORS,
           "Registering custom text sensor: OBIS '%s', Name '%s'",
           obis_code.c_str(), sens->get_name().c_str());
  CustomObisSensorDefinition def;
  def.obis_code_str = obis_code;
  def.has_obis_id = parse_custom_obis_id(obis_code, &def.obis_id);
  def.numeric_sensor_ptr = nullptr;
  def.text_sensor_ptr = sens;
  def.type = CustomObisSensorType::TEXT;
  def.last_published_text_value = "";
  def.last_publish_time = 0;
  this->custom_obis_definitions_.push_back(def);
}

} // namespace dsmr_custom
} // namespace esphome

#endif // defined(USE_ARDUINO) || defined(USE_ESP_IDF)
