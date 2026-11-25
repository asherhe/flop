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
#include "flop/app/warp_policy.hpp"
#include "flop/config/asset_path.hpp"
#include "flop/craft/craft.hpp"
#include "flop/input/pilot_control.hpp"
#include "flop/platform/frame_input.hpp"
#include "flop/platform/input_mapper.hpp"
#include "flop/platform/sdl_platform.hpp"
#include "flop/render/renderer.hpp"
#include "flop/view/frame.hpp"
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

  world::ControlManager &univ_controls = universe.controls();

  // select active craft, using first defined craft
  if (!scene.craft.empty()) {
    univ_controls.submit(world::ControlManager::SelectCraft{scene.craft.front()});
    (void)univ_controls.apply_commands();
  }

  std::optional<platform::SDLPlatform> platform;
  try {
    platform.emplace("flop", 1280, 720);
  } catch (const std::exception &error) {
    std::fprintf(stderr, "flop: %s\n", error.what());
    return 1;
  }

  std::optional<render::Renderer> renderer;
  try {
    renderer.emplace(platform->window(), *resolver);
  } catch (const std::exception &error) {
    std::fprintf(stderr, "flop: GPU device setup failed: %s\n", error.what());
    return 1;
  }

  platform::InputMapper input;
  TickScheduler scheduler;
  input::PilotControl pilot_control;
  view::ViewState views;
  views.flight.focus = scene.craft.empty() ? view::Focus{scene.body_visuals.front().name}
                                           : view::Focus{scene.craft.front()};

  int exit_code = 0;
  bool running = true;
  auto last = std::chrono::steady_clock::now();

  while (running) {
    const auto now = std::chrono::steady_clock::now();
    const double real_elapsed = std::chrono::duration<double>(now - last).count();
    last = now;

    // drain events into one frame of input.
    input.set_active_craft(univ_controls.active_craft_id());
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
      univ_controls.submit(command);
    }
    (void)univ_controls.apply_commands();

    const FrameSchedule schedule =
        scheduler.advance(real_elapsed, frame.scheduler, warp_ceiling(universe));

    // Read current throttle before integrating pilot input for each fixed step.
    const craft::Craft *active = universe.find_craft(univ_controls.active_craft_id());

    // advance simulation; each step covers its simulation time at the current warp.
    const double real_dt = TickScheduler::REAL_TIME_STEP_SEC;
    const double sim_dt = TickScheduler::REAL_TIME_STEP_SEC * schedule.warp;
    for (int step = 0; step < schedule.fixed_step_count; ++step) {
      const double current_throttle = active != nullptr ? active->control().throttle() : 0.0;
      pilot_control.update(frame.pilot, current_throttle, real_dt);
      univ_controls.set_pilot_sample(pilot_control.sample());
      // Apply held input immediately before each world step.
      univ_controls.apply_pilot_sample();
      universe.tick(sim_dt);
    }

    int logical_width = 0, logical_height = 0, pixel_width = 0, pixel_height = 0;
    platform->logical_size(logical_width, logical_height);
    platform->drawable_size(pixel_width, pixel_height);
    views.flight.update(
        frame.camera, logical_width, logical_height, pixel_width, pixel_height
    );
    const auto draw_data = views.flight.build_frame(universe, scene.body_visuals);
    try {
      renderer->present(draw_data);
    } catch (const std::exception &error) {
      std::fprintf(stderr, "flop: rendering failed: %s\n", error.what());
      exit_code = 1;
      break;
    }
  }

  // teardown, in reverse order from setup
  renderer.reset();
  platform.reset();
  return exit_code;
}

} // namespace flop::app
