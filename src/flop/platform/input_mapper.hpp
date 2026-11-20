#pragma once

#include "flop/platform/frame_input.hpp"
#include "flop/world/craft_id.hpp"

struct SDL_Window;
union SDL_Event;

namespace flop::platform {

class SDLPlatform;

// turn SDL events and keyboard state into a FrameInput. keeps track of key hold states. no SDL
// types are exposed through the resulting FrameInput
class InputMapper {
public:
  // the Craft that steering controls act on. the app keeps this in step with ControlManager's
  // active craft.
  void set_active_craft(world::CraftId craft) noexcept { active_craft_ = craft; }

  // indicate that a new frame ha sstarted. clear internal accounting of per-frame events
  void begin_frame() noexcept;

  // fold one SDL event into the in-progress frame.
  void handle_event(const SDL_Event &event);

  // gather all assembled input events. also samples held key states
  [[nodiscard]] FrameInput build_frame(const SDLPlatform &platform);

  // drop all held values
  void clear_held() noexcept;

private:
  // held keyboard throttle/steer intents, set on key down and cleared on key up.
  bool key_throttle_up_{false};
  bool key_throttle_down_{false};
  bool key_steer_ccw_{false};
  bool key_steer_cw_{false};

  // accumulated per-frame results.
  FrameInput frame_;
  world::CraftId active_craft_{world::CraftId::invalid};
};

} // namespace flop::platform
