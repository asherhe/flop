#pragma once

#include <vector>

#include "flop/app/tick_scheduler.hpp"
#include "flop/input/pilot_control.hpp"
#include "flop/world/control_manager.hpp"

namespace flop::platform {

// camera movement requested this frame, in screen units
struct CameraInput {
  // drag delta in logical pixels
  double pan_x{0.0};
  double pan_y{0.0};
  // zoom is a signed scroll wheel amount
  double zoom{0.0};
};

// ALL input from SDL, partitioned by responsibility
struct FrameInput {
  // forwarded to PilotControl
  input::PilotActions pilot;

  // queue of simulation actions
  std::vector<world::ControlManager::Command> commands;
  // TickScheduler-related inputs
  app::FrameActions scheduler;
  // camera operations
  CameraInput camera;

  bool quit{false};
  bool focus_lost{false};

  bool resized{false};
  int pixel_width{0};
  int pixel_height{0};
};

} // namespace flop::platform
