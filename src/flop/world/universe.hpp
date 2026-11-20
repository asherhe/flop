#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "flop/craft/config.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/world/celestial_body.hpp"
#include "flop/world/control_manager.hpp"
#include "flop/world/craft_id.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::world {

// describes a full game world.
// owns universal time (UT), celestial bodies, all universal Manager instances,
// and every craft in the world. a craft retains a pointer back into this
// Universe (through UniverseContext) and is stored behind a stable owning
// pointer, so Universe must be constructed at its final address and is neither
// copyable nor movable: relocating it would dangle those back-pointers.
class Universe : public UniverseContext {
public:
  explicit Universe(
      std::vector<BodyConfig> configs,
      double initial_ut_sec = 0,
      double gravitational_constant = 6.67430e-11
  );
  Universe(
      std::vector<BodyConfig> configs,
      std::vector<craft::ResourceTypeConfig> resource_configs,
      double initial_ut_sec = 0,
      double gravitational_constant = 6.67430e-11
  );

  Universe(const Universe &) = delete;
  Universe(Universe &&) = delete;
  Universe &operator=(const Universe &) = delete;
  Universe &operator=(Universe &&) = delete;
  ~Universe() override = default;

  [[nodiscard]] double ut_sec() const noexcept { return ut_sec_; }
  [[nodiscard]] double gravitational_constant() const noexcept { return gravitational_constant_; }
  [[nodiscard]] const craft::ResourceRegistry &resources() const noexcept override {
    return resources_;
  }

  [[nodiscard]] std::span<const CelestialBody> bodies() const noexcept { return bodies_; }
  // get CelestialBody by name
  [[nodiscard]] const CelestialBody &body(std::string_view name) const;

  [[nodiscard]] const orbit::RefFrameManager &reference_frames() const noexcept override {
    return frames_;
  }
  // get FrameId by celestial body name
  [[nodiscard]] orbit::FrameId frame_id(std::string_view name) const;

  // position and velocity of a body's center relative to the root at the requested UT
  [[nodiscard]] orbit::StateVectors body_state(std::string_view name, double ut_sec) const;

  // build a craft from its configuration and take ownership of it, returning a
  // stable id assigned in increasing order and never reused during this world's
  // lifetime. the craft is stored behind an owning pointer, so adding more
  // craft never relocates existing ones and outstanding ids and references stay
  // valid. the craft retains this Universe as its UniverseContext.
  CraftId add_craft(
      craft::CraftConfig config, std::vector<craft::PartTypeConfig> part_types = {}
  );

  // number of craft currently in the world.
  [[nodiscard]] std::size_t craft_count() const noexcept { return craft_order_.size(); }
  // ids of every craft, in the order they were added.
  [[nodiscard]] std::vector<CraftId> craft_ids() const;
  // the craft with the given id. throws std::out_of_range for an unknown or
  // stale id, so a retired or never-assigned id fails rather than aliasing.
  [[nodiscard]] const craft::Craft &craft(CraftId id) const;
  // the craft with the given id, or nullptr if there is none.
  [[nodiscard]] const craft::Craft *find_craft(CraftId id) const noexcept;

  // the world's control manager, which owns active-craft selection, the pending
  // command queue, and the held pilot sample. UI and input submit through the
  // mutable accessor; rendering and inspection use the const one. Mutable craft
  // access stays private to Universe so only apply_commands() can change craft.
  [[nodiscard]] ControlManager &controls() noexcept { return controls_; }
  [[nodiscard]] const ControlManager &controls() const noexcept { return controls_; }

  // drain the control queue, executing each command against live craft storage
  // in submit order, and return one result per command. Safe to call every
  // frame, including while paused; discrete commands still apply. A command
  // whose target no longer exists fails without touching any other craft.
  std::vector<ControlManager::Result> apply_commands();

  // advance UT by a simulation interval supplied by the application
  void tick(double dt_sec);

private:
  // the craft with the given id for internal mutation, or nullptr. kept private
  // so external callers cannot obtain mutable craft access; only command
  // execution and stepping use it.
  [[nodiscard]] craft::Craft *mutable_craft(CraftId id) noexcept;

  // clear the active craft's command plugin held throttle rate and steering, so
  // a deselected or focus-lost craft coasts on its persistent throttle without
  // continuing to receive pilot input. does nothing when no command plugin is
  // installed or no craft is active.
  void neutralize_active_pilot_input();

  double ut_sec_;
  double gravitational_constant_;
  std::vector<CelestialBody> bodies_;
  orbit::RefFrameManager frames_;
  orbit::FrameId root_frame_id_;
  craft::ResourceRegistry resources_;
  // owning craft storage keyed by id. owning pointers keep each craft at a
  // stable address as the map grows. craft_order_ preserves insertion order for
  // deterministic enumeration.
  std::unordered_map<CraftId, std::unique_ptr<craft::Craft>> craft_;
  std::vector<CraftId> craft_order_;
  // next craft id to assign; increments on every add and is never reset.
  std::uint64_t next_craft_id_{1};
  // active-craft selection, command queue, and held pilot sample.
  ControlManager controls_;
};

} // namespace flop::world
