#pragma once

namespace flop {

struct Vec2d {
  double x, y;

  double dot(const Vec2d &v) const noexcept;
};

} // namespace flop
