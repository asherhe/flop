#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/craft/plugins/part_plugin.hpp"

namespace flop::craft {
class ResourceRegistry;
} // namespace flop::craft

namespace flop::craft::plugin {

struct ResourceConfig;

// mutable runtime quantity of one resource type inside one container.
struct ResourceSlot {
  std::string resource_id;
  double amount_units;
  double max_amount_units;
  int priority;
};

// runtime resource container. owns the tank slots for one installed resource
// container plugin, validated and clamped from its configuration at
// construction. a disabled plugin still holds its contents (they count toward
// mass) but is not drawn from. holds no per-step behavior.
class ResourcePlugin : public PartPlugin {
public:
  ResourcePlugin(const ResourceConfig &config, const ResourceRegistry &registry, bool enabled);

  void tick(CraftContext &ctx, double dt_sec) override;

  [[nodiscard]] std::span<const ResourceSlot> slots() const noexcept { return slots_; }

  // the slot for a resource within this container, or nullptr if this container
  // does not hold that resource.
  [[nodiscard]] const ResourceSlot *find_slot(std::string_view resource_id) const;
  [[nodiscard]] ResourceSlot *find_slot(std::string_view resource_id);

private:
  std::vector<ResourceSlot> slots_;
};

} // namespace flop::craft::plugin
