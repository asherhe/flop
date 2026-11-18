#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "flop/craft/resource_config.hpp"

namespace flop::craft {

// owns the fixed properties of the available resource types.
class ResourceRegistry {
public:
  explicit ResourceRegistry(std::vector<ResourceConfig> resources);

  [[nodiscard]] std::span<const ResourceConfig> resources() const noexcept { return resources_; }
  [[nodiscard]] const ResourceConfig &at(std::string_view id) const;

private:
  std::vector<ResourceConfig> resources_;
};

} // namespace flop::craft
