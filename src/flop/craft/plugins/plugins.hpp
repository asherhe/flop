#pragma once

#include <utility>

#include "flop/craft/control.hpp"
#include "flop/craft/craft_context.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/plugins/engine.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/config.hpp"

namespace flop::craft::plugin {

// the command pod. the universe writes the pilot's throttle and steering into the craft's control
// state before each step, and the pod's tick is the hook where later control logic (autosteer)
// will override steering. autosteer is not modeled yet. a single command pod drives the craft;
// arbitration between multiple pods is a later addition.
class CommandPlugin : public PartPlugin {
public:
  CommandPlugin(CommandConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] TickPhase tick_phase() const noexcept override { return TickPhase::control; }

  [[nodiscard]] const CommandConfig &config() const noexcept { return config_; }

private:
  CommandConfig config_;
};

// converts steering commands into torque about the craft's center of mass. it
// reads the shared steering level and applies its rated torque scaled by that
// level, so a steering command of magnitude one applies full rated torque.
class ReactionWheelPlugin : public PartPlugin {
public:
  ReactionWheelPlugin(ReactionWheelConfig config, bool enabled) :
      PartPlugin(enabled), config_(config) {}

  void tick(CraftContext &ctx, double /*dt_sec*/) override {
    const double steering = ctx.control().steering();
    if (steering != 0.0) {
      ctx.newtonian().apply_torque(config_.torque_newton_meters * steering);
    }
  }

  [[nodiscard]] const ReactionWheelConfig &config() const noexcept { return config_; }

private:
  ReactionWheelConfig config_;
};

// render-only in the full game; a no-op in the headless simulation.
class SpritePlugin : public PartPlugin {
public:
  SpritePlugin(SpriteConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const SpriteConfig &config() const noexcept { return config_; }

private:
  SpriteConfig config_;
};

// render-only in the full game; a no-op in the headless simulation.
class LightPlugin : public PartPlugin {
public:
  LightPlugin(LightConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)),
      strength_(enabled ? config_.strength : 0.0) {}

  void tick(CraftContext & /*ctx*/, double /*dt_sec*/) override {}

  [[nodiscard]] const LightConfig &config() const noexcept { return config_; }
  void set_strength(double value) noexcept { strength_ = value; }
  [[nodiscard]] double strength() const noexcept { return strength_; }

private:
  void on_enabled() noexcept override { strength_ = config_.strength; }
  void on_disabled() noexcept override { strength_ = 0.0; }
  LightConfig config_;
  double strength_;
};

// emitter configuration and the command produced during the current step.
class ParticlePlugin : public PartPlugin {
public:
  ParticlePlugin(ParticleConfig config, bool enabled) :
      PartPlugin(enabled), config_(std::move(config)) {}

  void tick(CraftContext &, double) override {}
  void reset_step() override { emission_strength_ = 0.0; }
  void set_emission_strength(double value) noexcept { emission_strength_ = value; }
  [[nodiscard]] double emission_strength() const noexcept { return emission_strength_; }
  [[nodiscard]] const ParticleConfig &config() const noexcept { return config_; }

private:
  void on_disabled() noexcept override { emission_strength_ = 0.0; }
  ParticleConfig config_;
  double emission_strength_{};
};

} // namespace flop::craft::plugin
