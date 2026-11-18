#include "flop/config/load.hpp"

#include <fstream>
#include <vector>

namespace flop::config {

namespace {
std::string format_error(
    const std::string &address, const std::string &key, YAML::Mark mark, const std::string &message
) {
  std::string result = address;
  if (!mark.is_null())
    result += ":" + std::to_string(mark.line + 1) + ":" + std::to_string(mark.column + 1);
  if (!key.empty())
    result += " [" + key + "]";
  return result + ": " + message;
}

[[noreturn]] void fail_config(const AssetPath &path, YAML::Mark mark, std::string_view message) {
  throw ConfigError(path.address(), {}, mark, std::string(message));
}
} // namespace

ConfigError::ConfigError(
    std::string address, std::string key_path, YAML::Mark mark, std::string message
) :
    std::runtime_error(format_error(address, key_path, mark, message)),
    address_(std::move(address)), key_path_(std::move(key_path)), message_(std::move(message)),
    mark_(mark) {}

std::filesystem::path AssetReference::resolve(const AssetResolver &resolver) const {
  try {
    return resolver.resolve(path);
  } catch (const AssetError &error) {
    throw ConfigError(source_address, key_path, mark, error.what());
  }
}

ConfigView::ConfigView(YAML::Node node, std::string source_address, std::string key_path) :
    node_(std::move(node)), source_address_(std::move(source_address)),
    key_path_(std::move(key_path)) {}

std::string ConfigView::child_path(std::string_view key) const {
  return key_path_.empty() ? std::string(key) : key_path_ + "." + std::string(key);
}

ConfigView ConfigView::child(YAML::Node node, std::string key) const {
  return ConfigView(std::move(node), source_address_, child_path(key));
}

ConfigView ConfigView::required(std::string_view key) const {
  if (!node_.IsMap())
    fail(node_.Mark(), "Expected a mapping");
  const auto node = node_[std::string(key)];
  auto result = child(node, std::string(key));
  if (!node)
    result.fail(node_.Mark(), "Missing required field");
  if (node.IsNull())
    result.fail(node.Mark(), "Required value cannot be null");
  return result;
}

std::optional<ConfigView> ConfigView::optional(std::string_view key) const {
  if (!node_.IsMap())
    fail(node_.Mark(), "Expected a mapping");
  const auto node = node_[std::string(key)];
  if (!node)
    return std::nullopt;
  auto result = child(node, std::string(key));
  if (node.IsNull())
    result.fail(node.Mark(), "Optional value cannot be null");
  return result;
}

bool ConfigView::has(std::string_view key) const {
  if (!node_.IsMap())
    fail(node_.Mark(), "Expected a mapping");
  return static_cast<bool>(node_[std::string(key)]);
}

double ConfigView::positive_double() const {
  const double value = as<double>();
  if (value <= 0)
    fail(node_.Mark(), "Value must be positive");
  return value;
}

std::string ConfigView::nonempty_string() const {
  auto value = as<std::string>();
  if (value.empty())
    fail(node_.Mark(), "Value cannot be empty");
  return value;
}

double ConfigView::checked_finite(double value) const {
  if (!std::isfinite(value))
    fail(node_.Mark(), "Value must be finite after conversion");
  return value;
}

ConfigView ConfigView::at(std::size_t index) const {
  if (!node_.IsSequence())
    fail(node_.Mark(), "Expected a sequence");
  if (index >= node_.size())
    fail(node_.Mark(), "Sequence index is out of range");
  return ConfigView(node_[index], source_address_, key_path_ + "[" + std::to_string(index) + "]");
}

ConfigSequenceRange ConfigView::sequence() const {
  if (!node_.IsSequence())
    fail(node_.Mark(), "Expected a sequence");
  return ConfigSequenceRange(*this);
}

ConfigEntryRange ConfigView::entries() const {
  if (!node_.IsMap())
    fail(node_.Mark(), "Expected a mapping");
  return ConfigEntryRange(*this);
}

AssetPath ConfigView::asset_path() const {
  try {
    return AssetPath(as<std::string>());
  } catch (const std::invalid_argument &error) {
    fail(node_.Mark(), error.what());
  }
}

AssetReference ConfigView::node_reference() const {
  return {asset_path(), source_address_, key_path_, node_.Mark()};
}

void ConfigView::validate_unique_keys() const {
  const auto &node = node_;
  if (node.IsSequence()) {
    for (std::size_t i = 0; i < node.size(); ++i)
      at(i).validate_unique_keys();
  } else if (node.IsMap()) {
    std::vector<std::pair<std::string, YAML::Mark>> seen;
    for (const auto &entry : node) {
      std::string key;
      try {
        key = entry.first.as<std::string>();
      } catch (const YAML::Exception &error) {
        fail(error.mark, "Mapping keys must be strings");
      }
      for (const auto &[prior, mark] : seen) {
        if (prior == key)
          child(entry.second, key)
              .fail(
                  entry.first.Mark(),
                  "Duplicate mapping key (first declared at line " + std::to_string(mark.line + 1) +
                      ")"
              );
      }
      seen.emplace_back(key, entry.first.Mark());
      child(entry.second, key).validate_unique_keys();
    }
  }
}

void ConfigView::fail(YAML::Mark mark, std::string_view message) const {
  throw ConfigError(source_address_, key_path_, mark, std::string(message));
}

YAML::Node detail::read_config(const AssetResolver &resolver, const AssetPath &path) {
  try {
    std::ifstream input(resolver.resolve(path), std::ios::binary);
    if (!input) {
      fail_config(path, YAML::Mark::null_mark(), "Cannot open configuration file");
    }
    const auto documents = YAML::LoadAll(input);
    if (input.bad()) {
      fail_config(path, YAML::Mark::null_mark(), "Cannot read configuration file");
    }
    if (documents.size() != 1) {
      const auto mark = documents.empty() ? YAML::Mark::null_mark() : documents.at(1).Mark();
      fail_config(path, mark, "Expected exactly one YAML document");
    }
    return documents.front();
  } catch (const AssetError &error) {
    // the resolver already included the logical address
    throw ConfigError(path.address(), {}, YAML::Mark::null_mark(), error.what());
  } catch (const YAML::Exception &error) {
    fail_config(path, error.mark, error.msg);
  }
}

} // namespace flop::config
