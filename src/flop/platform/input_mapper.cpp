#include "flop/platform/input_mapper.hpp"

#include <algorithm>

#include <SDL3/SDL.h>

#include "flop/platform/sdl_platform.hpp"
#include "flop/world/control_manager.hpp"

#include "SDL3/SDL_keycode.h"

namespace flop::platform {

void InputMapper::begin_frame() noexcept {
  frame_ = FrameInput{};
}

void InputMapper::clear_held() noexcept {
  key_throttle_up_ = false;
  key_throttle_down_ = false;
  key_steer_ccw_ = false;
  key_steer_cw_ = false;
}

void InputMapper::handle_event(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_QUIT:
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED: frame_.quit = true; break;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    frame_.focus_lost = true;
    clear_held();
    break;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_RESIZED: frame_.resized = true; break;
  case SDL_EVENT_KEY_DOWN:
    if (event.key.repeat) {
      break; // edges fire once; held keys are tracked by their down/up pair.
    }
    switch (event.key.key) {
    case SDLK_LSHIFT: key_throttle_up_ = true; break;
    case SDLK_LCTRL: key_throttle_down_ = true; break;
    case SDLK_A: key_steer_ccw_ = true; break;
    case SDLK_D: key_steer_cw_ = true; break;
    case SDLK_X: frame_.commands.emplace_back(world::ControlManager::SetThrottle{0.0}); break;
    case SDLK_Z: frame_.commands.emplace_back(world::ControlManager::SetThrottle{1.0}); break;
    case SDLK_COMMA: --frame_.scheduler.warp_steps; break;
    case SDLK_PERIOD: ++frame_.scheduler.warp_steps; break;
    case SDLK_SLASH: frame_.scheduler.warp_cancel = true; break;
    case SDLK_ESCAPE: frame_.scheduler.toggle_pause = true; break;
    case SDLK_O: ++frame_.scheduler.single_steps; break;
    default: break;
    }
    break;
  case SDL_EVENT_KEY_UP:
    switch (event.key.key) {
    case SDLK_LSHIFT: key_throttle_up_ = false; break;
    case SDLK_LCTRL: key_throttle_down_ = false; break;
    case SDLK_A: key_steer_ccw_ = false; break;
    case SDLK_D: key_steer_cw_ = false; break;
    default: break;
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    // left button held: drag pans. SDL reports a relative delta in logical px.
    if ((event.motion.state & SDL_BUTTON_LMASK) != 0u) {
      frame_.camera.pan_x += event.motion.xrel;
      frame_.camera.pan_y += event.motion.yrel;
    }
    break;
  case SDL_EVENT_MOUSE_WHEEL: frame_.camera.zoom += event.wheel.y; break;
  default: break;
  }
}

FrameInput InputMapper::build_frame(const SDLPlatform &platform) {
  // keyboard held intents into a pilot sample.
  double throttle_rate = 0.0;
  if (key_throttle_up_) {
    throttle_rate += 1.0;
  }
  if (key_throttle_down_) {
    throttle_rate -= 1.0;
  }
  throttle_rate = std::clamp(throttle_rate, -1.0, 1.0);

  double steering = 0.0;
  if (key_steer_ccw_) {
    steering += 1.0;
  }
  if (key_steer_cw_) {
    steering -= 1.0;
  }
  steering = std::clamp(steering, -1.0, 1.0);

  frame_.pilot = input::PilotActions{throttle_rate, steering};

  int pixel_width = 0;
  int pixel_height = 0;
  platform.drawable_size(pixel_width, pixel_height);
  frame_.pixel_width = pixel_width;
  frame_.pixel_height = pixel_height;

  return frame_;
}

} // namespace flop::platform
