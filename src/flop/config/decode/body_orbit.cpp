#include "flop/config/decode/body_orbit.hpp"

#include <numbers>
#include <utility>

namespace flop::config {

world::BodyOrbitConfig ConfigDecoder<world::BodyOrbitConfig>::decode(const ConfigView &document) {
  auto parent_name = document.required("parent").nonempty_string();
  const auto eccentricity_field = document.required("eccentricity");
  const auto eccentricity = eccentricity_field.as<double>();
  if (eccentricity < 0 || eccentricity >= 1) {
    eccentricity_field.fail(eccentricity_field.mark(), "Body orbit must be bound");
  }
  const auto semimajor_axis = document.required("semimajor_axis");
  const auto longitude = document.required("longitude_periapsis");
  const auto mean_anomaly = document.required("epoch_mean_anom");
  return {
      std::move(parent_name),
      semimajor_axis.checked_finite(semimajor_axis.positive_double() * 1000),
      eccentricity,
      longitude.checked_finite(longitude.as<double>() * (std::numbers::pi / 180)),
      mean_anomaly.checked_finite(mean_anomaly.as<double>() * (std::numbers::pi / 180)),
      document.required("epoch_time").as<double>(),
  };
}

} // namespace flop::config
