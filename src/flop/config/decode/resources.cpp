#include "flop/config/decode/resources.hpp"

#include <algorithm>
#include <utility>

namespace flop::config {
namespace {

craft::ResourceFlow decode_flow(const ConfigView &field) {
  const auto value = field.as<std::string>();
  if (value == "None")
    return craft::ResourceFlow::none;
  if (value == "Stage")
    return craft::ResourceFlow::stage;
  if (value == "Craft")
    return craft::ResourceFlow::craft;
  field.fail(field.mark(), "Unknown resource flow mode");
}

} // namespace

std::vector<craft::ResourceConfig> ConfigDecoder<std::vector<craft::ResourceConfig>>::decode(
    const ConfigView &document
) {
  std::vector<craft::ResourceConfig> result;
  for (const auto &entry : document.sequence()) {
    const auto id = entry.required("type");
    auto resource_id = id.nonempty_string();
    if (std::ranges::find(result, resource_id, &craft::ResourceConfig::id) != result.end()) {
      id.fail(id.mark(), "Duplicate resource ID");
    }
    result.push_back({
        std::move(resource_id),
        entry.required("name").nonempty_string(),
        entry.required("density").positive_double(),
        decode_flow(entry.required("flow")),
    });
  }
  return result;
}

} // namespace flop::config
