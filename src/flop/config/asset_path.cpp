#include "flop/config/asset_path.hpp"

#include <algorithm>
#include <ranges>

#ifdef _WIN32
#include <memory>
#include <system_error>
#include <windows.h>
#endif

namespace flop::config {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view ASSET_PREFIX = "assets/";

bool invalid_character(char character) {
  return character == '\\' || character == ':' || static_cast<unsigned char>(character) < 32;
}

fs::path canonical_path(const fs::path &path) {
#ifdef _WIN32
  // some Windows standard libraries leave directory junctions unresolved.
  const HANDLE file = CreateFileW(
      path.c_str(),
      0,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS,
      nullptr
  );
  const auto fail = [&path] {
    throw fs::filesystem_error(
        "Cannot resolve asset path",
        path,
        {static_cast<int>(GetLastError()), std::system_category()}
    );
  };
  if (file == INVALID_HANDLE_VALUE) {
    fail();
  }
  const std::unique_ptr<void, decltype(&CloseHandle)> handle(file, &CloseHandle);
  std::wstring resolved(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetFinalPathNameByHandleW(
        handle.get(), resolved.data(), static_cast<DWORD>(resolved.size()), VOLUME_NAME_DOS
    );
    if (length == 0) {
      fail();
    }
    if (length < resolved.size()) {
      resolved.resize(length);
      break;
    }
    resolved.resize(length);
  }

  constexpr std::wstring_view UNC_PREFIX = LR"(\\?\UNC\)";
  constexpr std::wstring_view DOS_PREFIX = LR"(\\?\)";
  if (resolved.starts_with(UNC_PREFIX)) {
    resolved.replace(0, UNC_PREFIX.size(), LR"(\\)");
  } else if (resolved.starts_with(DOS_PREFIX)) {
    resolved.erase(0, DOS_PREFIX.size());
  }
  return fs::path(resolved);
#else
  return fs::canonical(path);
#endif
}

fs::path canonical_root(const fs::path &path) {
  if (path.empty()) {
    throw AssetError("The asset root is empty");
  }
  try {
    const auto root = canonical_path(path);
    if (!fs::is_directory(root)) {
      throw AssetError("The asset root is not a directory: " + path.string());
    }
    return root;
  } catch (const fs::filesystem_error &error) {
    throw AssetError("Cannot open the asset root: " + std::string(error.what()));
  }
}

fs::path exact_child(const fs::path &directory, std::string_view name) {
  // inspect the directory entries so case-insensitive filesystems obey the same rules
  const std::u8string filename(name.begin(), name.end());
  for (const auto &entry : fs::directory_iterator(directory)) {
    if (entry.path().filename().u8string() == filename) {
      return entry.path();
    }
  }
  throw AssetError("No entry with the exact name '" + std::string(name) + "'");
}

} // namespace

AssetPath::AssetPath(std::string_view address) : address_(address) {
  if (!address.starts_with(ASSET_PREFIX) || address.size() == ASSET_PREFIX.size() ||
      std::ranges::any_of(address, invalid_character)) {
    throw std::invalid_argument("Invalid asset address: " + address_);
  }
  for (const auto component : address.substr(ASSET_PREFIX.size()) | std::views::split('/')) {
    const std::string_view name(component.begin(), component.end());
    if (name.empty() || name == "." || name == "..") {
      throw std::invalid_argument("Invalid asset address: " + address_);
    }
  }
}

AssetResolver::AssetResolver(const fs::path &asset_root) : root_(canonical_root(asset_root)) {}

AssetResolver AssetResolver::from_startup(
    const fs::path &application_base, const std::optional<fs::path> &explicit_root
) {
  if (explicit_root) {
    return AssetResolver(*explicit_root);
  }
  if (!application_base.is_absolute()) {
    throw AssetError("The application base path must be absolute");
  }
  return AssetResolver(application_base / "assets");
}

fs::path AssetResolver::resolve(const AssetPath &path) const {
  const std::string_view address = path.address();
  auto current = root_;
  try {
    for (const auto component : address.substr(ASSET_PREFIX.size()) | std::views::split('/')) {
      current = canonical_path(exact_child(current, {component.begin(), component.end()}));
      if (std::mismatch(root_.begin(), root_.end(), current.begin(), current.end()).first !=
          root_.end()) {
        throw AssetError("Path resolves outside the asset root");
      }
    }
    if (!fs::is_regular_file(current)) {
      throw AssetError("Path does not refer to a regular file");
    }
    return current;
  } catch (const fs::filesystem_error &error) {
    throw AssetError(path.address() + ": " + error.what());
  } catch (const AssetError &error) {
    throw AssetError(path.address() + ": " + error.what());
  }
}

} // namespace flop::config
