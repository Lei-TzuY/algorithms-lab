#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"

#include <cstddef>
#include <stdexcept>

namespace algorithms::graphs {
namespace {

void validate_storage_below_frame_base(
    const BaseRelativeBackendStorage& storage,
    const std::size_t frame_base_physical_register) {
  if (storage.kind == BaseRelativeBackendStorageKind::physical_register &&
      storage.physical_register >= frame_base_physical_register) {
    throw std::logic_error(
        "stack-pointer ownership overlaps frame-base-owned register space");
  }
}

void validate_successful_register_ownership(
    const StackDirectedBackendFramePlan& nested,
    const std::size_t stack_pointer_physical_register) {
  const FrameBaseReservedBackendPlan& base_plan = nested.frame_base_plan;
  if (base_plan.total_physical_registers !=
      stack_pointer_physical_register) {
    throw std::logic_error(
        "stack-pointer ownership nested register count is inconsistent");
  }

  if (stack_pointer_physical_register == 0U) {
    if (base_plan.frame_base_physical_register.has_value() ||
        base_plan.addressed_frame.has_value() || nested.entry_setup.has_value()) {
      throw std::logic_error(
          "single-register stack-pointer ownership unexpectedly produced a frame");
    }
    return;
  }

  const std::size_t expected_frame_base =
      stack_pointer_physical_register - 1U;
  if (!base_plan.frame_base_physical_register.has_value() ||
      *base_plan.frame_base_physical_register != expected_frame_base) {
    throw std::logic_error(
        "stack-pointer ownership frame-base register is inconsistent");
  }
  if (expected_frame_base >= stack_pointer_physical_register) {
    throw std::logic_error(
        "stack-pointer ownership does not place frame base below stack pointer");
  }

  if (!base_plan.addressed_frame.has_value()) {
    if (nested.entry_setup.has_value()) {
      throw std::logic_error(
          "stack-pointer ownership produced entry setup without addressed frame");
    }
    return;
  }
  if (!nested.entry_setup.has_value() ||
      nested.entry_setup->frame_base_physical_register != expected_frame_base) {
    throw std::logic_error(
        "stack-pointer ownership entry setup frame-base register is inconsistent");
  }

  const ScratchAwareBaseRelativeBackendFrame& frame =
      *base_plan.addressed_frame;
  if (frame.total_registers != expected_frame_base) {
    throw std::logic_error(
        "stack-pointer ownership addressed-frame register count is inconsistent");
  }
  for (const std::size_t scratch : frame.scratch_physical_registers) {
    if (scratch >= expected_frame_base) {
      throw std::logic_error(
          "stack-pointer ownership scratch register overlaps reserved registers");
    }
  }
  for (const BaseRelativeBackendClassStorage& item : frame.class_storage) {
    validate_storage_below_frame_base(item.storage, expected_frame_base);
  }
  for (const BaseRelativeBackendLocationStorage& item : frame.location_storage) {
    validate_storage_below_frame_base(item.storage, expected_frame_base);
  }
  for (const BaseRelativeBackendBlock& block : frame.blocks) {
    for (const BaseRelativeBackendOperation& operation : block.operations) {
      for (const BaseRelativeBackendStorage& input : operation.inputs) {
        validate_storage_below_frame_base(input, expected_frame_base);
      }
      if (operation.output.has_value()) {
        validate_storage_below_frame_base(*operation.output,
                                          expected_frame_base);
      }
    }
  }
}

}  // namespace

StackPointerOwnedBackendFramePlan plan_stack_pointer_owned_backend_frame(
    const OutOfSsaProgram& program,
    const std::size_t total_physical_registers,
    const BackendFrameLayoutConfig frame_layout_config,
    const BackendFrameAddressingConfig addressing_config,
    const BackendStackGrowthDirection stack_growth_direction) {
  StackPointerOwnedBackendFramePlan result;
  result.total_physical_registers = total_physical_registers;

  const std::size_t nested_registers =
      total_physical_registers == 0U ? 0U : total_physical_registers - 1U;
  if (total_physical_registers != 0U) {
    result.stack_pointer_physical_register = total_physical_registers - 1U;
  }

  const FrameBaseReservedBackendPlan base_plan =
      plan_frame_base_reserved_backend(program, nested_registers,
                                       frame_layout_config, addressing_config);
  result.stack_directed_frame_plan = derive_stack_directed_backend_frame_entry(
      base_plan, stack_growth_direction);

  if (total_physical_registers == 0U) {
    if (result.stack_pointer_physical_register.has_value() ||
        result.stack_directed_frame_plan.frame_base_plan.total_physical_registers !=
            0U ||
        result.stack_directed_frame_plan.frame_base_plan
            .frame_base_physical_register.has_value() ||
        result.stack_directed_frame_plan.entry_setup.has_value()) {
      throw std::logic_error(
          "zero-register stack-pointer ownership produced reserved state");
    }
    return result;
  }

  validate_successful_register_ownership(
      result.stack_directed_frame_plan,
      *result.stack_pointer_physical_register);
  return result;
}

}  // namespace algorithms::graphs
