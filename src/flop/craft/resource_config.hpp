#pragma once

#include <string>

namespace flop::craft {

enum class ResourceFlow { none, stage, craft };

struct ResourceConfig {
  std::string id;
  std::string name;
  double kg_per_unit;
  ResourceFlow flow;
};

} // namespace flop::craft
