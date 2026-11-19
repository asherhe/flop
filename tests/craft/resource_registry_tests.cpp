#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/resource_registry.hpp"

namespace {
using Catch::Matchers::ContainsSubstring;
using flop::craft::ResourceFlow;
using flop::craft::ResourceRegistry;
using flop::craft::ResourceTypeConfig;
} // namespace

TEST_CASE("Resource registry owns resource types and exposes read-only lookup") {
  std::vector<ResourceTypeConfig> configs{{"NTO", "N2O4", 1.0, ResourceFlow::stage}};
  const ResourceRegistry registry(configs);
  configs.front().name = "changed";
  CHECK(registry.at("NTO").name == "N2O4");
  CHECK(registry.at("NTO").kg_per_unit == 1.0);
  CHECK(registry.at("NTO").flow == ResourceFlow::stage);
  CHECK(registry.resources().size() == 1);
  CHECK_THROWS_WITH(registry.at("missing"), ContainsSubstring("Unknown resource ID"));
}

TEST_CASE("Resource registry rejects duplicate IDs and invalid mass conversion") {
  CHECK_THROWS_WITH(
      ResourceRegistry(
          {{"NTO", "N2O4", 1, ResourceFlow::stage}, {"NTO", "duplicate", 1, ResourceFlow::none}}
      ),
      ContainsSubstring("Duplicate resource ID")
  );
  CHECK_THROWS_AS(
      ResourceRegistry({{"NTO", "N2O4", 0, ResourceFlow::stage}}), std::invalid_argument
  );
}
