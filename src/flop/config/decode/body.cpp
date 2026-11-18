#include "flop/config/decode/body.hpp"

#include <utility>

#include "flop/config/decode/body_orbit.hpp"

namespace flop::config {

world::BodyConfig ConfigDecoder<world::BodyConfig>::decode(const ConfigView &document) {
  const auto orbit_field = document.required("orbit");
  std::optional<world::BodyOrbitConfig> orbit;
  if (orbit_field.type() == YAML::NodeType::Map) {
    orbit = ConfigDecoder<world::BodyOrbitConfig>::decode(orbit_field);
  } else if (orbit_field.as<bool>()) {
    orbit_field.fail(orbit_field.mark(), "Root body orbit must be false");
  }
  const auto radius = document.required("radius");
  const auto day_length = document.required("day_length");
  return {
      document.required("name").nonempty_string(),
      radius.checked_finite(radius.positive_double() * 1000),
      document.required("surface_gravity").positive_double(),
      day_length.checked_finite(day_length.positive_double() * 3600),
      std::move(orbit),
  };
}

} // namespace flop::config
