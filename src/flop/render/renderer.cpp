#include "flop/render/renderer.hpp"

#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "flop/render/material_recipe.hpp"
#include "flop/render/shader_library.hpp"
#include "flop/render/texture_cache.hpp"
#include "flop/view/frame.hpp"

#include "embedded.hpp"

namespace flop::render {
namespace {
struct alignas(16) SpriteGeometry {
  float origin_x, origin_y, x_axis_x, x_axis_y;
  float y_axis_x, y_axis_y, viewport_width, viewport_height;
};
static_assert(sizeof(SpriteGeometry) == 8 * sizeof(float));
static_assert(offsetof(SpriteGeometry, y_axis_x) == 4 * sizeof(float));

std::runtime_error failure(const char *operation) {
  return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

SDL_GPUGraphicsPipeline *create_pipeline(
    SDL_GPUDevice *device, SDL_Window *window, const char *id, bool blend,
    CraftSpriteBindings *bindings = nullptr, const MaterialRecipe *recipe = nullptr,
    CraftLitSpriteBindings *lit_bindings = nullptr
) {
  const auto formats = SDL_GetGPUShaderFormats(device);
  const bool spirv = (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0 &&
                     shaders::contains(shaders::manifest, id, "vertex", "spirv") &&
                     shaders::contains(shaders::manifest, id, "fragment", "spirv");
  const bool dxil = (formats & SDL_GPU_SHADERFORMAT_DXIL) != 0 &&
                    shaders::contains(shaders::manifest, id, "vertex", "dxil") &&
                    shaders::contains(shaders::manifest, id, "fragment", "dxil");
  if (!spirv && !dxil) {
    throw std::runtime_error(std::string(id) + ": no complete packaged shader variant supported by the GPU device; rebuild with a supported shader format");
  }
  const auto format = spirv ? "spirv" : "dxil";
    const auto &vertex_entry = shaders::find(shaders::manifest, id, "vertex", format);
  const auto &fragment_entry = shaders::find(shaders::manifest, id, "fragment", format);
  if (bindings)
    *bindings = resolve_craft_sprite_bindings(*recipe, vertex_entry, fragment_entry);
  if (lit_bindings)
    *lit_bindings = resolve_craft_lit_sprite_bindings(*recipe, vertex_entry, fragment_entry);
  SDL_GPUShaderCreateInfo info{};
  info.format = spirv ? SDL_GPU_SHADERFORMAT_SPIRV : SDL_GPU_SHADERFORMAT_DXIL;
  info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  info.entrypoint = vertex_entry.entrypoint.data();
  info.code = vertex_entry.code;
  info.code_size = vertex_entry.size;
  info.num_samplers = vertex_entry.samplers;
  info.num_storage_textures = vertex_entry.storage_textures;
  info.num_storage_buffers = vertex_entry.storage_buffers;
  info.num_uniform_buffers = vertex_entry.uniform_buffers;
  SDL_GPUShader *vertex = SDL_CreateGPUShader(device, &info);
  if (!vertex)
    throw std::runtime_error(std::string(id) + " vertex shader: " + SDL_GetError());
  info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.entrypoint = fragment_entry.entrypoint.data();
  info.code = fragment_entry.code;
  info.code_size = fragment_entry.size;
  info.num_samplers = fragment_entry.samplers;
  info.num_storage_textures = fragment_entry.storage_textures;
  info.num_storage_buffers = fragment_entry.storage_buffers;
  info.num_uniform_buffers = fragment_entry.uniform_buffers;
  SDL_GPUShader *fragment = SDL_CreateGPUShader(device, &info);
  if (!fragment) {
    const auto error = std::runtime_error(std::string(id) + " fragment shader: " + SDL_GetError());
    SDL_ReleaseGPUShader(device, vertex);
    throw error;
  }
  SDL_GPUColorTargetDescription target{};
  target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
  if (blend) {
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  }
  SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.vertex_shader = vertex;
  pipeline_info.fragment_shader = fragment;
  pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  pipeline_info.target_info.color_target_descriptions = &target;
  pipeline_info.target_info.num_color_targets = 1;
  SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
  const auto error = pipeline
                         ? std::runtime_error("")
                         : std::runtime_error(std::string(id) + " pipeline: " + SDL_GetError());
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  if (!pipeline)
    throw error;
  return pipeline;
}
} // namespace

Renderer::Renderer(SDL_Window *window, const config::AssetResolver &resolver) : window_(window), resolver_(resolver) {
  SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_INVALID;
  if (shaders::contains(shaders::manifest, "test_tri", "vertex", "spirv") &&
      shaders::contains(shaders::manifest, "test_tri", "fragment", "spirv") &&
      shaders::contains(shaders::manifest, craft_sprite_recipe.shader, "vertex", "spirv") &&
      shaders::contains(shaders::manifest, craft_sprite_recipe.shader, "fragment", "spirv"))
    formats |= SDL_GPU_SHADERFORMAT_SPIRV;
  if (shaders::contains(shaders::manifest, "test_tri", "vertex", "dxil") &&
      shaders::contains(shaders::manifest, "test_tri", "fragment", "dxil") &&
      shaders::contains(shaders::manifest, craft_sprite_recipe.shader, "vertex", "dxil") &&
      shaders::contains(shaders::manifest, craft_sprite_recipe.shader, "fragment", "dxil"))
    formats |= SDL_GPU_SHADERFORMAT_DXIL;
  if (formats == SDL_GPU_SHADERFORMAT_INVALID) {
    throw std::runtime_error("test_tri and craft/sprite: no common complete packaged shader format; rebuild with shader reflection enabled");
  }
  device_ = SDL_CreateGPUDevice(formats, true, nullptr);
  if (!device_) {
    throw failure("SDL_CreateGPUDevice");
  }
  if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
    const auto error = failure("SDL_ClaimWindowForGPUDevice");
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    throw error;
  }
  SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device_));
  try {
    // Sampled sRGB textures yield linear RGB. An sRGB swapchain encodes the
    // blended linear result for display; SDL's default SDR swapchain does not.
    if (!SDL_SetGPUSwapchainParameters(
            device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR, SDL_GPU_PRESENTMODE_VSYNC
        )) {
      throw failure("SDL_SetGPUSwapchainParameters (linear SDR)");
    }
    pipeline_ = create_pipeline(device_, window_, "test_tri", false);
    particle_pipeline_ = create_pipeline(device_, window_, "particle", true);
    {
      const auto loaded_recipe = load_craft_sprite_recipe(resolver);
      if (loaded_recipe.shader != craft_sprite_recipe.shader)
        throw std::runtime_error("assets/shared/shaders/craft_sprite.yaml: unexpected shader ID");
      const auto recipe = loaded_recipe.view();
      try {
        craft_pipeline_ = create_pipeline(device_, window_, loaded_recipe.shader.c_str(), true, &craft_bindings_, &recipe);
      } catch (const std::exception &error) {
        throw std::runtime_error(std::string("assets/shared/shaders/craft_sprite.yaml: ") + error.what());
      }
      SDL_GPUSamplerCreateInfo sampler{};
      sampler.min_filter = SDL_GPU_FILTER_NEAREST;
      sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
      sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
      sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
      sprite_sampler_ = SDL_CreateGPUSampler(device_, &sampler);
      if (!sprite_sampler_)
        throw failure("SDL_CreateGPUSampler");
    }
    textures_ = std::make_unique<TextureCache>(device_, resolver);
  } catch (...) {
    if (sprite_sampler_)
      SDL_ReleaseGPUSampler(device_, sprite_sampler_);
    if (craft_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, craft_pipeline_);
    if (particle_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, particle_pipeline_);
    if (lit_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, lit_pipeline_);
    if (pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    throw;
  }
}

Renderer::~Renderer() {
  if (device_) {
    SDL_WaitForGPUIdle(device_);
    textures_.reset();
    if (sprite_sampler_)
      SDL_ReleaseGPUSampler(device_, sprite_sampler_);
    if (craft_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, craft_pipeline_);
    if (particle_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, particle_pipeline_);
    if (lit_pipeline_)
      SDL_ReleaseGPUGraphicsPipeline(device_, lit_pipeline_);
    SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
  }
}

void Renderer::present(const view::FrameDrawData &frame) {
  struct SpriteTextures { SDL_GPUTexture *albedo{}, *normal{}, *emission{}; bool lit{}; };
  std::vector<SpriteTextures> sprite_textures;
  sprite_textures.reserve(frame.sprites.size());
  for (const auto &sprite : frame.sprites) {
    const auto &material = sprite.visual.material;
    const bool lit = material.shader == "craft/lit_sprite";
    if (!lit && material.shader != craft_sprite_recipe.shader)
      throw std::runtime_error("unsupported sprite shader '" + sprite.visual.material.shader + "'");
    if (lit && !lit_pipeline_) {
      const auto loaded = load_craft_lit_sprite_recipe(resolver_);
      const auto recipe = loaded.view();
      lit_pipeline_ = create_pipeline(device_, window_, "craft/lit_sprite", true, nullptr, &recipe, &lit_bindings_);
    }
    SpriteTextures maps{};
    maps.lit = lit;
    try {
      maps.albedo = textures_->get(sprite.visual.sprite, TextureColorSpace::srgb);
    } catch (const std::exception &error) {
      const auto &source = sprite.visual.sprite_source;
      throw std::runtime_error(source.address + ": " + source.key + ": " + error.what());
    }
    if (lit) {
      const auto load_map = [&](const std::string &path, const auto &source, TextureColorSpace space, DefaultTexture fallback) {
        try { return path.empty() ? textures_->get_default(fallback) : textures_->get(path, space); }
        catch (const std::exception &error) {
          throw std::runtime_error(source.address + ": " + source.key + ": " + error.what());
        }
      };
      maps.normal = load_map(material.normal_map, material.normal_map_source, TextureColorSpace::linear, DefaultTexture::flat_normal);
      maps.emission = load_map(material.emission_map, material.emission_map_source, TextureColorSpace::srgb, DefaultTexture::transparent);
    }
    sprite_textures.push_back(maps);
  }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device_);
  if (!commands) {
    throw failure("SDL_AcquireGPUCommandBuffer");
  }
  SDL_GPUTexture *swapchain = nullptr;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &swapchain, nullptr, nullptr)) {
    const auto error = failure("SDL_WaitAndAcquireGPUSwapchainTexture");
    SDL_CancelGPUCommandBuffer(commands);
    throw error;
  }
