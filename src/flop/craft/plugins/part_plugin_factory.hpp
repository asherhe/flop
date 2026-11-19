#pragma once

#include <memory>

namespace flop::craft {
class ResourceRegistry;
} // namespace flop::craft

namespace flop::craft::plugin {

struct PartPluginConfig;
class PartPlugin;

// construct the runtime plugin for one configured plugin, selecting the type
// from the configuration variant and seeding its enabled flag. the registry is
// used to validate resource containers; other plugins ignore it.
[[nodiscard]] std::unique_ptr<PartPlugin> make_plugin(
    const PartPluginConfig &config, const ResourceRegistry &registry
);

} // namespace flop::craft::plugin
