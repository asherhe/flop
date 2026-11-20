#pragma once

struct SDL_Window;
union SDL_Event;

namespace flop::platform {

// owns the SDL subsystems and the window for the app.
// Created once at startup and destroyed last (after the renderer, so the GPU
// device and window claim stay valid during teardown). Exposes the window,
// one-at-a-time event polling, and the drawable (pixel) and logical sizes the
// renderer and input mapper need. Construction throws std::runtime_error on an
// SDL init or window failure.
class SDLPlatform {
public:
  SDLPlatform(const char *title, int logical_width, int logical_height);
  SDLPlatform(const SDLPlatform &) = delete;
  SDLPlatform &operator=(const SDLPlatform &) = delete;
  ~SDLPlatform();

  [[nodiscard]] SDL_Window *window() const noexcept { return window_; }

  // poll the next pending event, or return false when the queue is empty.
  bool poll_event(SDL_Event &event);

  // the window's drawable size in pixels (may exceed logical size on high DPI).
  void drawable_size(int &width, int &height) const;
  // the window's logical size, the coordinate space SDL reports input in.
  void logical_size(int &width, int &height) const;

private:
  SDL_Window *window_{nullptr};
};

} // namespace flop::platform
