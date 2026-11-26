#pragma once

#include <span>
#include <stdexcept>

#include "flop/craft/craft_context.hpp"
#include "flop/craft/control.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/resources.hpp"

namespace flop::testing {

// minimal CraftContext for tests that drive one craft component without a full
// Craft. each accessor is backed by an optional held reference or value; an
// accessor whose backing was not supplied throws, so a test only wires up the
// members the component under test actually reads.
class StubCraftContext : public craft::CraftContext {
public:
  StubCraftContext() = default;

  StubCraftContext &with_parts(std::span<const craft::Part> parts) {
    parts_ = parts;
    has_parts_ = true;
    return *this;
  }
  StubCraftContext &with_mutable_parts(std::span<craft::Part> parts) {
    mutable_parts_ = parts;
    return *this;
  }
  StubCraftContext &with_control(craft::CraftControl &control) {
    control_ = &control;
    return *this;
  }
  StubCraftContext &with_resources(craft::CraftResources &resources) {
    resources_ = &resources;
    return *this;
  }
  StubCraftContext &with_newtonian(craft::CraftNewtonian &newtonian) {
    newtonian_ = &newtonian;
    return *this;
  }
  StubCraftContext &with_mass_properties(craft::MassProperties mass) {
    mass_ = mass;
    has_mass_ = true;
    return *this;
  }

  [[nodiscard]] std::span<const craft::Part> parts() const override {
    if (!has_parts_) {
      throw std::logic_error("StubCraftContext::parts() queried without parts set");
    }
    return parts_;
  }
  [[nodiscard]] std::span<craft::Part> mutable_parts() override {
    if (mutable_parts_.empty()) throw std::logic_error("StubCraftContext has no mutable parts");
    return mutable_parts_;
  }

  [[nodiscard]] const craft::CraftControl &control() const override { return require_control(); }
  [[nodiscard]] craft::CraftControl &control() override { return require_control(); }

  [[nodiscard]] const craft::CraftResources &resources() const override {
    return require_resources();
  }
  [[nodiscard]] craft::CraftResources &resources() override { return require_resources(); }

  [[nodiscard]] const craft::CraftNewtonian &newtonian() const override {
    return require_newtonian();
  }
  [[nodiscard]] craft::CraftNewtonian &newtonian() override { return require_newtonian(); }

  [[nodiscard]] craft::MassProperties mass_properties() const override {
    if (!has_mass_) {
      throw std::logic_error("StubCraftContext::mass_properties() queried without mass set");
    }
    return mass_;
  }

private:
  [[nodiscard]] craft::CraftControl &require_control() const {
    if (control_ == nullptr) {
      throw std::logic_error("StubCraftContext::control() queried without control set");
    }
    return *control_;
  }
  [[nodiscard]] craft::CraftResources &require_resources() const {
    if (resources_ == nullptr) {
      throw std::logic_error("StubCraftContext::resources() queried without resources set");
    }
    return *resources_;
  }
  [[nodiscard]] craft::CraftNewtonian &require_newtonian() const {
    if (newtonian_ == nullptr) {
      throw std::logic_error("StubCraftContext::newtonian() queried without newtonian set");
    }
    return *newtonian_;
  }

  std::span<const craft::Part> parts_{};
  std::span<craft::Part> mutable_parts_{};
  bool has_parts_{false};
  craft::CraftControl *control_{nullptr};
  craft::CraftResources *resources_{nullptr};
  craft::CraftNewtonian *newtonian_{nullptr};
  craft::MassProperties mass_{};
  bool has_mass_{false};
};

} // namespace flop::testing
