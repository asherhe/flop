#pragma once

#include <utility>
#include <vector>

#include "flop/craft/resource_registry.hpp"
#include "flop/orbit/ref_frame.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::testing {

// minimal UniverseContext for tests that need a craft without a full Universe.
// owns a reference-frame manager and a resource registry and hands out
// read-only references to them, exactly as Universe does in production.
class StubUniverseContext : public world::UniverseContext {
public:
  explicit StubUniverseContext(
      orbit::RefFrameManager frames, std::vector<craft::ResourceConfig> resources = {}
  ) : frames_(std::move(frames)), resources_(std::move(resources)) {}

  [[nodiscard]] const craft::ResourceRegistry &resources() const override { return resources_; }
  [[nodiscard]] const orbit::RefFrameManager &reference_frames() const override { return frames_; }

private:
  orbit::RefFrameManager frames_;
  craft::ResourceRegistry resources_;
};

} // namespace flop::testing
