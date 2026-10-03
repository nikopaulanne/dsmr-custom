#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_millis;
inline uint32_t millis() { return test_millis; }
// A wait changes the fake clock, allowing tests to detect blocking reception.
inline void yield() { ++test_millis; }
inline void delay(uint32_t n) { test_millis += n; }
class GPIOPin {
 public:
  void setup() {}
  void digital_write(bool v) { value = v; }
  bool digital_read() { return value; }
  bool value = false;
};
}
