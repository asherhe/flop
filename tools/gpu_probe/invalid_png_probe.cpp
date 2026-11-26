#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "flop/config/asset_path.hpp"
#include "flop/render/texture_cache.hpp"

int main() {
  namespace fs = std::filesystem;
  struct TempDirectory {
    fs::path path;
    TempDirectory() {
      do { path = fs::temp_directory_path() / ("flop-invalid-png-" + std::to_string(std::random_device{}())); }
      while (!fs::create_directory(path));
    }
    ~TempDirectory() { std::error_code error; fs::remove_all(path, error); }
  } temp;
  fs::create_directory(temp.path / "assets");
  { std::ofstream file(temp.path / "assets" / "invalid.png", std::ios::binary); file << "not a png"; }
  try {
    flop::render::TextureCache cache(nullptr, flop::config::AssetResolver(temp.path / "assets"));
    static_cast<void>(cache.get("assets/invalid.png", flop::render::TextureColorSpace::srgb));
  } catch (const std::exception &error) {
    const std::string message = error.what();
    if (message.find("assets/invalid.png") == std::string::npos) return 1;
    std::cout << message << '\n';
    return 0;
  }
  std::cerr << "invalid PNG was accepted\n";
  return 1;
}
