#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/load.hpp"

namespace {

namespace fs = std::filesystem;
using namespace flop::config;
using Catch::Matchers::ContainsSubstring;

struct TestConfig {
  std::string name;
  int count;
  AssetPath image;
};

struct OptionalConfig {
  std::optional<double> value;
  bool has_enabled;
};

class TempDirectory {
public:
  TempDirectory() {
    const auto temp = fs::canonical(fs::temp_directory_path());
    std::random_device random;
    do {
      path_ = temp / ("flop-config-test-" + std::to_string(random()));
    } while (!fs::create_directory(path_));
  }

  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;

  ~TempDirectory() {
    std::error_code error;
    fs::remove_all(path_, error);
  }

  [[nodiscard]] const fs::path &path() const { return path_; }

private:
  fs::path path_;
};

class WorkingDirectory {
public:
  explicit WorkingDirectory(const fs::path &path) : original_(fs::current_path()) {
    fs::current_path(path);
  }

  WorkingDirectory(const WorkingDirectory &) = delete;
  WorkingDirectory &operator=(const WorkingDirectory &) = delete;

  ~WorkingDirectory() {
    std::error_code error;
    fs::current_path(original_, error);
  }

private:
  fs::path original_;
};

void write_file(const fs::path &path, std::string_view contents) {
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << contents;
  output.close();
  REQUIRE(output.good());
}

} // namespace

namespace flop::config {

template <> struct ConfigDecoder<TestConfig> {
  static TestConfig decode(const ConfigView &document) {
    const auto display = document.required("display");
    return {
        document.required("name").as<std::string>(),
        document.required("count").as<int>(),
        display.required("image").asset_path(),
    };
  }
};

template <> struct ConfigDecoder<std::vector<std::string>> {
  static std::vector<std::string> decode(const ConfigView &document) {
    std::vector<std::string> result;
    for (const auto &item : document.sequence())
      result.push_back(item.as<std::string>());
    return result;
  }
};

template <> struct ConfigDecoder<std::map<std::string, int>> {
  static std::map<std::string, int> decode(const ConfigView &document) {
    std::map<std::string, int> result;
    for (const auto &entry : document.entries())
      result.emplace(entry.key, entry.value.as<int>());
    return result;
  }
};

template <> struct ConfigDecoder<OptionalConfig> {
  static OptionalConfig decode(const ConfigView &document) {
    const auto value = document.optional("value");
    return {
        value ? std::optional<double>(value->as<double>()) : std::nullopt, document.has("enabled")
    };
  }
};

} // namespace flop::config

TEST_CASE("Typed YAML loading returns owning mapping and sequence values") {
  const TempDirectory temp;
  const auto root = temp.path() / "assets";
  const auto texture = root / "parts" / "TestPart" / "Icon.png";
  write_file(texture, "test texture");
  write_file(
      root / "mapping.yaml",
      "name: Test Part\ncount: 3\ndisplay:\n  image: assets/parts/TestPart/Icon.png\n"
  );
  write_file(root / "sequence.yaml", "- Sun\n- Hamswell\n");
  write_file(root / "mapping_entries.yaml", "Sun: 1\nHamswell: 2\n");
  const AssetResolver resolver(root);

  const auto mapping = load_config<TestConfig>(resolver, AssetPath("assets/mapping.yaml"));
  CHECK(mapping.name == "Test Part");
  CHECK(mapping.count == 3);
  CHECK(mapping.image.address() == "assets/parts/TestPart/Icon.png");
  const auto resolved_image = resolver.resolve(mapping.image);
  CHECK(resolved_image.is_absolute());
  CHECK(fs::equivalent(resolved_image, texture));
  const auto sequence =
      load_config<std::vector<std::string>>(resolver, AssetPath("assets/sequence.yaml"));
  CHECK(sequence == std::vector<std::string>{"Sun", "Hamswell"});
  const auto entries =
      load_config<std::map<std::string, int>>(resolver, AssetPath("assets/mapping_entries.yaml"));
  CHECK(entries == std::map<std::string, int>{{"Sun", 1}, {"Hamswell", 2}});
}

TEST_CASE("Config views add sequence indices to conversion errors") {
  const TempDirectory temp;
  const AssetResolver resolver(temp.path());
  write_file(temp.path() / "bad_sequence.yaml", "- Sun\n- [not, scalar]\n");
  CHECK_THROWS_WITH(
      load_config<std::vector<std::string>>(resolver, AssetPath("assets/bad_sequence.yaml")),
      ContainsSubstring("[1]")
  );
}

