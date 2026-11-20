#pragma once

#include "flop/craft/ids.hpp"
#include "flop/world/craft_id.hpp"

namespace flop::world {

// identifies one plugin instance anywhere in the world by the triple of craft,
// part, and plugin ids. a queued action (e.g. enable/disable a plugin) carries
// a PluginTarget and resolves it against live storage at execution, so a target
// whose craft, part, or plugin no longer exists fails cleanly instead of
// aliasing another instance.
struct PluginTarget {
  CraftId craft;
  craft::PartId part;
  craft::PluginId plugin;

  friend bool operator==(const PluginTarget &, const PluginTarget &) = default;
};

} // namespace flop::world
