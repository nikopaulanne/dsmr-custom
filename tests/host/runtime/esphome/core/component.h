#pragma once
namespace esphome {
namespace setup_priority { constexpr float LATE = 0; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 0; }
  void mark_failed() { failed = true; }
  void status_set_warning() { warning = true; }
  void status_clear_warning() { warning = false; }
  bool warning = false;
  bool failed = false;
};
}
