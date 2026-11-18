#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace flop::config {

// a portable, case-sensitive logical address, without a root or machine-specific location
class AssetPath {
public:
  explicit AssetPath(std::string_view address);

  [[nodiscard]] const std::string &address() const noexcept { return address_; }
  bool operator==(const AssetPath &) const = default;

private:
  std::string address_;
};

class AssetError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class AssetResolver {
public:
  // maps asset addresses to files under one selected file root
  explicit AssetResolver(const std::filesystem::path &asset_root);

  // application_base is absolute. an explicit root replaces the bundled root.
  [[nodiscard]] static AssetResolver from_startup(
      const std::filesystem::path &application_base,
      const std::optional<std::filesystem::path> &explicit_root = std::nullopt
  );

  // resolve an existing regular file without crossing outside the asset root
  [[nodiscard]] std::filesystem::path resolve(const AssetPath &path) const;

private:
  std::filesystem::path root_;
};

} // namespace flop::config
