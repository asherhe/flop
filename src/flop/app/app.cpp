#include "flop/app/app.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <string_view>

#include <SDL3/SDL.h>

#include "flop/app/game_scene.hpp"
#include "flop/app/tick_scheduler.hpp"
#include "flop/config/asset_path.hpp"
#include "flop/craft/craft.hpp"
#include "flop/input/pilot_control.hpp"
#include "flop/platform/frame_input.hpp"
#include "flop/platform/input_mapper.hpp"
#include "flop/platform/sdl_platform.hpp"
#include "flop/world/control_manager.hpp"
#include "flop/world/universe.hpp"

namespace flop::app {

namespace {

namespace fs = std::filesystem;

// pick assets folder root
// use an explicit --assets <path> cli flag, if given. otherwise, use the assets folder beside the
// executable. returns nullopt when --assets is present but not given a value
std::optional<config::AssetResolver> resolve_assets(int argc, char **argv) {
  std::optional<fs::path> explicit_root;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--assets") {
      if (i + 1 >= argc) {
        return std::nullopt;
      }
      explicit_root = fs::path(argv[++i]);
    }
  }
  fs::path base;
  if (const char *base_path = SDL_GetBasePath()) {
    base = fs::path(base_path);
  } else {
    base = fs::current_path();
  }
  return config::AssetResolver::from_startup(base, explicit_root);
}

} // namespace

int App::run(int argc, char **argv) {
  const auto resolver = resolve_assets(argc, argv);
  if (!resolver) {
    std::fprintf(stderr, "flop: --assets requires a path argument\n");
    return 2;
  }

  GameScene scene;
  try {
    scene = load_game_scene(*resolver);
  } catch (const std::exception &error) {
    std::fprintf(stderr, "flop: failed to load scene: %s\n", error.what());
    return 1;
  }
  world::Universe &universe = *scene.universe;

  // select active craft, using first defined craft
  if (!scene.craft.empty()) {
    universe.controls().submit(world::ControlManager::SelectCraft{scene.craft.front()});
    (void)universe.apply_commands();
  }

  std::optional<platform::SDLPlatform> platform;
  try {
    platform.emplace("flop", 1280, 720);
  } catch (const std::exception &error) {
    std::fprintf(stderr, "flop: %s\n", error.what());
    return 1;
  }

  SDL_GPUShaderFormat shader_formats = SDL_GPU_SHADERFORMAT_SPIRV;
#ifdef _WIN32
  shader_formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL;
#elif defined(__APPLE__)
  shader_formats = SDL_GPU_SHADERFORMAT_METALLIB;
#endif
  SDL_GPUDevice *device = SDL_CreateGPUDevice(shader_formats, true, nullptr);
  if (device == nullptr || !SDL_ClaimWindowForGPUDevice(device, platform->window())) {
    std::fprintf(stderr, "flop: GPU device setup failed: %s\n", SDL_GetError());
    if (device != nullptr) {
      SDL_DestroyGPUDevice(device);
    }
    return 1;
  }
  SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

  platform::InputMapper input;
  TickScheduler scheduler;
  input::PilotControl pilot_control;

  int exit_code = 0;
  bool running = true;
  auto last = std::chrono::steady_clock::now();

  while (running) {
    const auto now = std::chrono::steady_clock::now();
    const double wall_elapsed = std::chrono::duration<double>(now - last).count();
    last = now;

    // drain events into one frame of input.
    input.set_active_craft(universe.controls().active_craft_id());
    input.begin_frame();
    SDL_Event event;
    while (platform->poll_event(event)) {
      input.handle_event(event);
    }
    const platform::FrameInput frame = input.build_frame(*platform);
    if (frame.quit) {
      running = false;
    }

    // queue craft commands. absolute throttle requests apply before the pilot integrates.
    for (const auto &command : frame.commands) {
      universe.controls().submit(command);
    }
    (void)universe.apply_commands();

    const FrameSchedule schedule =
        scheduler.advance(wall_elapsed, frame.scheduler, world::warp_ceiling(universe));

    // integrate the pilot throttle once per frame, over the control time the fixed steps cover.
    const craft::Craft *active = universe.find_craft(universe.controls().active_craft_id());

    // advance simulation; each step covers its simulation time at the current warp.
    const double wall_dt = TickScheduler::FIXED_STEP_SEC;
    const double sim_dt = TickScheduler::FIXED_STEP_SEC * schedule.warp;
    for (int step = 0; step < schedule.fixed_step_count; ++step) {
      const double current_throttle = active != nullptr ? active->control().throttle() : 0.0;
      pilot_control.update(frame.pilot, current_throttle, wall_dt);
      universe.controls().set_pilot_sample(pilot_control.sample());
      universe.tick(sim_dt);
    }

    // rendering time!
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer == nullptr) {
      std::fprintf(stderr, "flop: SDL_AcquireGPUCommandBuffer: %s\n", SDL_GetError());
      exit_code = 1;
      break;
    }
    SDL_GPUTexture *swapchain = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            command_buffer, platform->window(), &swapchain, nullptr, nullptr
        )) {
      std::fprintf(stderr, "flop: SDL_WaitAndAcquireGPUSwapchainTexture: %s\n", SDL_GetError());
      SDL_CancelGPUCommandBuffer(command_buffer);
      exit_code = 1;
      break;
    }
    if (swapchain != nullptr) {
      SDL_GPUColorTargetInfo target{};
      target.texture = swapchain;
      target.clear_color = SDL_FColor{0.02f, 0.05f, 0.12f, 1.0f};
      target.load_op = SDL_GPU_LOADOP_CLEAR;
      target.store_op = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(command_buffer, &target, 1, nullptr);
      SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(command_buffer);
  }

  // teardown, in reverse order from setup
  SDL_WaitForGPUIdle(device);
  SDL_ReleaseWindowFromGPUDevice(device, platform->window());
  SDL_DestroyGPUDevice(device);
  platform.reset();
  return exit_code;
}

} // namespace flop::app
