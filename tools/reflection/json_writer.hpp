#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace flop::tools::json {

// Reflection names with control characters are not supported by the format.
inline std::string quote(std::string_view value) {
  std::string result = "\"";
  for (const unsigned char c : value) {
    if (c < 32)
      throw std::runtime_error("control character in reflected name");
    if (c == '\\' || c == '"')
      result += '\\';
    result += static_cast<char>(c);
  }
  return result + '"';
}

inline std::string name(std::string_view value) {
  if (value.empty())
    throw std::runtime_error("reflection name missing");
  return quote(value);
}

inline std::string name(const char *value) {
  if (!value)
    throw std::runtime_error("reflection name missing");
  return name(std::string_view(value));
}

inline std::string required_name(const char *value) {
  if (!value || !*value)
    throw std::runtime_error("reflection name missing");
  return value;
}

struct UniformMember {
  std::string name, type;
  unsigned components;
  std::size_t offset, size;
};

struct UniformBuffer {
  std::string name;
  std::optional<unsigned> binding;
  std::size_t size;
  std::vector<UniformMember> members;
};

struct Resource {
  std::string name, type;
  unsigned space, binding;
};

struct Texture {
  std::string name;
  unsigned binding;
};

struct ShaderReflection {
  enum class Format { spirv, dxil } format;
  std::string stage;
  // DXIL reflection currently does not provide an entry point name.
  std::optional<std::string> entrypoint;
  std::vector<Resource> resources;
  std::vector<UniformBuffer> uniform_buffers;
  std::vector<Texture> textures;
};

// Preserve the existing backend-specific fields until both formats are validated.
inline std::string emit(const ShaderReflection &reflection) {
  std::string json = "{\"version\":1";
  if (reflection.format == ShaderReflection::Format::spirv)
    json += ",\"binding_validation\":\"partial\"";
  json += ",\"stage\":" + name(reflection.stage);
  if (reflection.format == ShaderReflection::Format::spirv) {
    if (!reflection.entrypoint)
      throw std::runtime_error("reflection entry point missing");
    json += ",\"entrypoint\":" + name(*reflection.entrypoint);
  } else {
    json += ",\"resources\":[";
    bool first = true;
    for (const auto &resource : reflection.resources) {
      if (!first)
        json += ',';
      first = false;
      json += "{\"name\":" + name(resource.name) + ",\"type\":" + name(resource.type) +
              ",\"space\":" + std::to_string(resource.space) +
              ",\"binding\":" + std::to_string(resource.binding) + '}';
    }
    json += ']';
  }
  json += ",\"uniform_buffers\":[";
  bool first = true;
  for (const auto &buffer : reflection.uniform_buffers) {
    if (!first)
      json += ',';
    first = false;
    json += "{\"name\":" + name(buffer.name);
    if (buffer.binding)
      json += ",\"binding\":" + std::to_string(*buffer.binding);
    json += ",\"size\":" + std::to_string(buffer.size) + ",\"members\":[";
    bool first_member = true;
    for (const auto &member : buffer.members) {
      if (!first_member)
        json += ',';
      first_member = false;
      json += "{\"name\":" + name(member.name) + ",\"type\":" + name(member.type) +
              ",\"components\":" + std::to_string(member.components) +
              ",\"offset\":" + std::to_string(member.offset) +
              ",\"size\":" + std::to_string(member.size) + '}';
    }
    json += "]}";
  }
  json += ']';
  if (reflection.format == ShaderReflection::Format::spirv) {
    json += ",\"textures\":[";
    first = true;
    for (const auto &texture : reflection.textures) {
      if (!first)
        json += ',';
      first = false;
      json += "{\"name\":" + name(texture.name) +
              ",\"binding\":" + std::to_string(texture.binding) + '}';
    }
    json += ']';
  }
  return json + '}';
}

} // namespace flop::tools::json
