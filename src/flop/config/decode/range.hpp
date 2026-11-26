#pragma once

#include <array>
#include <vector>

#include "flop/config/load.hpp"

namespace flop::config {

// decode two ordered finite values without imposing a domain-specific range.
inline std::array<double, 2> decode_ordered_range(const ConfigView &field) {
  std::vector<double> values;
  for (const auto &value : field.sequence()) values.push_back(value.as<double>());
  if (values.size() != 2 || values[1] < values[0])
    field.fail(field.mark(), "Expected an ordered pair of values");
  return {values[0], values[1]};
}

} // namespace flop::config
