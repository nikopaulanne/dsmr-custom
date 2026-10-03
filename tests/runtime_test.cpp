// Exercise production parsing/publication with synthetic UART input and sensors.
#define DSMR_CUSTOM_SENSOR_LIST(F, SEP) F(energy_delivered_lux) SEP F(gas_delivered) SEP F(water_delivered)
#define DSMR_CUSTOM_TEXT_SENSOR_LIST(F, SEP) F(identification) SEP F(timestamp)
#define DSMR_CUSTOM_BOTH ,
#include "dsmr.cpp"
#include <openssl/evp.h>
#include <cstdio>
#include <new>

namespace esphome { uint32_t test_millis = 10000; }

namespace { int fail_allocation_after = -1; }
void *operator new[](size_t size, const std::nothrow_t &) noexcept {
  if (fail_allocation_after == 0) { fail_allocation_after = -1; return nullptr; }
  if (fail_allocation_after > 0) --fail_allocation_after;
  try { return ::operator new[](size); } catch (...) { return nullptr; }
}

#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return false; } } while (0)

using esphome::sensor::Sensor;
using esphome::text_sensor::TextSensor;
using esphome::uart::UARTComponent;

class Hub : public esphome::dsmr_custom::Dsmr {
 public:
  Hub(UARTComponent *uart, bool crc = true) : Dsmr(uart, crc) { set_max_telegram_length(1700); }
  ~Hub() { delete[] telegram_; delete[] crypt_telegram_; }
  bool parse(const std::string &frame) {
    std::memcpy(telegram_, frame.data(), frame.size()); bytes_read_ = frame.size();
    return parse_telegram();
  }
  using Dsmr::parse_numeric_value_from_string;
  bool encrypted() const { return has_decryption_key_; }
  bool pending() const { return header_found_; }
  bool requesting() const { return requesting_data_; }
  size_t plain_bytes() const { return bytes_read_; }
};

std::string telegram(const std::string &body) {
  std::string frame = "/TEST9\r\n\r\n" + body + "\r\n!";
  uint16_t crc = 0;
  for (unsigned char c : frame) crc = _crc16_update(crc, c);
  char suffix[8]; std::snprintf(suffix, sizeof(suffix), "%04X\r\n", crc);
  return frame + suffix;
}

void feed(UARTComponent &uart, Hub &hub, const std::string &bytes) {
  for (unsigned char c : bytes) uart.input.push_back(c);
  while (!uart.input.empty()) hub.loop();
}

bool test_crc_and_custom_fallback() {
  UARTComponent uart; Hub hub(&uart); hub.setup();
  Sensor custom; TextSensor raw;
  hub.add_custom_numeric_sensor("1-0:1.8.0", &custom); hub.set_telegram(&raw);
  auto valid = telegram("1-0:1.8.0(001.234*kWh)");
  auto bad = valid; bad[bad.size() - 3] = bad[bad.size() - 3] == '0' ? '1' : '0';
  CHECK(!hub.parse(bad)); CHECK(custom.values.empty()); CHECK(hub.warning);
  CHECK(raw.values.back() == bad);
  for (const auto &frame : {std::string("/TEST9\r\n1-0:1.8.0(9*kWh)"),
                            std::string("/TEST9\r\n1-0:1.8.0(9*kWh)\r\n!+ABC\r\n"),
                            valid.substr(0, valid.size() - 5), valid + "junk"}) {
    CHECK(!hub.parse(frame)); CHECK(custom.values.empty());
  }
  CHECK(hub.parse(valid)); CHECK(custom.values.size() == 1); CHECK(!hub.warning);
  // A standard-field error must not suppress valid custom measurements.
  CHECK(!hub.parse(telegram("0-0:1.0.0(BAD)\r\n1-0:1.8.0(002.234*kWh)")));
  CHECK(custom.values.size() == 2);
  auto wrapped = telegram("1-0:1.8.0\r\n(003.234*kWh)");
  CHECK(hub.parse(wrapped)); CHECK(custom.values.size() == 3);
  CHECK(raw.values.back() == wrapped);
  return true;
}

