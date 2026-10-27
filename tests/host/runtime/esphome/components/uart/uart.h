#pragma once
#include <deque>
#include <cstdint>
#include <cstddef>
namespace esphome { namespace uart {
class UARTComponent {
 public:
  size_t get_rx_buffer_size() const { return rx_buffer_size; }
  size_t rx_buffer_size = 1700;
  std::deque<uint8_t> input;
};
class UARTDevice {
 public:
  UARTDevice(UARTComponent *p) : parent_(p) {}
  size_t available() const { return parent_->input.size(); }
  int read() {
    if (parent_->input.empty()) return -1;
    int c = parent_->input.front();
    parent_->input.pop_front();
    return c;
  }
 protected:
  UARTComponent *parent_;
};
} }
