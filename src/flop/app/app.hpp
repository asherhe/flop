#pragma once

namespace flop::app {

// owns all app lifecycle and logic
class App {
public:
  [[nodiscard]] int run(int argc, char **argv);
};

} // namespace flop::app
