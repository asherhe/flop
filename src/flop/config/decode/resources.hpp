#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "flop/config/load.hpp"
#include "flop/craft/resource_config.hpp"

namespace flop::config {

namespace detail {

inline craft::ResourceFlow decode_resource_flow(const ConfigView &field) {
  const auto value = field.as<std::string>();
  if (value == "None")
    return craft::ResourceFlow::none;
  if (value == "Stage")
    return craft::ResourceFlow::stage;
  if (value == "Craft")
    return craft::ResourceFlow::craft;
  field.fail(field.mark(), "Unknown resource flow mode");
}

} // namespace detail

template <> struct ConfigDecoder<std::vector<craft::ResourceConfig>> {
  [[nodiscard]] static std::vector<craft::ResourceConfig> decode(const ConfigView &document) {
    std::vector<craft::ResourceConfig> result;
    for (const auto &entry : document.sequence()) {
      const auto id = entry.required("type");
      auto resource_id = id.nonempty_string();
      if (std::ranges::find(result, resource_id, &craft::ResourceConfig::id) != result.end()) {
        id.fail(id.mark(), "Duplicate resource ID");
      }
      result.push_back({
          std::move(resource_id),
          entry.required("name").nonempty_string(),
          entry.required("density").positive_double(),
          detail::decode_resource_flow(entry.required("flow")),
      });
    }
    return result;
  }
};

} // namespace flop::config
