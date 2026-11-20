#pragma once

#include <functional>
#include <string>
#include <utility>

namespace flop::craft {

// strong wrapper over the authored PartConfig::id string. unique and nonempty
// within one craft. lets callers address an installed part without an index or
// pointer while preserving the existing YAML ids. construction does not
// validate nonemptiness or scoped uniqueness; the craft enforces those at
// assembly, where the owning context and the source field are known.
class PartId {
public:
  PartId() = default;
  explicit PartId(std::string value) noexcept : value_(std::move(value)) {}

  [[nodiscard]] const std::string &value() const noexcept { return value_; }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

  friend bool operator==(const PartId &, const PartId &) = default;

private:
  std::string value_;
};

// stable string identifying one installed PartPlugin instance, unique within
// its owning part. distinguishes a plugin instance from its plugin type: the
// existing one-instance-per-type mappings default this to the plugin's name, so
// current assets keep working, while an explicit authored id can disambiguate
// multiple instances of a type once that is supported.
class PluginId {
public:
  PluginId() = default;
  explicit PluginId(std::string value) noexcept : value_(std::move(value)) {}

  [[nodiscard]] const std::string &value() const noexcept { return value_; }
  [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

  friend bool operator==(const PluginId &, const PluginId &) = default;

private:
  std::string value_;
};

} // namespace flop::craft

template <> struct std::hash<flop::craft::PartId> {
  [[nodiscard]] std::size_t operator()(const flop::craft::PartId &id) const noexcept {
    return std::hash<std::string>{}(id.value());
  }
};

template <> struct std::hash<flop::craft::PluginId> {
  [[nodiscard]] std::size_t operator()(const flop::craft::PluginId &id) const noexcept {
    return std::hash<std::string>{}(id.value());
  }
};
