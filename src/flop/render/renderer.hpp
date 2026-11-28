#pragma once

#include <memory>
#include <array>
#ifdef FLOP_RENDER_TESTING
#include <cstdint>
#endif
#include "flop/render/material_recipe.hpp"
#include "flop/config/asset_path.hpp"

struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
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
  std::array<std::uint16_t, 4> readback_normal(const view::FrameDrawData &frame);
  std::array<std::uint16_t, 4> readback_scene(const view::FrameDrawData &frame);
  std::array<std::uint16_t, 4> readback_sanitized(const view::FrameDrawData &frame);
#endif

private:
  SDL_Window *window_{};
  SDL_GPUDevice *device_{};
  SDL_GPUGraphicsPipeline *craft_pipeline_{};
  SDL_GPUGraphicsPipeline *lit_pipeline_{};
  SDL_GPUGraphicsPipeline *particle_pipeline_{};
  SDL_GPUGraphicsPipeline *point_light_pipeline_{};
  SDL_GPUGraphicsPipeline *normal_pipeline_{};
  SDL_GPUGraphicsPipeline *post_composite_pipeline_{};
  SDL_GPUGraphicsPipeline *sanitize_pipeline_{};
  SDL_GPUGraphicsPipeline *bloom_pipeline_{};
  SDL_GPUGraphicsPipeline *smaa_edge_pipeline_{};
  SDL_GPUGraphicsPipeline *smaa_weights_pipeline_{};
  SDL_GPUGraphicsPipeline *smaa_blend_pipeline_{};
  SDL_GPUTexture *light_texture_{};
  SDL_GPUTexture *normal_texture_{};
  SDL_GPUTexture *scene_texture_{};
  SDL_GPUTexture *sanitized_texture_{};
  SDL_GPUTexture *smaa_edges_{};
  SDL_GPUTexture *smaa_weights_{};
  SDL_GPUTexture *smaa_output_{};
  std::array<SDL_GPUTexture *, 6> bloom_down_{};
  std::array<SDL_GPUTexture *, 6> bloom_up_{};
  unsigned bloom_count_{};
  unsigned light_width_{}, light_height_{};
  SDL_GPUSampler *sprite_sampler_{};
  SDL_GPUSampler *post_sampler_{};
  CraftSpriteBindings craft_bindings_{};
  CraftLitSpriteBindings lit_bindings_{};
  config::AssetResolver resolver_;
  std::unique_ptr<TextureCache> textures_;
#ifdef FLOP_RENDER_TESTING
  bool capture_{};
  bool capture_normal_{};
  bool capture_scene_{};
  bool capture_sanitized_{};
  std::array<std::uint8_t, 4> captured_{};
  std::array<std::uint16_t, 4> captured_normal_{};
#endif
};

} // namespace flop::render
