#pragma once

#include <cmath>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <yaml-cpp/yaml.h>

#include "flop/config/asset_path.hpp"

namespace flop::config {

class ConfigError : public std::runtime_error {
public:
  ConfigError(std::string address, std::string key_path, YAML::Mark mark, std::string message);
  [[nodiscard]] const std::string &address() const noexcept { return address_; }
  [[nodiscard]] const std::string &key_path() const noexcept { return key_path_; }
  [[nodiscard]] YAML::Mark source_location() const noexcept { return mark_; }
  [[nodiscard]] const std::string &message() const noexcept { return message_; }

private:
  std::string address_, key_path_, message_;
  YAML::Mark mark_;
};

struct AssetReference {
  AssetPath path;
  std::string source_address;
  std::string key_path;
  YAML::Mark mark;
  [[nodiscard]] std::filesystem::path resolve(const AssetResolver &resolver) const;
};

struct ConfigMapEntry;
class ConfigSequenceRange;
class ConfigEntryRange;

// a view of one YAML node in the config document tree
class ConfigView {
public:
  ConfigView(YAML::Node node, std::string source_address, std::string key_path = {});

  [[nodiscard]] YAML::NodeType::value type() const noexcept { return node_.Type(); }
  [[nodiscard]] const std::string &source_address() const noexcept { return source_address_; }
  [[nodiscard]] YAML::Mark mark() const noexcept { return node_.Mark(); }
  [[nodiscard]] const std::string &key_path() const noexcept { return key_path_; }

  // indicate that a mapping key is required and return a ConfigView that wraps it
  [[nodiscard]] ConfigView required(std::string_view key) const;
  // indicate that a mapping key is optional and return its ConfigView, if available
  [[nodiscard]] std::optional<ConfigView> optional(std::string_view key) const;
  // check if a mapping key is present
  [[nodiscard]] bool has(std::string_view key) const;

  // index a sequence for the index-th node
  [[nodiscard]] ConfigView at(std::size_t index) const;

  [[nodiscard]] ConfigSequenceRange sequence() const;
  [[nodiscard]] ConfigEntryRange entries() const;

  // decode the YAML node to a given type. T must be a YAML primitive supported by yaml-cpp
  template <class T> [[nodiscard]] T as() const {
    if (!node_ || node_.IsNull())
      fail(node_.Mark(), "Required value cannot be null");
    try {
      auto value = node_.as<T>();
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value))
          fail(node_.Mark(), "Value must be finite");
      }
      return value;
    } catch (const ConfigError &) {
      throw;
    } catch (const YAML::Exception &error) {
      fail(error.mark, error.msg);
    }
  }

  [[nodiscard]] double positive_double() const;
  [[nodiscard]] std::string nonempty_string() const;
  // retain this field's source location when a unit conversion overflows
  [[nodiscard]] double checked_finite(double value) const;

  // if this value node holds an asset path, resolve it
  [[nodiscard]] AssetPath asset_path() const;

  // full reference to this current ConfigView node
  [[nodiscard]] AssetReference node_reference() const;

  void validate_unique_keys() const;
  [[noreturn]] void fail(YAML::Mark mark, std::string_view message) const;

private:
  friend class ConfigSequenceRange;
  friend class ConfigEntryRange;
  [[nodiscard]] ConfigView child(YAML::Node node, std::string key) const;
  [[nodiscard]] std::string child_path(std::string_view key) const;

  YAML::Node node_;
  std::string source_address_;
  std::string key_path_;
};

struct ConfigMapEntry {
  std::string key;
  ConfigView value;
};

// a range over sequence children that creates each ConfigView as it is visited
class ConfigSequenceRange {
public:
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = ConfigView;
    using difference_type = std::ptrdiff_t;

    [[nodiscard]] ConfigView operator*() const {
      return {
          *current_, parent_.source_address_, parent_.key_path_ + "[" + std::to_string(index_) + "]"
      };
    }
    iterator &operator++() {
      ++current_;
      ++index_;
      return *this;
    }
    iterator operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }
    friend bool operator==(const iterator &left, const iterator &right) {
      return left.current_ == right.current_;
    }

  private:
    friend class ConfigSequenceRange;
    iterator(ConfigView parent, YAML::const_iterator current, std::size_t index) :
        parent_(std::move(parent)), current_(std::move(current)), index_(index) {}
    ConfigView parent_;
    YAML::const_iterator current_;
    std::size_t index_;
  };

  [[nodiscard]] iterator begin() const { return {parent_, parent_.node_.begin(), 0}; }
  [[nodiscard]] iterator end() const {
    return {parent_, parent_.node_.end(), parent_.node_.size()};
  }

private:
  friend class ConfigView;
  explicit ConfigSequenceRange(ConfigView parent) : parent_(std::move(parent)) {}
  ConfigView parent_;
};

// a range over mapping entries that creates each key/value view as it is visited
class ConfigEntryRange {
public:
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = ConfigMapEntry;
    using difference_type = std::ptrdiff_t;

    [[nodiscard]] ConfigMapEntry operator*() const {
      const auto entry = *current_;
      std::string key;
      try {
        key = entry.first.as<std::string>();
      } catch (const YAML::Exception &error) {
        parent_.fail(error.mark, "Mapping keys must be strings");
      }
      return {key, parent_.child(entry.second, key)};
    }
    iterator &operator++() {
      ++current_;
      return *this;
    }
    iterator operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }
    friend bool operator==(const iterator &left, const iterator &right) {
      return left.current_ == right.current_;
    }

  private:
    friend class ConfigEntryRange;
    iterator(ConfigView parent, YAML::const_iterator current) :
        parent_(std::move(parent)), current_(std::move(current)) {}
    ConfigView parent_;
    YAML::const_iterator current_;
  };

  [[nodiscard]] iterator begin() const { return {parent_, parent_.node_.begin()}; }
  [[nodiscard]] iterator end() const { return {parent_, parent_.node_.end()}; }

private:
  friend class ConfigView;
  explicit ConfigEntryRange(ConfigView parent) : parent_(std::move(parent)) {}
  ConfigView parent_;
};

// define an explicit specialization for each config type with
// `static T decode(const ConfigView &)`. it selects fields, conversions, defaults,
// and domain rules; there is no reflection-based decoder. T and its nested fields
// must own their data rather than retain views into the parsed YAML document.
template <class T> struct ConfigDecoder;

namespace detail {
[[nodiscard]] YAML::Node read_config(const AssetResolver &resolver, const AssetPath &path);
} // namespace detail

// read one YAML document, validate shared syntax, then use ConfigDecoder<T> to
// build the requested type. the caller selects T; it is not inferred from YAML.
template <class T>
[[nodiscard]] T load_config(const AssetResolver &resolver, const AssetPath &path) {
  static_assert(
      !std::is_reference_v<T> && !std::is_pointer_v<T> &&
          !std::is_same_v<std::remove_cv_t<T>, std::string_view>,
      "ConfigDecoder output must be an owning value"
  );
  const auto document = detail::read_config(resolver, path);
  ConfigView root(document, path.address());
  try {
    root.validate_unique_keys();
    return ConfigDecoder<T>::decode(root);
  } catch (const ConfigError &) {
    throw;
  } catch (const YAML::Exception &error) {
    root.fail(error.mark, error.msg);
  }
}

} // namespace flop::config
