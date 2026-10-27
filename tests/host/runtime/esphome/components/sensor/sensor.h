#pragma once
#include <string>
#include <vector>
namespace esphome { namespace sensor {
class Sensor {
 public:
  void publish_state(float v) { values.push_back(v); }
  const std::string &get_name() const { return name; }
  std::string name = "synthetic";
  std::vector<float> values;
};
} }
