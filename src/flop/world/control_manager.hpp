#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "flop/world/craft_id.hpp"
#include "flop/world/pilot_sample.hpp"
#include "flop/world/plugin_target.hpp"

namespace flop::world {

class Universe;

// owns active-craft selection and the queue of external craft-control commands
// for one world. UI and input submit typed commands; Universe drains the queue
// once per frame through apply_commands() and resolves each target against live
// craft storage at that moment. The manager holds no pointer back into Universe
// and no craft references: it carries only the pending commands, the active
// craft id, and the latest pilot sample. Only external changes flow through
// here; a plugin's own internal behavior (fuel drain, flame-out, force
// accumulation) is not a command.
class ControlManager {
public:
  // select which craft receives held pilot input and pilot throttle commands.
  struct SelectCraft {
    CraftId craft;
  };
  // set the active craft's persistent throttle
  struct SetThrottle {
    double throttle;
  };
  // enable or disable one specific installed plugin instance.
  struct SetPluginEnabled {
    PluginTarget target;
    bool enabled;
  };

  // a typed external command. no opaque payloads: each alternative names its
  // own data and the exact craft or plugin it affects.
  using Command = std::variant<SelectCraft, SetThrottle, SetPluginEnabled>;

  // outcome of executing one command, in submit order.
  enum class Status : std::uint8_t {
    // the command applied to its intended target.
    applied,
    // the command named a craft or plugin that does not exist in the world.
    missing_target,
    // the command cannot apply in the current state (e.g. a pilot throttle
    // command with no craft selected).
    unsupported,
  };
  struct Result {
    Command command;
    Status status;
  };

  // queue a command for the next apply_commands(). commands are copied or moved
  // into the queue and resolve their targets only at execution, preserving
  // submit order.
  void submit(Command command) { pending_.push_back(std::move(command)); }

  // replace the held pilot input for the active craft. the value is read when
  // Universe prepares the active command plugin before a step.
  void set_pilot_sample(PilotSample sample) noexcept { pilot_sample_ = sample; }
  [[nodiscard]] PilotSample pilot_sample() const noexcept { return pilot_sample_; }

  // the craft currently receiving held pilot input, or CraftId::invalid when no
  // craft is selected.
  [[nodiscard]] CraftId active_craft_id() const noexcept { return active_craft_; }

  [[nodiscard]] bool has_pending() const noexcept { return !pending_.empty(); }

private:
  friend class Universe;

  std::vector<Command> pending_;
  CraftId active_craft_{CraftId::invalid};
  PilotSample pilot_sample_{};
};

} // namespace flop::world
