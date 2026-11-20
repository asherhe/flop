#include "flop/platform/sdl_platform.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

namespace flop::platform {

SDLPlatform::SDLPlatform(const char *title, int logical_width, int logical_height) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
  }
  window_ = SDL_CreateWindow(
      title, logical_width, logical_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
  );
  if (window_ == nullptr) {
    const std::string error = SDL_GetError();
    SDL_Quit();
    throw std::runtime_error("SDL_CreateWindow failed: " + error);
  }
}

SDLPlatform::~SDLPlatform() {
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

bool SDLPlatform::poll_event(SDL_Event &event) {
  return SDL_PollEvent(&event);
}

void SDLPlatform::drawable_size(int &width, int &height) const {
  SDL_GetWindowSizeInPixels(window_, &width, &height);
}

void SDLPlatform::logical_size(int &width, int &height) const {
  SDL_GetWindowSize(window_, &width, &height);
}

} // namespace flop::platform
