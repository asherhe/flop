#pragma once

// runtime behavior object for one installed part plugin. each installed plugin
// becomes one polymorphic PartPlugin owned by its part; the craft ticks the
// enabled ones each simulation step. there is a single per-step cadence: tick.
// a render/per-frame cadence is not modeled here and would be introduced with
// the presentation layer when it exists.

#include <cstddef>
#include <cstdint>

namespace flop::craft {
class CraftContext;
} // namespace flop::craft

namespace flop::craft::plugin {

// when during a step a plugin ticks. control plugins produce the shared control
// state (throttle/steering); actuation plugins consume it to apply forces and
// torques. the craft runs all control-phase plugins before any actuation-phase
// plugin so a reader never observes stale control within a step.
enum class TickPhase : std::uint8_t { control, actuation };

class PartPlugin {
public:
  PartPlugin(const PartPlugin &) = delete;
  PartPlugin(PartPlugin &&) = delete;
  PartPlugin &operator=(const PartPlugin &) = delete;
  PartPlugin &operator=(PartPlugin &&) = delete;
  virtual ~PartPlugin() = default;

  // advance this plugin by one simulation step. only called for enabled
  // plugins. the plugin reads and mutates craft state through the context.
  virtual void tick(CraftContext &ctx, double dt_sec) = 0;

  // the phase in which this plugin ticks. actuation by default; a plugin that
  // writes control state overrides this to run in the control phase.
  [[nodiscard]] virtual TickPhase tick_phase() const noexcept { return TickPhase::actuation; }

  // index of the part that owns this plugin within its craft's part list, set
  // when the owning part installs its plugins. a plugin uses it to address its
  // own part for resource draws and force application.
  [[nodiscard]] std::size_t part_index() const noexcept { return part_index_; }
  void set_part_index(std::size_t part_index) noexcept { part_index_ = part_index; }

  [[nodiscard]] bool enabled() const noexcept { return enabled_; }
  void set_enabled(bool enabled) noexcept { enabled_ = enabled; }

protected:
  explicit PartPlugin(bool enabled) noexcept : enabled_(enabled) {}

private:
  bool enabled_;
  std::size_t part_index_{};
};

} // namespace flop::craft::plugin
