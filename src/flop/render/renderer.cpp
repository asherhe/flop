#include "flop/render/renderer.hpp"

#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

namespace flop::render {
namespace {
std::runtime_error failure(const char *operation) {
  return std::runtime_error(std::string(operation) + ": " + SDL_GetError());
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
}

Renderer::~Renderer() {
  if (device_) {
    SDL_WaitForGPUIdle(device_);
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
      SDL_CancelGPUCommandBuffer(commands);
      throw error;
    }
    SDL_EndGPURenderPass(pass);
  }
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    throw failure("SDL_SubmitGPUCommandBuffer");
  }
}
} // namespace flop::render
