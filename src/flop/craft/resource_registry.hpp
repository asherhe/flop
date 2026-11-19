#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "flop/craft/resource_type_config.hpp"

namespace flop::craft {

// owns the fixed properties of the available resource types.
class ResourceRegistry {
public:
  explicit ResourceRegistry(std::vector<ResourceTypeConfig> resources);

  [[nodiscard]] std::span<const ResourceTypeConfig> resources() const noexcept {
    return resources_;
  }
  [[nodiscard]] const ResourceTypeConfig &at(std::string_view id) const;

private:
  std::vector<ResourceTypeConfig> resources_;
};

} // namespace flop::craft
