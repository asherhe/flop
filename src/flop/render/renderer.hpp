#pragma once

#include <memory>
#ifdef FLOP_RENDER_TESTING
#include <array>
#include <cstdint>
#endif
#include "flop/render/material_recipe.hpp"

struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_Window;

namespace flop::view { struct FrameDrawData; }
namespace flop::config { class AssetResolver; }

namespace flop::render {
class TextureCache;

// Owns the GPU device and its claim on the platform window. The platform must
// outlive this object. Construction and presentation report failures by exception.
class Renderer {
public:
  Renderer(SDL_Window *window, const config::AssetResolver &resolver);
  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;
  ~Renderer();

  void present(const view::FrameDrawData &frame);
#ifdef FLOP_RENDER_TESTING
  std::array<std::uint8_t, 4> readback_pixel(const view::FrameDrawData &frame);
#endif

private:
  SDL_Window *window_{};
  SDL_GPUDevice *device_{};
  SDL_GPUGraphicsPipeline *pipeline_{};
  SDL_GPUGraphicsPipeline *craft_pipeline_{};
  SDL_GPUSampler *sprite_sampler_{};
  CraftSpriteBindings craft_bindings_{};
  std::unique_ptr<TextureCache> textures_;
#ifdef FLOP_RENDER_TESTING
  bool capture_{};
  std::array<std::uint8_t, 4> captured_{};
#endif
};

} // namespace flop::render
