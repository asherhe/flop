#include "flop/render/renderer.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include "flop/render/shader_library.hpp"
#include "flop/render/texture_cache.hpp"
#include "flop/view/frame.hpp"
#include "embedded.hpp"

namespace flop::render {
namespace {
std::runtime_error failure(const char *operation) {
  return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

SDL_GPUGraphicsPipeline *create_pipeline(SDL_GPUDevice *device, SDL_Window *window,
                                         const char *id, bool blend) {
  const auto formats = SDL_GetGPUShaderFormats(device);
  const bool spirv = (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0 &&
                     shaders::contains(shaders::manifest, id, "vertex", "spirv") &&
                     shaders::contains(shaders::manifest, id, "fragment", "spirv");
  const bool dxil = (formats & SDL_GPU_SHADERFORMAT_DXIL) != 0 &&
                    shaders::contains(shaders::manifest, id, "vertex", "dxil") &&
                    shaders::contains(shaders::manifest, id, "fragment", "dxil");
  if (!spirv && !dxil) {
    throw std::runtime_error(std::string(id) + ": no supported packaged variant (SPIR-V, DXIL)");
  }
  const auto format = spirv ? "spirv" : "dxil";
  const auto &vertex_entry = shaders::find(shaders::manifest, id, "vertex", format);
  const auto &fragment_entry = shaders::find(shaders::manifest, id, "fragment", format);
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
  if (!vertex) throw std::runtime_error(std::string(id) + " vertex shader: " + SDL_GetError());
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
  const auto error = pipeline ? std::runtime_error("") : std::runtime_error(std::string(id) + " pipeline: " + SDL_GetError());
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  if (!pipeline) throw error;
  return pipeline;
}
} // namespace

Renderer::Renderer(SDL_Window *window, const config::AssetResolver &resolver) : window_(window) {
  SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_INVALID;
  if (shaders::contains(shaders::manifest, "test_tri", "vertex", "spirv") &&
      shaders::contains(shaders::manifest, "test_tri", "fragment", "spirv")) formats |= SDL_GPU_SHADERFORMAT_SPIRV;
  if (shaders::contains(shaders::manifest, "test_tri", "vertex", "dxil") &&
      shaders::contains(shaders::manifest, "test_tri", "fragment", "dxil")) formats |= SDL_GPU_SHADERFORMAT_DXIL;
  if (formats == SDL_GPU_SHADERFORMAT_INVALID) {
    throw std::runtime_error("test_tri shader: no complete packaged shader variant");
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
    pipeline_ = create_pipeline(device_, window_, "test_tri", false);
    if (shaders::contains(shaders::manifest, "craft/sprite", "vertex", "spirv") ||
        shaders::contains(shaders::manifest, "craft/sprite", "vertex", "dxil"))
      craft_pipeline_ = create_pipeline(device_, window_, "craft/sprite", true);
    textures_ = std::make_unique<TextureCache>(device_, resolver);
  } catch (...) {
    if (craft_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, craft_pipeline_);
    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
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
    if (craft_pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, craft_pipeline_);
    SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
  }
}

void Renderer::present(const view::FrameDrawData &frame) {
  for (const auto &sprite : frame.sprites)
    (void)textures_->get_srgb(sprite.visual.sprite);
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
  if (swapchain) {
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{0.02f, 0.05f, 0.12f, 1.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (!pass) {
      const auto error = failure("SDL_BeginGPURenderPass");
      // SDL requires submission once a swapchain texture has been acquired.
      SDL_SubmitGPUCommandBuffer(commands);
      throw error;
    }
    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
  }
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    throw failure("SDL_SubmitGPUCommandBuffer");
  }
}
} // namespace flop::render
