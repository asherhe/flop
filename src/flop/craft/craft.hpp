#pragma once

#include <string>
#include <string_view>

#include "flop/craft/craft_config.hpp"
#include "flop/orbit/prediction/patched_conics.hpp"
#include "flop/orbit/ref_frame.hpp"

namespace flop::craft {

// owns one craft's orbit and prediction. parts and flight behavior follow later.
class Craft {
public:
  Craft(CraftConfig config, const orbit::RefFrameManager &frames);

  [[nodiscard]] std::string_view name() const noexcept { return name_; }
  [[nodiscard]] const orbit::OrbitState &orbit_state() const noexcept { return orbit_state_; }
  [[nodiscard]] const orbit::OrbitPatchSegments &patches() const noexcept { return patches_; }

  // replace the owned prediction only after the requested interval is covered.
  void predict_patches(const orbit::RefFrameManager &frames, double begin_ut, double end_ut);

private:
  std::string name_;
  orbit::OrbitState orbit_state_;
  orbit::OrbitPatchSegments patches_;
};

} // namespace flop::craft
