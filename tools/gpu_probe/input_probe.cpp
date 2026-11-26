#include <iostream>
#include <stdexcept>

#include <SDL3/SDL.h>

#include "flop/platform/input_mapper.hpp"
#include "flop/platform/sdl_platform.hpp"

int main() {
  try {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    flop::platform::SDLPlatform platform("input probe", 64, 64);
    flop::platform::InputMapper mapper;
    const auto key = [&](SDL_EventType type, SDL_Keycode code) {
      SDL_Event event{};
      event.type = type;
      event.key.key = code;
      mapper.handle_event(event);
    };
    const auto expect = [&](double value) {
      if (mapper.build_frame(platform).pilot.throttle_rate != value)
        throw std::runtime_error("unexpected throttle rate");
    };
    expect(0);
    key(SDL_EVENT_KEY_DOWN, SDLK_LSHIFT);
    expect(1);
    key(SDL_EVENT_KEY_DOWN, SDLK_LCTRL);
    expect(0);
    key(SDL_EVENT_KEY_UP, SDLK_LSHIFT);
    expect(-1);
    key(SDL_EVENT_KEY_UP, SDLK_LCTRL);
    expect(0);
    key(SDL_EVENT_KEY_DOWN, SDLK_LCTRL);
    expect(-1);
    SDL_Event focus_lost{};
    focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    mapper.handle_event(focus_lost);
    expect(0);
    std::cout << "throttle key press, release, and focus loss: passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
