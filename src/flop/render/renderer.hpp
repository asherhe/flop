#pragma once

struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_Window;

namespace flop::view { struct FrameDrawData; }

namespace flop::render {

// Owns the GPU device and its claim on the platform window. The platform must
// outlive this object. Construction and presentation report failures by exception.
class Renderer {
public:
  explicit Renderer(SDL_Window *window);
  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;
  ~Renderer();

  void present(const view::FrameDrawData &frame);

private:
  SDL_Window *window_{};
  SDL_GPUDevice *device_{};
  SDL_GPUGraphicsPipeline *pipeline_{};
  SDL_GPUGraphicsPipeline *craft_pipeline_{};
};

} // namespace flop::render
