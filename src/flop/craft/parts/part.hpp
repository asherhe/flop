#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "flop/craft/parts/config.hpp"

namespace flop::craft {

namespace plugin {
class PartPlugin;
enum class TickPhase : std::uint8_t;
} // namespace plugin
class CraftContext;
class ResourceRegistry;

// one installed part. the geometry and dry properties are a read-only
// description fixed at assembly; the part additionally owns the runtime plugin
// objects built from its authored plugin configuration and drives their
// per-step behavior. move-only: it owns its runtime plugins.
class Part {
public:
  Part();
  Part(const Part &) = delete;
  Part(Part &&) noexcept;
  Part &operator=(const Part &) = delete;
  Part &operator=(Part &&) noexcept;
  ~Part();

  // read-only description fixed at assembly.
  std::string type_id;
  std::string id;
  std::string name;
  Vec2d position_meters;
  double angle_rad;
  double dry_mass_kg;
  bool crossfeed;
  std::vector<AttachmentNodeConfig> attachment_nodes;
  std::vector<PartAttachmentConfig> attachments;
  std::vector<plugin::PartPluginConfig> plugins;

  // build the runtime plugin objects from this part's authored plugin
  // configuration, binding each to this part's index within its craft. the
  // registry validates resource containers.
  void install_plugins(const ResourceRegistry &registry, std::size_t part_index);

  // advance this part by one simulation step, ticking the enabled plugins whose
  // tick phase matches. the craft drives each phase across all parts in turn.
  void step_phase(CraftContext &ctx, double dt_sec, plugin::TickPhase phase);

  [[nodiscard]] std::span<const std::unique_ptr<plugin::PartPlugin>>
  installed_plugins() const noexcept {
    return installed_plugins_;
  }

  // mutable view of the installed plugins, for callers that drive a plugin's
  // own inputs (e.g. a command plugin's throttle/steering).
  [[nodiscard]] std::span<const std::unique_ptr<plugin::PartPlugin>> installed_plugins() noexcept {
    return installed_plugins_;
  }

private:
  std::vector<std::unique_ptr<plugin::PartPlugin>> installed_plugins_;
};

} // namespace flop::craft
