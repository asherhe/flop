#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "flop/config/load.hpp"
#include "flop/utils/key_curve.hpp"

namespace flop::config {

template <
    typename T,
    typename Interpolation = LinearInterpolation<T>,
    typename Decode,
    typename Validate>
KeyCurve<T, Interpolation> decode_key_curve(
    const ConfigView &field, Decode decode, Validate validate
) {
  std::map<double, T> result;
  for (const auto &entry : field.entries()) {
    double key{};
    try {
      std::size_t consumed{};
      key = std::stod(entry.key, &consumed);
      if (consumed != entry.key.size() || !std::isfinite(key))
        entry.value.fail(entry.value.mark(), "Invalid curve key");
    } catch (const std::invalid_argument &) {
      entry.value.fail(entry.value.mark(), "Invalid curve key");
    } catch (const std::out_of_range &) {
      entry.value.fail(entry.value.mark(), "Invalid curve key");
    }
    std::invoke(validate, key, entry.value);
    if (!result.emplace(key, std::invoke(decode, entry.value)).second)
      entry.value.fail(entry.value.mark(), "Duplicate curve key");
  }
  if (result.empty())
    field.fail(field.mark(), "Curve must have at least one key");
  std::vector<CurveKey<T>> keys;
  keys.reserve(result.size());
  for (auto &[time, value] : result)
    keys.push_back({time, std::move(value)});
  return KeyCurve<T, Interpolation>(std::move(keys));
}

} // namespace flop::config
