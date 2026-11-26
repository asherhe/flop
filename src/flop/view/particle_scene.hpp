#pragma once

#include <optional>
#include <vector>

#include "flop/view/frame.hpp"
#include "flop/view/particles.hpp"
#include "flop/world/plugin_target.hpp"

namespace flop::view {

// bridges world ticks and transient presentation particles.
class ParticleScene {
public:
  void begin_step(const world::Universe &universe, const Focus &focus);
  void end_step(const world::Universe &universe, const Focus &focus, double dt_sec);
  void set_focus(const Focus &focus);
  void append_draw_data(FrameDrawData &frame) const;
  [[nodiscard]] const ParticleSystem &system() const { return particles_; }

private:
  struct Entry {
    world::PluginTarget target;
    ParticleEmitter emitter;
  };
  ParticleSystem particles_;
  std::optional<Focus> focus_;
  Vec2d focus_before_{};
  std::vector<Entry> emitters_;
};

} // namespace flop::view
