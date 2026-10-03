#pragma once
#include <string>
#include <vector>
namespace esphome { namespace text_sensor {
class TextSensor {
 public:
  void publish_state(const std::string &v) { values.push_back(v); }
  const std::string &get_name() const { return name; }
  std::string name = "synthetic";
  std::vector<std::string> values;
};
} }
