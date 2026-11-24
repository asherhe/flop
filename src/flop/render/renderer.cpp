#include "flop/render/renderer.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include "embedded.hpp"

namespace flop::render {
namespace {
std::runtime_error failure(const char *operation) {
  return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}

SDL_GPUGraphicsPipeline *create_test_tri_pipeline(SDL_GPUDevice *device, SDL_Window *window) {
  const auto formats = SDL_GetGPUShaderFormats(device);
  const bool spirv = (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
  if (!spirv && !(formats & SDL_GPU_SHADERFORMAT_DXIL)) {
    throw std::runtime_error("test_tri shader: no supported packaged variant (SPIR-V, DXIL)");
  }
  SDL_GPUShaderCreateInfo info{};
  info.format = spirv ? SDL_GPU_SHADERFORMAT_SPIRV : SDL_GPU_SHADERFORMAT_DXIL;
  info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  info.entrypoint = "vertex_main";
  info.code = spirv ? shaders::vertex_spirv : shaders::vertex_dxil;
  info.code_size = spirv ? sizeof(shaders::vertex_spirv) : sizeof(shaders::vertex_dxil);
  SDL_GPUShader *vertex = SDL_CreateGPUShader(device, &info);
  if (!vertex) throw failure("create test_tri vertex shader");
  info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  info.entrypoint = "fragment_main";
  info.code = spirv ? shaders::fragment_spirv : shaders::fragment_dxil;
  info.code_size = spirv ? sizeof(shaders::fragment_spirv) : sizeof(shaders::fragment_dxil);
  SDL_GPUShader *fragment = SDL_CreateGPUShader(device, &info);
  if (!fragment) {
    const auto error = failure("create test_tri fragment shader");
    SDL_ReleaseGPUShader(device, vertex);
    throw error;
  }
  SDL_GPUColorTargetDescription target{};
  target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
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
  const auto error = pipeline ? std::runtime_error("") : failure("create test_tri pipeline");
  SDL_ReleaseGPUShader(device, fragment);
  SDL_ReleaseGPUShader(device, vertex);
  if (!pipeline) throw error;
  return pipeline;
}
} // namespace

Renderer::Renderer(SDL_Window *window) : window_(window) {
  SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;
#ifdef _WIN32
  formats |= SDL_GPU_SHADERFORMAT_DXIL;
#elif defined(__APPLE__)
  formats = SDL_GPU_SHADERFORMAT_METALLIB;
#endif
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
    pipeline_ = create_test_tri_pipeline(device_, window_);
  } catch (...) {
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
    throw;
  }
}

Renderer::~Renderer() {
  if (device_) {
    SDL_WaitForGPUIdle(device_);
    SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    SDL_ReleaseWindowFromGPUDevice(device_, window_);
    SDL_DestroyGPUDevice(device_);
  }
}

void Renderer::present(const view::FrameDrawData &frame) {
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
