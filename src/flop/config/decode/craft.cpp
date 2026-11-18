#include "flop/config/decode/craft.hpp"

namespace flop::config {
namespace {

craft::CraftOrbitConfig decode_orbit(const ConfigView &orbit) {
  const auto h = orbit.required("h");
  const auto specific_angular_momentum = h.as<double>();
  if (specific_angular_momentum == 0) {
    h.fail(h.mark(), "Specific angular momentum cannot be zero");
  }
  const auto e = orbit.required("e");
  const auto eccentricity = e.as<double>();
  if (eccentricity < 0) {
    e.fail(e.mark(), "Eccentricity cannot be negative");
  }
  return {
      orbit.required("parent").nonempty_string(),
      specific_angular_momentum,
      eccentricity,
      orbit.required("omega").as<double>(),
      orbit.required("M0").as<double>(),
      orbit.required("t0").as<double>(),
  };
}

} // namespace

craft::CraftConfig ConfigDecoder<craft::CraftConfig>::decode(const ConfigView &document) {
  const auto rotation = document.required("rotation");
  // part entries are decoded by PartConfig in the next phase.
  static_cast<void>(document.required("parts").sequence());
  craft::CraftConfig result{
      document.required("name").nonempty_string(),
      decode_orbit(document.required("orbit")),
      {rotation.required("angle").as<double>(), rotation.required("momentum").as<double>()},
  };
  return result;
}

} // namespace flop::config
