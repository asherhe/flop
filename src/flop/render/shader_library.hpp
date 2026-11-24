#pragma once

#include <stdexcept>
#include <span>
#include <string>
#include <string_view>

namespace flop::render::shaders {

struct Entry {
  std::string_view id, stage, format, entrypoint;
  const unsigned char *code;
  std::size_t size;
  unsigned samplers, storage_textures, storage_buffers, uniform_buffers;
  std::string_view reflection_json{};
};

inline bool contains(std::span<const Entry> manifest, std::string_view id,
                     std::string_view stage, std::string_view format) {
  for (const Entry &entry : manifest) {
    if (entry.id == id && entry.stage == stage && entry.format == format) return true;
  }
  return false;
}

inline const Entry &find(std::span<const Entry> manifest, std::string_view id,
                         std::string_view stage, std::string_view format) {
  std::string available;
  for (const Entry &entry : manifest) {
    if (entry.id == id && entry.stage == stage) {
      if (entry.format == format) return entry;
      if (!available.empty()) available += ", ";
      available += entry.format;
    }
  }
  if (available.empty()) available = "none";
  throw std::runtime_error("shader '" + std::string(id) + "', stage '" + std::string(stage) +
                           "', format '" + std::string(format) + "' unavailable; available: " +
                           available);
}
} // namespace flop::render::shaders