#ifdef FLOP_RENDER_TESTING
  SDL_GPUTexture *capture_texture = nullptr;
  SDL_GPUTransferBuffer *download = nullptr;
  if (capture_) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = info.height = info.layer_count_or_depth = info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    capture_texture = SDL_CreateGPUTexture(device_, &info);
    if (!capture_texture) { SDL_SubmitGPUCommandBuffer(commands); throw failure("create capture texture"); }
    SDL_GPUTransferBufferCreateInfo transfer{};
    transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer.size = 4;
    download = SDL_CreateGPUTransferBuffer(device_, &transfer);
    if (!download) { SDL_ReleaseGPUTexture(device_, capture_texture); SDL_SubmitGPUCommandBuffer(commands); throw failure("create capture buffer"); }
    swapchain = capture_texture;
  }
#endif
  if (swapchain) {
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (!pass) {
      const auto error = failure("SDL_BeginGPURenderPass");
      // SDL requires submission once a swapchain texture has been acquired.
      SDL_SubmitGPUCommandBuffer(commands);
      throw error;
    }
    if (craft_pipeline_ && frame.camera.viewport_width > 0 && frame.camera.viewport_height > 0) {
      SDL_BindGPUGraphicsPipeline(pass, particle_pipeline_);
      for (const auto &particle : frame.particles) {
        struct ParticleGeometry { std::array<float, 4> origin_and_x_axis, y_axis_and_viewport, color; };
        const float size = particle.size_pixels;
        const float c = std::cos(particle.angle_rad) * size;
        const float s = std::sin(particle.angle_rad) * size;
        const ParticleGeometry geometry{
            {particle.center_pixels.x - 0.5f * (c + s), particle.center_pixels.y - 0.5f * (c - s), c, -s},
            {s, c, static_cast<float>(frame.camera.viewport_width), static_cast<float>(frame.camera.viewport_height)},
            particle.color
        };
        SDL_PushGPUVertexUniformData(commands, 0, &geometry, sizeof(geometry));
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
      }
      for (std::size_t i = 0; i < frame.sprites.size(); ++i) {
        const auto &maps = sprite_textures[i];
        SDL_BindGPUGraphicsPipeline(pass, maps.lit ? lit_pipeline_ : craft_pipeline_);
        const auto &corners = frame.sprites[i].corners_pixels;
        const SpriteGeometry geometry{
            corners[0].x,
            corners[0].y,
            corners[1].x - corners[0].x,
            corners[1].y - corners[0].y,
            corners[3].x - corners[0].x,
            corners[3].y - corners[0].y,
            static_cast<float>(frame.camera.viewport_width),
            static_cast<float>(frame.camera.viewport_height)
        };
        const auto &bindings = maps.lit ? lit_bindings_.geometry : craft_bindings_;
        SDL_PushGPUVertexUniformData(commands, bindings.uniform_slot, &geometry, sizeof(geometry));
        SDL_GPUTextureSamplerBinding binding{maps.albedo, sprite_sampler_};
        SDL_BindGPUFragmentSamplers(pass, bindings.texture_slot, &binding, 1);
        if (maps.lit) {
          SDL_GPUTextureSamplerBinding normal{maps.normal, sprite_sampler_};
          SDL_GPUTextureSamplerBinding emission{maps.emission, sprite_sampler_};
          SDL_BindGPUFragmentSamplers(pass, lit_bindings_.normal_slot, &normal, 1);
          SDL_BindGPUFragmentSamplers(pass, lit_bindings_.emission_slot, &emission, 1);
          const auto &sun = frame.sprites[i].sun;
          struct Lighting { std::array<float, 4> direction, color, shine_direction, shine_color; };
          const auto &shine = frame.sprites[i].planet_shine;
          const Lighting light{sun.direction, sun.color, shine.direction_and_spread, shine.color};
          SDL_PushGPUFragmentUniformData(commands, lit_bindings_.light_uniform_slot, &light, sizeof(light));
        }
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
      }
    } else if (!craft_pipeline_) {
      SDL_BindGPUGraphicsPipeline(pass, pipeline_);
      SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
  }
#ifdef FLOP_RENDER_TESTING
  if (capture_) {
    auto *copy = SDL_BeginGPUCopyPass(commands);
    if (!copy) throw failure("begin capture copy");
    SDL_GPUTextureRegion region{};
    region.texture = capture_texture;
    region.w = region.h = region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download;
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (!fence || !SDL_WaitForGPUFences(device_, true, &fence, 1)) throw failure("capture submission");
    const auto *pixels = static_cast<const std::uint8_t *>(SDL_MapGPUTransferBuffer(device_, download, false));
    if (!pixels) throw failure("map capture");
    const auto format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    const bool bgra = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    captured_ = {pixels[bgra ? 2 : 0], pixels[1], pixels[bgra ? 0 : 2], pixels[3]};
    SDL_UnmapGPUTransferBuffer(device_, download);
    SDL_ReleaseGPUFence(device_, fence);
    SDL_ReleaseGPUTransferBuffer(device_, download);
    SDL_ReleaseGPUTexture(device_, capture_texture);
    return;
  }
#endif
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    throw failure("SDL_SubmitGPUCommandBuffer");
  }
}
#ifdef FLOP_RENDER_TESTING
std::array<std::uint8_t, 4> Renderer::readback_pixel(const view::FrameDrawData &frame) {
  capture_ = true;
  try { present(frame); } catch (...) { capture_ = false; throw; }
  capture_ = false;
  return captured_;
}
#endif
} // namespace flop::render
