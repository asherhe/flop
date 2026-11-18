#pragma once

// self-contained accessor interface for the shared managers a craft needs from
// its world. kept free of includes (only forward declarations) so that lower
// layers such as flop::craft can depend on it without pulling in flop::world,
// which would otherwise be a dependency cycle. flop::world::Universe implements
// this interface; callers that use the returned references include the concrete
// manager headers themselves.

namespace flop::orbit {
class RefFrameManager;
}

namespace flop::craft {
class ResourceRegistry;
}

namespace flop::world {

class UniverseContext {
public:
  UniverseContext() = default;
  UniverseContext(const UniverseContext &) = default;
  UniverseContext(UniverseContext &&) = default;
  UniverseContext &operator=(const UniverseContext &) = default;
  UniverseContext &operator=(UniverseContext &&) = default;
  virtual ~UniverseContext() = default;

  [[nodiscard]] virtual const craft::ResourceRegistry &resources() const = 0;
  [[nodiscard]] virtual const orbit::RefFrameManager &reference_frames() const = 0;
};

} // namespace flop::world
