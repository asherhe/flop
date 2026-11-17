#include "flop/math/vec2.hpp"

using flop::Vec2d;

double Vec2d::dot(const Vec2d &v) const noexcept { return x * v.x + y * v.y; }
