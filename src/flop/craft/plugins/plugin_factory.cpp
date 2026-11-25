#include "flop/craft/plugins/plugin_factory.hpp"

#include <variant>

#include "flop/craft/plugins/plugins.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/resource.hpp"

namespace flop::craft::plugin {

std::unique_ptr<PartPlugin> make_plugin(
    const PartPluginConfig &config, const ResourceRegistry &registry
) {
  const bool enabled = config.enabled;
  return std::visit(
      [&](const auto &values) -> std::unique_ptr<PartPlugin> {
        using Config = std::decay_t<decltype(values)>;
        if constexpr (std::is_same_v<Config, CommandConfig>) {
          return std::make_unique<CommandPlugin>(values, enabled);
        } else if constexpr (std::is_same_v<Config, ReactionWheelConfig>) {
          return std::make_unique<ReactionWheelPlugin>(values, enabled);
        } else if constexpr (std::is_same_v<Config, EngineConfig>) {
          return std::make_unique<EnginePlugin>(values, registry, enabled);
        } else if constexpr (std::is_same_v<Config, ResourceConfig>) {
          return std::make_unique<ResourcePlugin>(values, registry, enabled);
        } else if constexpr (std::is_same_v<Config, SpriteConfig>) {
          return std::make_unique<SpritePlugin>(values, enabled);
        } else if constexpr (std::is_same_v<Config, LightConfig>) {
          return std::make_unique<LightPlugin>(values, enabled);
        }
      },
      config.values
  );
}

} // namespace flop::craft::plugin
