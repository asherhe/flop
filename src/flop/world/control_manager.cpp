#include "flop/world/control_manager.hpp"

#include <algorithm>
#include <type_traits>
#include <utility>

#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"

namespace flop::world {

std::vector<ControlManager::Result> ControlManager::apply_commands() {
  std::vector<ControlManager::Result> results;
  results.reserve(pending_.size());
  // take the queue so commands submitted during execution (none today) wait for
  // the next drain, and so the manager is left empty afterward.
  std::vector<ControlManager::Command> commands;
  commands.swap(pending_);

  for (auto &command : commands) {
    const ControlManager::Status status = std::visit(
        [&](auto &value) -> ControlManager::Status {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ControlManager::SelectCraft>) {
            if (access_->find_craft(value.craft) == nullptr) {
              return ControlManager::Status::missing_target;
            }
            if (value.craft != active_craft_) {
              // leaving a craft neutralizes its held input so it coasts on its
              // persistent throttle.
              clear_active_pilot_input();
              active_craft_ = value.craft;
            }
            return ControlManager::Status::applied;
          } else if constexpr (std::is_same_v<T, ControlManager::SetThrottle>) {
            craft::Craft *active = access_->mutable_craft(active_craft_);
            if (active == nullptr) {
              return ControlManager::Status::unsupported;
            }
            active->control().set_throttle(std::clamp(value.throttle, 0.0, 1.0));
            return ControlManager::Status::applied;
          } else if constexpr (std::is_same_v<T, ControlManager::SetPluginEnabled>) {
            craft::Craft *target = access_->mutable_craft(value.target.craft);
            if (target == nullptr) {
              return ControlManager::Status::missing_target;
            }
            craft::Part *part = target->find_part(value.target.part);
            if (part == nullptr) {
              return ControlManager::Status::missing_target;
            }
            craft::plugin::PartPlugin *plugin = part->find_plugin(value.target.plugin);
            if (plugin == nullptr) {
              return ControlManager::Status::missing_target;
            }
            plugin->set_enabled(value.enabled);
            return ControlManager::Status::applied;
          }
          return ControlManager::Status::unsupported;
        },
        command
    );
    results.push_back({std::move(command), status});
  }
  return results;
}

void ControlManager::clear_active_pilot_input() {
  if (auto *active = access_->mutable_craft(active_craft_)) {
    active->control().set_steering(0.0);
  }
}

void ControlManager::apply_pilot_sample() {
  if (auto *active = access_->mutable_craft(active_craft_)) {
    active->control().set_throttle(pilot_sample_.throttle);
    active->control().set_steering(pilot_sample_.steering);
  }
}

} // namespace flop::world
