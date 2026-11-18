#include "flop/craft/resource_registry.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "flop/math/utils.hpp"

namespace flop::craft {

ResourceRegistry::ResourceRegistry(std::vector<ResourceConfig> resources) :
    resources_(std::move(resources)) {
  for (auto entry = resources_.begin(); entry != resources_.end(); ++entry) {
    if (entry->id.empty() || entry->name.empty() || !positive_finite(entry->kg_per_unit)) {
      throw std::invalid_argument("Invalid resource type");
    }
    if (std::find_if(resources_.begin(), entry, [&](const ResourceConfig &prior) {
          return prior.id == entry->id;
        }) != entry) {
      throw std::invalid_argument("Duplicate resource ID: " + entry->id);
    }
  }
}

const ResourceConfig &ResourceRegistry::at(std::string_view id) const {
  const auto found = std::ranges::find(resources_, id, &ResourceConfig::id);
  if (found == resources_.end()) {
    throw std::invalid_argument("Unknown resource ID: " + std::string(id));
  }
  return *found;
}

} // namespace flop::craft
