#pragma once

#include <memory>
#include <string>
#include <vector>

#include "flop/config/asset_path.hpp"
#include "flop/config/body_visual_config.hpp"
#include "flop/world/craft_id.hpp"
#include "flop/world/universe.hpp"

namespace flop::app {

// the loaded first flight scene: a populated Universe plus the per-body visual
// configs the renderer reads. The set of bodies, the resource list, and the
// part types are fixed by the game build (hard-coded below); only the craft are
// loaded from a save. Universe is non-movable and holds craft back-pointers, so
// it is owned here behind a unique_ptr and handed out by reference.
struct GameScene {
  std::unique_ptr<world::Universe> universe;
  std::vector<config::BodyVisualConfig> body_visuals;
  std::vector<world::CraftId> craft;
};

// the body definition files this build ships, root first.
[[nodiscard]] inline std::vector<config::AssetPath> scene_body_paths() {
  return {
      config::AssetPath("assets/celestialbodies/Sun/Sun.yaml"),
      config::AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml"),
  };
}

// the part type folder names this build ships.
[[nodiscard]] inline std::vector<std::string> scene_part_type_ids() {
  return {"TestingCommandPod", "TestingTank", "TestingEngine"};
}

// the save loaded at startup. a later menu can choose among saves; for now the
// first scene always loads the orbital test save.
[[nodiscard]] inline config::AssetPath scene_save_path() {
  return config::AssetPath("assets/saves/orbital_test.yaml");
}

// build the scene from assets under the given resolver: decode the hard-coded
// bodies (physical and visual), resources, and part types, construct the
// Universe at its final address, then load the save and add every craft.
// Throws config::ConfigError / std::invalid_argument on malformed or missing
// assets, naming the offending file.
[[nodiscard]] GameScene load_game_scene(const config::AssetResolver &resolver);

} // namespace flop::app
