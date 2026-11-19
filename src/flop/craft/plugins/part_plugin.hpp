#pragma once

// runtime behavior object for one installed part plugin. each installed plugin
// becomes one polymorphic PartPlugin owned by its part; the craft ticks the
// enabled ones each simulation step. there is a single per-step cadence: tick.
// a render/per-frame cadence is not modeled here and would be introduced with
// the presentation layer when it exists.

namespace flop::craft {
class CraftContext;
} // namespace flop::craft

namespace flop::craft::plugin {

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

  [[nodiscard]] bool enabled() const noexcept { return enabled_; }
  void set_enabled(bool enabled) noexcept { enabled_ = enabled; }

protected:
  explicit PartPlugin(bool enabled) noexcept : enabled_(enabled) {}

private:
  bool enabled_;
};

} // namespace flop::craft::plugin
