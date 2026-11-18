#include "flop/world/universe.hpp"

#include <cmath>
#include <stdexcept>

namespace flop::world {

Universe::Universe(double initial_ut_sec, double gravitational_constant) :
    ut_sec_(initial_ut_sec), gravitational_constant_(gravitational_constant) {
  if (!std::isfinite(ut_sec_)) {
    throw std::invalid_argument("Universe UT must be finite");
  }
  if (!std::isfinite(gravitational_constant_) || gravitational_constant_ <= 0) {
    throw std::invalid_argument("Universe gravitational constant must be finite and positive");
  }
}

void Universe::tick(double dt_sec) {
  if (!std::isfinite(dt_sec) || dt_sec <= 0) {
    throw std::invalid_argument("Universe tick must be finite and positive");
  }
  const double next_ut_sec = ut_sec_ + dt_sec;
  if (!std::isfinite(next_ut_sec) || next_ut_sec <= ut_sec_) {
    throw std::invalid_argument("Universe tick must advance to a finite UT");
  }
  ut_sec_ = next_ut_sec;
}

} // namespace flop::world
