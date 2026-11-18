#pragma once

namespace flop::world {

// owns universal time and the gravitational constant for one world
class Universe {
public:
  explicit Universe(double initial_ut_sec = 0, double gravitational_constant = 6.67430e-11);

  [[nodiscard]] double ut_sec() const noexcept { return ut_sec_; }
  [[nodiscard]] double gravitational_constant() const noexcept { return gravitational_constant_; }

  // advance UT by a simulation interval supplied by the application
  void tick(double dt_sec);

private:
  double ut_sec_;
  double gravitational_constant_;
};

} // namespace flop::world