TEST_CASE("Config views validate reusable scalar constraints at the source field") {
  const ConfigView root(
      YAML::Load("name: ''\nradius: -1\nvalid_name: Hamswell\nvalid_radius: 2\n"),
      "assets/values.yaml"
  );
  CHECK(root.required("valid_name").nonempty_string() == "Hamswell");
  CHECK(root.required("valid_radius").positive_double() == 2);
  CHECK_THROWS_WITH(
      root.required("name").nonempty_string(),
      ContainsSubstring("[name]") && ContainsSubstring("cannot be empty")
  );
  CHECK_THROWS_WITH(
      root.required("radius").positive_double(),
      ContainsSubstring("[radius]") && ContainsSubstring("must be positive")
  );
  CHECK_THROWS_WITH(
      root.required("valid_radius").checked_finite(std::numeric_limits<double>::infinity()),
      ContainsSubstring("[valid_radius]") && ContainsSubstring("finite after conversion")
  );
}

TEST_CASE("Config views distinguish absent optional fields and report their paths") {
  const TempDirectory temp;
  const AssetResolver resolver(temp.path());
  write_file(temp.path() / "absent.yaml", "enabled: true\n");
  write_file(temp.path() / "present.yaml", "value: 2.5\n");
  write_file(temp.path() / "null_optional.yaml", "value: null\n");
  const auto absent = load_config<OptionalConfig>(resolver, AssetPath("assets/absent.yaml"));
  CHECK_FALSE(absent.value.has_value());
  CHECK(absent.has_enabled);
  const auto present = load_config<OptionalConfig>(resolver, AssetPath("assets/present.yaml"));
  REQUIRE(present.value.has_value());
  CHECK(*present.value == 2.5);
  CHECK_FALSE(present.has_enabled);
  CHECK_THROWS_WITH(
      load_config<OptionalConfig>(resolver, AssetPath("assets/null_optional.yaml")),
      ContainsSubstring("[value]") && ContainsSubstring("cannot be null")
  );
}

TEST_CASE("Config errors identify the document and field location") {
  const TempDirectory temp;
  const AssetResolver resolver(temp.path());

  write_file(temp.path() / "bad.yaml", "name: [1, 2\n");
  CHECK_THROWS_AS(load_config<TestConfig>(resolver, AssetPath("assets/bad.yaml")), ConfigError);
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/bad.yaml")),
      ContainsSubstring("assets/bad.yaml:")
  );

  write_file(temp.path() / "missing.yaml", "name: Test\ncount: 3\ndisplay: {}\n");
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/missing.yaml")),
      ContainsSubstring("assets/missing.yaml:3:10 [display.image]") &&
          ContainsSubstring("Missing required field")
  );

  write_file(
      temp.path() / "wrong.yaml", "name: Test\ncount: [1, 2]\ndisplay:\n  image: assets/Icon.png\n"
  );
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/wrong.yaml")),
      ContainsSubstring("assets/wrong.yaml:2:8 [count]")
  );

  write_file(
      temp.path() / "invalid_address.yaml",
      "name: Test\ncount: 3\ndisplay:\n  image: assets/../Icon.png\n"
  );
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/invalid_address.yaml")),
      ContainsSubstring("assets/invalid_address.yaml:4:10 [display.image]") &&
          ContainsSubstring("Invalid asset address")
  );

  write_file(temp.path() / "multiple.yaml", "---\n- first\n---\n- second\n");
  write_file(temp.path() / "empty.yaml", "# no document\n");
  for (const auto path : {"assets/multiple.yaml", "assets/empty.yaml"}) {
    CHECK_THROWS_WITH(
        load_config<std::vector<std::string>>(resolver, AssetPath(path)),
        ContainsSubstring(path) && ContainsSubstring("Expected exactly one YAML document")
    );
  }
  CHECK_THROWS_AS(load_config<TestConfig>(resolver, AssetPath("assets/absent.yaml")), ConfigError);
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/absent.yaml")),
      ContainsSubstring("assets/absent.yaml")
  );
}

TEST_CASE("Config validation rejects duplicate keys, null values, and nonfinite numbers") {
  const TempDirectory temp;
  const AssetResolver resolver(temp.path());
  write_file(
      temp.path() / "duplicate.yaml",
      "name: First\ncount: 100\ncount: 300\ndisplay:\n  image: assets/Icon.png\n"
  );
  try {
    (void)load_config<TestConfig>(resolver, AssetPath("assets/duplicate.yaml"));
    FAIL("expected duplicate error");
  } catch (const ConfigError &error) {
    CHECK(error.address() == "assets/duplicate.yaml");
    CHECK(error.key_path() == "count");
    CHECK(error.source_location().line == 2);
  }
  write_file(
      temp.path() / "null.yaml", "name: null\ncount: 1\ndisplay:\n  image: assets/Icon.png\n"
  );
  CHECK_THROWS_WITH(
      load_config<TestConfig>(resolver, AssetPath("assets/null.yaml")),
      ContainsSubstring("[name]") && ContainsSubstring("cannot be null")
  );
}

