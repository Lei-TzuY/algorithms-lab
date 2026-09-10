#include "algorithms/graphs/backend_frame_entry_setup.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::graphs {
namespace {

std::int64_t positive_size_to_i64(const std::size_t value) {
  constexpr auto limit = static_cast<std::uintmax_t>(
      std::numeric_limits<std::int64_t>::max());
  if (static_cast<std::uintmax_t>(value) > limit) {
    throw std::overflow_error(
        "frame-entry positive byte displacement is not int64 representable");
  }
  return static_cast<std::int64_t>(value);
}

std::int64_t negative_size_magnitude_to_i64(const std::size_t magnitude) {
  constexpr auto positive_limit = static_cast<std::uintmax_t>(
      std::numeric_limits<std::int64_t>::max());
  constexpr auto negative_limit = positive_limit + std::uintmax_t{1U};
  const auto wide = static_cast<std::uintmax_t>(magnitude);
  if (wide > negative_limit) {
    throw std::overflow_error(
        "frame-entry negative byte displacement is not int64 representable");
  }
  if (wide == negative_limit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(magnitude);
}

std::int64_t checked_add(const std::int64_t first,
                         const std::int64_t second) {
  if (second > 0 && first > std::numeric_limits<std::int64_t>::max() - second) {
    throw std::overflow_error("frame-entry signed addition overflow");
  }
  if (second < 0 && first < std::numeric_limits<std::int64_t>::min() - second) {
    throw std::overflow_error("frame-entry signed addition overflow");
  }
  return first + second;
}

std::int64_t signed_size_difference(const std::size_t first,
                                    const std::size_t second) {
  if (first >= second) {
    return positive_size_to_i64(first - second);
  }
  return negative_size_magnitude_to_i64(second - first);
}

void validate_source_plan(const FrameBaseReservedBackendPlan& plan) {
  if (plan.total_physical_registers == 0U) {
    if (plan.frame_base_physical_register.has_value() ||
        plan.byte_addressed_frame.has_value() || plan.addressed_frame.has_value()) {
      throw std::logic_error(
          "zero-register Phase-54 plan unexpectedly owns a frame base or frame");
    }
    return;
  }

  const std::size_t expected_base = plan.total_physical_registers - 1U;
  if (!plan.frame_base_physical_register.has_value() ||
      *plan.frame_base_physical_register != expected_base) {
    throw std::logic_error("Phase-54 frame-base id is inconsistent");
  }
  if (plan.non_base_register_plan.total_registers != expected_base) {
    throw std::logic_error("Phase-54 non-base register count is inconsistent");
  }
  if (plan.byte_addressed_frame.has_value() != plan.addressed_frame.has_value()) {
    throw std::logic_error("Phase-54 frame witnesses have inconsistent presence");
  }
  if (!plan.addressed_frame.has_value()) {
    return;
  }

  const auto& bytes = *plan.byte_addressed_frame;
  const auto& addressed = *plan.addressed_frame;
  if (bytes.frame_size_bytes != addressed.frame_size_bytes ||
      bytes.total_registers != expected_base ||
      addressed.total_registers != expected_base ||
      bytes.stack_slots.size() != addressed.stack_slots.size() ||
      bytes.config != addressed.frame_layout_config) {
    throw std::logic_error("Phase-54 frame witnesses are inconsistent");
  }
  if (addressed.addressing_config.frame_base_byte_anchor >
      addressed.frame_size_bytes) {
    throw std::logic_error("Phase-54 frame base anchor lies outside the frame");
  }

  for (std::size_t index = 0U; index < addressed.stack_slots.size(); ++index) {
    const auto& byte_slot = bytes.stack_slots[index];
    const auto& slot = addressed.stack_slots[index];
    if (slot.stack_slot != byte_slot.stack_slot ||
        slot.byte_offset != byte_slot.byte_offset ||
        slot.byte_size != byte_slot.byte_size ||
        slot.byte_offset > addressed.frame_size_bytes ||
        slot.byte_size > addressed.frame_size_bytes - slot.byte_offset) {
      throw std::logic_error("Phase-54 stack-slot witnesses are inconsistent");
    }
    const std::int64_t expected = signed_size_difference(
        slot.byte_offset, addressed.addressing_config.frame_base_byte_anchor);
    if (slot.displacement != expected) {
      throw std::logic_error("Phase-54 stack-slot displacement is inconsistent");
    }
  }
}

}  // namespace

StackDirectedBackendFramePlan derive_stack_directed_backend_frame_entry(
    const FrameBaseReservedBackendPlan& frame_base_plan,
    const BackendStackGrowthDirection stack_growth_direction) {
  validate_source_plan(frame_base_plan);

  StackDirectedBackendFramePlan result;
  result.stack_growth_direction = stack_growth_direction;
  result.frame_base_plan = frame_base_plan;

  switch (stack_growth_direction) {
    case BackendStackGrowthDirection::toward_lower_addresses:
    case BackendStackGrowthDirection::toward_higher_addresses:
      break;
    default:
      throw std::invalid_argument("unknown backend stack-growth direction");
  }

  if (!frame_base_plan.addressed_frame.has_value()) {
    return result;
  }

  const auto& frame = *frame_base_plan.addressed_frame;
  const std::size_t frame_size = frame.frame_size_bytes;
  const std::size_t anchor = frame.addressing_config.frame_base_byte_anchor;

  StackDirectedBackendFrameEntrySetup setup;
  setup.stack_growth_direction = stack_growth_direction;
  setup.frame_base_physical_register =
      *frame_base_plan.frame_base_physical_register;
  setup.frame_size_bytes = frame_size;
  setup.frame_base_byte_anchor = anchor;

  if (stack_growth_direction ==
      BackendStackGrowthDirection::toward_lower_addresses) {
    setup.stack_pointer_adjustment =
        negative_size_magnitude_to_i64(frame_size);
    setup.frame_base_from_adjusted_stack_pointer =
        positive_size_to_i64(anchor);
    setup.frame_base_from_entry_stack_pointer =
        signed_size_difference(anchor, frame_size);
  } else {
    setup.stack_pointer_adjustment = positive_size_to_i64(frame_size);
    setup.frame_base_from_adjusted_stack_pointer =
        signed_size_difference(anchor, frame_size);
    setup.frame_base_from_entry_stack_pointer = positive_size_to_i64(anchor);
  }

  if (checked_add(setup.stack_pointer_adjustment,
                  setup.frame_base_from_adjusted_stack_pointer) !=
      setup.frame_base_from_entry_stack_pointer) {
    throw std::logic_error("frame-entry base coordinate composition failed");
  }

  setup.stack_slots.reserve(frame.stack_slots.size());
  for (const BaseRelativeBackendFrameSlot& slot : frame.stack_slots) {
    const std::int64_t direct_entry_displacement =
        stack_growth_direction ==
                BackendStackGrowthDirection::toward_lower_addresses
            ? signed_size_difference(slot.byte_offset, frame_size)
            : positive_size_to_i64(slot.byte_offset);
    if (checked_add(setup.frame_base_from_entry_stack_pointer,
                    slot.displacement) != direct_entry_displacement) {
      throw std::logic_error("frame-entry slot coordinate composition failed");
    }
    setup.stack_slots.push_back({slot.stack_slot, slot.byte_offset, slot.byte_size,
                                 slot.displacement, direct_entry_displacement});
  }

  result.entry_setup = std::move(setup);
  return result;
}

}  // namespace algorithms::graphs
