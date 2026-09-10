#include "algorithms/graphs/backend_frame_actions.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace algorithms::graphs {
namespace {

BackendStackPointerMoveDirection entry_move_direction(
    const BackendStackGrowthDirection direction) {
  switch (direction) {
    case BackendStackGrowthDirection::toward_lower_addresses:
      return BackendStackPointerMoveDirection::toward_lower_addresses;
    case BackendStackGrowthDirection::toward_higher_addresses:
      return BackendStackPointerMoveDirection::toward_higher_addresses;
    default:
      throw std::invalid_argument("unknown backend stack-growth direction");
  }
}

BackendStackPointerMoveDirection opposite_move_direction(
    const BackendStackPointerMoveDirection direction) {
  switch (direction) {
    case BackendStackPointerMoveDirection::toward_lower_addresses:
      return BackendStackPointerMoveDirection::toward_higher_addresses;
    case BackendStackPointerMoveDirection::toward_higher_addresses:
      return BackendStackPointerMoveDirection::toward_lower_addresses;
    default:
      throw std::invalid_argument("unknown backend stack-pointer move direction");
  }
}

std::size_t negative_i64_magnitude_to_size(const std::int64_t value) {
  if (value >= 0) {
    throw std::logic_error("expected negative frame-entry stack adjustment");
  }
  const std::uint64_t magnitude =
      value == std::numeric_limits<std::int64_t>::min()
          ? std::uint64_t{1U} << 63U
          : static_cast<std::uint64_t>(-value);
  if (magnitude > static_cast<std::uintmax_t>(
                      std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error(
        "frame action stack-adjustment magnitude is not size_t representable");
  }
  return static_cast<std::size_t>(magnitude);
}

std::size_t positive_i64_to_size(const std::int64_t value) {
  if (value < 0) {
    throw std::logic_error("expected nonnegative frame-entry stack adjustment");
  }
  const auto magnitude = static_cast<std::uint64_t>(value);
  if (magnitude > static_cast<std::uintmax_t>(
                      std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error(
        "frame action stack-adjustment magnitude is not size_t representable");
  }
  return static_cast<std::size_t>(magnitude);
}

std::size_t adjustment_magnitude(
    const StackDirectedBackendFrameEntrySetup& setup) {
  if (setup.frame_size_bytes == 0U) {
    if (setup.stack_pointer_adjustment != 0) {
      throw std::logic_error(
          "zero-sized frame has nonzero stack-pointer adjustment");
    }
    return 0U;
  }

  std::size_t magnitude = 0U;
  switch (setup.stack_growth_direction) {
    case BackendStackGrowthDirection::toward_lower_addresses:
      magnitude = negative_i64_magnitude_to_size(setup.stack_pointer_adjustment);
      break;
    case BackendStackGrowthDirection::toward_higher_addresses:
      magnitude = positive_i64_to_size(setup.stack_pointer_adjustment);
      break;
    default:
      throw std::invalid_argument("unknown backend stack-growth direction");
  }
  if (magnitude != setup.frame_size_bytes) {
    throw std::logic_error(
        "frame action stack adjustment disagrees with frame size");
  }
  return magnitude;
}

void validate_source_plan(const StackPointerOwnedBackendFramePlan& source) {
  const auto& directed = source.stack_directed_frame_plan;
  const auto& base = directed.frame_base_plan;

  switch (directed.stack_growth_direction) {
    case BackendStackGrowthDirection::toward_lower_addresses:
    case BackendStackGrowthDirection::toward_higher_addresses:
      break;
    default:
      throw std::invalid_argument("unknown backend stack-growth direction");
  }

  if (source.total_physical_registers == 0U) {
    if (source.stack_pointer_physical_register.has_value() ||
        base.total_physical_registers != 0U ||
        base.frame_base_physical_register.has_value() ||
        base.addressed_frame.has_value() || directed.entry_setup.has_value()) {
      throw std::logic_error(
          "zero-register Phase-56 witness contains reserved frame state");
    }
    return;
  }

  const std::size_t expected_stack_pointer =
      source.total_physical_registers - 1U;
  if (!source.stack_pointer_physical_register.has_value() ||
      *source.stack_pointer_physical_register != expected_stack_pointer ||
      base.total_physical_registers != expected_stack_pointer) {
    throw std::logic_error("Phase-56 stack-pointer ownership is inconsistent");
  }

  if (expected_stack_pointer == 0U) {
    if (base.frame_base_physical_register.has_value() ||
        base.addressed_frame.has_value() || directed.entry_setup.has_value()) {
      throw std::logic_error(
          "single-register Phase-56 witness unexpectedly owns a frame base");
    }
    return;
  }

  const std::size_t expected_frame_base = expected_stack_pointer - 1U;
  if (!base.frame_base_physical_register.has_value() ||
      *base.frame_base_physical_register != expected_frame_base) {
    throw std::logic_error("Phase-56 frame-base ownership is inconsistent");
  }

  if (!directed.entry_setup.has_value()) {
    if (base.addressed_frame.has_value()) {
      throw std::logic_error(
          "Phase-56 addressed frame is missing its entry-coordinate witness");
    }
    return;
  }

  if (!base.addressed_frame.has_value() ||
      !base.frame_base_physical_register.has_value()) {
    throw std::logic_error(
        "Phase-56 entry-coordinate witness is missing addressed-frame provenance");
  }
  if (*base.frame_base_physical_register >= expected_stack_pointer) {
    throw std::logic_error(
        "Phase-56 frame-base register overlaps stack-pointer ownership");
  }

  const auto& setup = *directed.entry_setup;
  const auto& frame = *base.addressed_frame;
  if (setup.stack_growth_direction != directed.stack_growth_direction ||
      setup.frame_base_physical_register != *base.frame_base_physical_register ||
      setup.frame_size_bytes != frame.frame_size_bytes ||
      setup.frame_base_byte_anchor !=
          frame.addressing_config.frame_base_byte_anchor) {
    throw std::logic_error(
        "Phase-56 entry-coordinate witness disagrees with addressed frame");
  }
  static_cast<void>(adjustment_magnitude(setup));
}

}  // namespace

BackendFixedFrameActionPlan derive_fixed_frame_actions(
    const StackPointerOwnedBackendFramePlan& source_plan) {
  validate_source_plan(source_plan);

  BackendFixedFrameActionPlan result;
  result.source_plan = source_plan;
  const auto& directed = source_plan.stack_directed_frame_plan;
  if (!directed.entry_setup.has_value()) {
    return result;
  }

  const auto& setup = *directed.entry_setup;
  const std::size_t stack_pointer =
      *source_plan.stack_pointer_physical_register;
  const std::size_t frame_base = setup.frame_base_physical_register;
  const std::size_t bytes = adjustment_magnitude(setup);
  const BackendStackPointerMoveDirection entry_direction =
      entry_move_direction(setup.stack_growth_direction);

  result.entry_actions.push_back(BackendStackPointerAdjustmentAction{
      stack_pointer, entry_direction, bytes});
  result.entry_actions.push_back(BackendFrameBaseMaterializationAction{
      frame_base, stack_pointer,
      setup.frame_base_from_adjusted_stack_pointer});
  result.exit_actions.push_back(BackendStackPointerAdjustmentAction{
      stack_pointer, opposite_move_direction(entry_direction), bytes});
  return result;
}

}  // namespace algorithms::graphs