namespace {
struct ReferenceConfig {
  AssetReference image;
};
} // namespace
namespace flop::config {
template <> struct ConfigDecoder<ReferenceConfig> {
  static ReferenceConfig decode(const ConfigView &document) {
    return {document.required("display").required("image").node_reference()};
  }
};
} // namespace flop::config

TEST_CASE("Deferred asset references retain their source field") {
  const TempDirectory temp;
  const AssetResolver resolver(temp.path());
  write_file(temp.path() / "reference.yaml", "display:\n  image: assets/Absent.png\n");
  const auto config = load_config<ReferenceConfig>(resolver, AssetPath("assets/reference.yaml"));
  try {
    (void)config.image.resolve(resolver);
    FAIL("expected missing asset error");
  } catch (const ConfigError &error) {
    CHECK(error.address() == "assets/reference.yaml");
    CHECK(error.key_path() == "display.image");
    CHECK(error.what() != std::string{});
  }
}

TEST_CASE("Asset addresses reject traversal and use exact case for each component") {
  for (const auto path : {
           "assets",
           "assets/",
           "Assets/config.yaml",
           "config.yaml",
           "/assets/config.yaml",
           "C:/assets/config.yaml",
           "assets/C:/config.yaml",
           "assets/./config.yaml",
           "assets/a/../config.yaml",
           "assets//config.yaml",
           "assets/config.yaml/",
           "assets/a\\config.yaml",
       }) {
    CAPTURE(path);
    CHECK_THROWS_AS(AssetPath(path), std::invalid_argument);
  }
  CHECK_THROWS_AS(AssetPath(std::string_view("assets/a\0b", 10)), std::invalid_argument);

  const TempDirectory temp;
  const auto file = temp.path() / "Folder" / "Config.yaml";
  write_file(file, "- test\n");
  const AssetResolver resolver(temp.path());
  CHECK(fs::equivalent(resolver.resolve(AssetPath("assets/Folder/Config.yaml")), file));
  CHECK_THROWS_AS(resolver.resolve(AssetPath("assets/folder/Config.yaml")), AssetError);
  CHECK_THROWS_AS(resolver.resolve(AssetPath("assets/Folder/config.yaml")), AssetError);
  CHECK_THROWS_AS(resolver.resolve(AssetPath("assets/Folder")), AssetError);
}

TEST_CASE("Asset resolution rejects links outside the asset root") {
  const TempDirectory temp;
  const auto root = temp.path() / "assets";
  const auto outside = temp.path() / "assets-outside";
  fs::create_directory(root);
  write_file(outside / "config.yaml", "- outside\n");
  std::error_code error;
  fs::create_directory_symlink(outside, root / "External", error);
  if (error) {
    SKIP("Cannot create a directory symlink: " << error.message());
  }

  const AssetResolver resolver(root);
  CHECK_THROWS_WITH(
      resolver.resolve(AssetPath("assets/External/config.yaml")),
      ContainsSubstring("assets/External/config.yaml") &&
          ContainsSubstring("outside the asset root")
  );
}

TEST_CASE("Asset startup selects an explicit or bundled root independently of working directory") {
  const TempDirectory temp;
  const auto application_base = temp.path() / "application";
  const auto bundled = application_base / "assets" / "choice.yaml";
  const auto override_root = temp.path() / "override";
  const auto overridden = override_root / "choice.yaml";
  write_file(bundled, "- bundled\n");
  write_file(overridden, "- explicit\n");
  write_file(application_base / "assets" / "bundled_only.yaml", "- bundled\n");
  const auto unrelated = temp.path() / "unrelated";
  fs::create_directory(unrelated);
  const WorkingDirectory working_directory(unrelated);

  const AssetPath address("assets/choice.yaml");
  CHECK(fs::equivalent(AssetResolver::from_startup(application_base).resolve(address), bundled));
  const auto explicit_resolver = AssetResolver::from_startup(application_base, override_root);
  CHECK(fs::equivalent(explicit_resolver.resolve(address), overridden));
  CHECK_THROWS_AS(explicit_resolver.resolve(AssetPath("assets/bundled_only.yaml")), AssetError);
  CHECK_THROWS_AS(
      AssetResolver::from_startup(application_base, temp.path() / "missing"), AssetError
  );
  CHECK_THROWS_AS(AssetResolver::from_startup(fs::path{}), AssetError);
}