bool test_overrides_and_duplicate_definitions() {
  for (bool custom_first : {false, true}) {
    UARTComponent uart; Hub hub(&uart); hub.setup();
    Sensor custom, standard, second; TextSensor text;
    if (!custom_first) hub.set_energy_delivered_lux(&standard);
    hub.add_custom_numeric_sensor("1-0:1.8.0", &custom);
    if (custom_first) hub.set_energy_delivered_lux(&standard);
    hub.add_custom_numeric_sensor("1-0:1.8.0*255", &second);
    hub.add_custom_text_sensor("1-0:1.8.0", &text);
    CHECK(hub.parse(telegram("1-0:1.8.0(004.234*kWh)")));
    CHECK(standard.values.empty()); CHECK(custom.values.size() == 1);
    CHECK(second.values.size() == 1); CHECK(text.values.back() == "004.234*kWh");
  }
  UARTComponent uart; Hub hub(&uart); hub.setup();
  TextSensor gas, water; Sensor gas_standard, water_standard;
  hub.add_custom_text_sensor("0-3:24.2.1", &gas);
  hub.add_custom_text_sensor("0-4:24.2.1", &water);
  hub.set_gas_delivered(&gas_standard); hub.set_water_delivered(&water_standard);
  CHECK(hub.parse(telegram("0-3:24.2.1(240101000000W)(001.000*m3)\r\n0-4:24.2.1(240101000000W)(002.000*m3)")));
  CHECK(gas.values.size() == 1 && water.values.size() == 1);
  CHECK(gas_standard.values.empty() && water_standard.values.empty());
  return true;
}

bool test_nonblocking_reception_and_limits() {
  UARTComponent uart; uart.rx_buffer_size = 256;
  Hub hub(&uart); hub.setup(); Sensor custom;
  hub.add_custom_numeric_sensor("1-0:1.8.0", &custom);
  const uint32_t start = esphome::test_millis;
  auto frame = telegram("1-0:1.8.0(001.000*kWh)");
  feed(uart, hub, frame.substr(0, 10)); hub.loop();
  CHECK(esphome::test_millis == start); CHECK(hub.pending()); CHECK(custom.values.empty());
  feed(uart, hub, frame.substr(10)); CHECK(custom.values.size() == 1);
  feed(uart, hub, frame.substr(0, 10)); esphome::test_millis += 201; hub.loop();
  CHECK(!hub.pending()); CHECK(!hub.requesting());
  feed(uart, hub, telegram("1-0:1.8.0(002.000*kWh)")); CHECK(custom.values.size() == 2);
  for (int i = 0; i < 1000; ++i) uart.input.push_back('x');
  hub.loop(); CHECK(uart.input.size() == 744); uart.input.clear();
  // Exactly max_telegram_length bytes, including the completing CR, fit.
  UARTComponent exact_uart; Hub exact(&exact_uart); exact.set_max_telegram_length(frame.size() - 1);
  exact.setup(); Sensor exact_sensor; exact.add_custom_numeric_sensor("1-0:1.8.0", &exact_sensor);
  feed(exact_uart, exact, frame); CHECK(exact_sensor.values.size() == 1);
  UARTComponent short_uart; Hub short_hub(&short_uart); short_hub.set_max_telegram_length(frame.size() - 2);
  short_hub.setup(); Sensor short_sensor; short_hub.add_custom_numeric_sensor("1-0:1.8.0", &short_sensor);
  feed(short_uart, short_hub, frame); CHECK(short_sensor.values.empty());
  return true;
}

