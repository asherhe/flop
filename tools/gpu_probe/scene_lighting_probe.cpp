#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <SDL3/SDL.h>

#include "flop/app/game_scene.hpp"
#include "flop/config/asset_path.hpp"
#include "flop/render/renderer.hpp"
#include "flop/view/frame.hpp"
#include "flop/world/control_manager.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: flop_scene_lighting_probe <assets-directory>\n";
    return 2;
  }
  try {
    const flop::config::AssetResolver resolver{std::filesystem::path(argv[1])};
    auto scene = flop::app::load_game_scene(resolver);
    auto &world = *scene.universe;
    const auto id = scene.craft.front();
    world.controls().submit(flop::world::ControlManager::SelectCraft{id});
    (void)world.controls().apply_commands();
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    auto *window = SDL_CreateWindow("scene lighting probe", 16, 16, SDL_WINDOW_HIDDEN);
    if (!window) throw std::runtime_error(SDL_GetError());
    {
      flop::render::Renderer renderer(window, resolver);
      flop::view::CameraState camera;
      camera.focus = id;
      camera.viewport_width = camera.viewport_height = 1;
      camera.pixels_per_meter = 50;
      const auto sample = [&] {
        camera.pan_meters = {};
        auto frame = camera.build_frame({world, scene.body_visuals});
        const auto engine = std::ranges::find_if(frame.sprites, [](const auto &sprite) {
          return sprite.part.value() == "TestingEngine_3";
        });
        if (engine == frame.sprites.end()) throw std::runtime_error("engine sprite missing");
        camera.pan_meters = engine->camera_relative_meters;
        frame = camera.build_frame({world, scene.body_visuals});
        // Isolate local lights while retaining the shipped geometry and materials.
        for (auto &sprite : frame.sprites) {
          sprite.sun.color = {};
          sprite.planet_shine = {};
        }
        return renderer.readback_pixel(frame);
      };
      const auto idle = sample();
      world.controls().submit(flop::world::ControlManager::SetThrottle{1.0});
      (void)world.controls().apply_commands();
      world.tick(0.02);
      const auto burning = sample();
      if (burning[0] <= idle[0] + 3 && burning[1] <= idle[1] + 3)
        throw std::runtime_error("engine thrust did not illuminate the shipped engine sprite");
      world.controls().submit(flop::world::ControlManager::SetThrottle{0.0});
      (void)world.controls().apply_commands();
      world.tick(0.02);
      const auto stopped = sample();
      if (stopped[0] > idle[0] + 3 || stopped[1] > idle[1] + 3 || stopped[2] > idle[2] + 3)
        throw std::runtime_error("engine light persisted after throttle closed");
      std::cout << "shipped scene idle: " << int(idle[0]) << ", " << int(idle[1]) << ", " << int(idle[2])
                << "; burning: " << int(burning[0]) << ", " << int(burning[1]) << ", " << int(burning[2])
                << "; stopped: " << int(stopped[0]) << ", " << int(stopped[1]) << ", " << int(stopped[2]) << '\n';
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "scene lighting probe: " << error.what() << '\n';
    SDL_Quit();
    return 1;
  }
}
