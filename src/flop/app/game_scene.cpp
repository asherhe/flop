#include "flop/app/game_scene.hpp"

#include <utility>
#include <vector>

#include "flop/config/decode/body.hpp"
#include "flop/config/decode/body_visual.hpp"
#include "flop/config/decode/part.hpp"
#include "flop/config/decode/resources.hpp"
#include "flop/config/decode/save.hpp"
#include "flop/config/load.hpp"
#include "flop/craft/parts/config.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/world/body_config.hpp"

namespace flop::app {

GameScene load_game_scene(const config::AssetResolver &resolver) {
  const auto body_paths = scene_body_paths();

  std::vector<world::BodyConfig> bodies;
  std::vector<config::BodyVisualConfig> body_visuals;
  bodies.reserve(body_paths.size());
  body_visuals.reserve(body_paths.size());
  for (const auto &path : body_paths) {
    bodies.push_back(config::load_config<world::BodyConfig>(resolver, path));
    body_visuals.push_back(config::load_config<config::BodyVisualConfig>(resolver, path));
  }

  const auto resources = config::load_config<std::vector<craft::ResourceTypeConfig>>(
      resolver, config::AssetPath("assets/resources.yaml")
  );

  std::vector<craft::PartTypeConfig> part_types;
  for (const auto &type_id : scene_part_type_ids()) {
    part_types.push_back(config::load_part_type(resolver, type_id));
  }

  // construct the Universe at its final heap address before any craft, since
  // craft retain it as their UniverseContext and it is non-movable.
  GameScene scene;
  scene.universe = std::make_unique<world::Universe>(std::move(bodies), resources);
  scene.body_visuals = std::move(body_visuals);

  const auto saved = config::load_save(resolver, scene_save_path(), part_types);
  for (const auto &craft : saved) {
    scene.craft.push_back(scene.universe->add_craft(craft, part_types));
  }
  return scene;
}

} // namespace flop::app