std::string encrypt_frame(const std::string &plain) {
  std::string frame(18 + plain.size() + 12, '\0');
  frame[0] = static_cast<char>(0xDB); frame[1] = 0x08;
  for (int i = 0; i < 8; ++i) frame[2 + i] = static_cast<char>(0x10 + i);
  frame[11] = static_cast<char>(plain.size() >> 8); frame[12] = static_cast<char>(plain.size());
  frame[13] = 0x30; frame[17] = 1;
  uint8_t key[16], iv[12];
  esphome::dsmr_custom::parse_aes128_key("00112233445566778899aabbccddeeff", key);
  std::memcpy(iv, frame.data() + 2, 8); std::memcpy(iv + 8, frame.data() + 14, 4);
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); int length = 0, final_length = 0;
  if (!ctx || EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, iv) != 1 ||
      EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(&frame[18]), &length,
                        reinterpret_cast<const unsigned char *>(plain.data()), plain.size()) != 1 ||
      EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(&frame[18 + length]), &final_length) != 1 ||
      EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 12, &frame[18 + plain.size()]) != 1)
    std::abort();
  EVP_CIPHER_CTX_free(ctx); return frame;
}

bool test_runtime_keys_and_authenticated_receive() {
  UARTComponent uart; Hub hub(&uart);
  CHECK(hub.set_decryption_key("00112233445566778899aabbccddeeff")); hub.setup();
  Sensor custom; hub.add_custom_numeric_sensor("1-0:1.8.0", &custom);
  auto encrypted = encrypt_frame(telegram("1-0:1.8.0(001.000*kWh)"));
  feed(uart, hub, encrypted); CHECK(custom.values.size() == 1);
  CHECK(!hub.set_decryption_key(std::string(32, 'G'))); CHECK(hub.encrypted());
  encrypted = encrypt_frame(telegram("1-0:1.8.0(002.000*kWh)"));
  auto tampered = encrypted; tampered.back() ^= 1;
  feed(uart, hub, tampered); CHECK(custom.values.size() == 1);
  feed(uart, hub, encrypted.substr(0, 10)); CHECK(hub.pending());
  CHECK(hub.set_decryption_key("")); CHECK(!hub.encrypted() && !hub.pending());
  feed(uart, hub, telegram("1-0:1.8.0(003.000*kWh)")); CHECK(custom.values.size() == 2);
  feed(uart, hub, "/TEST9\r\n"); CHECK(hub.pending());
  CHECK(hub.set_decryption_key("00112233445566778899aabbccddeeff")); CHECK(!hub.pending());
  feed(uart, hub, encrypted); CHECK(custom.values.size() == 3);
  return true;
}

bool test_numeric_and_multi_value_text() {
  UARTComponent uart; Hub hub(&uart); hub.setup();
  for (const auto &value : {"nan", "inf", "1e9999*kWh", "1e-9999", "1.2junk", "240101)(2.3*m3"})
    CHECK(!hub.parse_numeric_value_from_string(value).has_value());
  CHECK(hub.parse_numeric_value_from_string("-1.25*kW").value() == -1.25f);
  TextSensor raw; hub.add_custom_text_sensor("0-1:24.2.1", &raw);
  CHECK(hub.parse(telegram("0-1:24.2.1(240101000000W)(001.234*m3)")));
  CHECK(raw.values.back() == "240101000000W)(001.234*m3");
  return true;
}

bool test_allocation_failures() {
  UARTComponent uart;
  Hub plain(&uart); fail_allocation_after = 0; plain.setup(); CHECK(plain.failed);
  Hub encrypted(&uart); CHECK(encrypted.set_decryption_key("00112233445566778899aabbccddeeff"));
  fail_allocation_after = 1; encrypted.setup(); CHECK(encrypted.failed);
  Hub runtime(&uart); runtime.setup(); fail_allocation_after = 0;
  CHECK(!runtime.set_decryption_key("00112233445566778899aabbccddeeff")); CHECK(!runtime.encrypted());
  return true;
}

int main() {
  if (!test_crc_and_custom_fallback() || !test_overrides_and_duplicate_definitions() ||
      !test_nonblocking_reception_and_limits() || !test_runtime_keys_and_authenticated_receive() ||
      !test_numeric_and_multi_value_text() || !test_allocation_failures()) return 1;
  std::puts("Runtime parser and publication regressions passed"); return 0;
}
