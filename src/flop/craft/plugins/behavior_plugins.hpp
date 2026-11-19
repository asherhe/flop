#pragma once

#include <utility>

#include "flop/craft/parts/part_plugin_config.hpp"
#include "flop/craft/plugins/part_plugin.hpp"

namespace flop::craft {

// stub behavior plugins. each is constructed from its configuration and holds
// it for the behavior that lands in a later stage; tick is a no-op for now.

class CommandPlugin : public PartPlugin {
public:
  CommandPlugin(CommandConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const CommandConfig &config() const noexcept { return config_; }

private:
  CommandConfig config_;
};

class EnginePlugin : public PartPlugin {
public:
  EnginePlugin(EngineConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const EngineConfig &config() const noexcept { return config_; }

private:
  EngineConfig config_;
};

class ReactionWheelPlugin : public PartPlugin {
public:
  ReactionWheelPlugin(ReactionWheelConfig config, bool enabled) :
      PartPlugin(enabled), config_(config) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const ReactionWheelConfig &config() const noexcept { return config_; }

private:
  ReactionWheelConfig config_;
};

class SpritePlugin : public PartPlugin {
public:
  SpritePlugin(SpriteRendererConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const SpriteRendererConfig &config() const noexcept { return config_; }

private:
  SpriteRendererConfig config_;
};

class LightPlugin : public PartPlugin {
public:
  LightPlugin(LightConfig config, bool enabled) : PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const LightConfig &config() const noexcept { return config_; }

private:
  LightConfig config_;
};

} // namespace flop::craft
