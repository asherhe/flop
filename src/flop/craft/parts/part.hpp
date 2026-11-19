#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "flop/craft/parts/part_config.hpp"

namespace flop::craft {

namespace plugin {
class PartPlugin;
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
  // configuration. the registry validates resource containers.
  void install_plugins(const ResourceRegistry &registry);

  // advance this part by one simulation step, ticking its enabled plugins.
  void step(CraftContext &ctx, double dt_sec);

  [[nodiscard]] std::span<const std::unique_ptr<plugin::PartPlugin>> installed_plugins() const noexcept {
    return installed_plugins_;
  }

private:
  std::vector<std::unique_ptr<plugin::PartPlugin>> installed_plugins_;
};

} // namespace flop::craft
