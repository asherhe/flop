#pragma once

#include "flop/world/craft_id.hpp"

namespace flop::craft {
class Craft;
}

namespace flop::world {
// Narrow access to world-owned craft for external control operations.
class CraftAccess {
public:
  virtual ~CraftAccess() = default;
  [[nodiscard]] virtual const craft::Craft *find_craft(CraftId id) const noexcept = 0;
  [[nodiscard]] virtual craft::Craft *mutable_craft(CraftId id) noexcept = 0;
};
} // namespace flop::world
