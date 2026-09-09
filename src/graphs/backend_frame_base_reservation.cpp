#include "algorithms/graphs/backend_frame_base_reservation.hpp"

#include <cstddef>
#include <stdexcept>

namespace algorithms::graphs {
namespace {

void validate_storage_below_base(const BaseRelativeBackendStorage& storage,
                                 const std::size_t frame_base_register) {
  if (storage.kind == BaseRelativeBackendStorageKind::physical_register &&
      storage.physical_register >= frame_base_register) {
    throw std::logic_error(
        "frame-base reservation produced an overlapping physical register");
  }
}

void validate_frame_base_disjointness(
    const ScratchAwareBaseRelativeBackendFrame& frame,
    const std::size_t frame_base_register) {
  if (frame.total_registers != frame_base_register) {
    throw std::logic_error(
        "frame-base reservation non-base register count is inconsistent");
  }
  for (const std::size_t scratch : frame.scratch_physical_registers) {
    if (scratch >= frame_base_register) {
      throw std::logic_error(
          "frame-base reservation scratch register overlaps frame base");
    }
  }
  for (const BaseRelativeBackendClassStorage& item : frame.class_storage) {
    validate_storage_below_base(item.storage, frame_base_register);
  }
  for (const BaseRelativeBackendLocationStorage& item : frame.location_storage) {
    validate_storage_below_base(item.storage, frame_base_register);
  }
  for (const BaseRelativeBackendBlock& block : frame.blocks) {
    for (const BaseRelativeBackendOperation& operation : block.operations) {
      for (const BaseRelativeBackendStorage& input : operation.inputs) {
        validate_storage_below_base(input, frame_base_register);
      }
      if (operation.output.has_value()) {
        validate_storage_below_base(*operation.output, frame_base_register);
      }
    }
  }
}

}  // namespace

FrameBaseReservedBackendPlan plan_frame_base_reserved_backend(
    const OutOfSsaProgram& program, const std::size_t total_physical_registers,
    const BackendFrameLayoutConfig frame_layout_config,
    const BackendFrameAddressingConfig addressing_config) {
  FrameBaseReservedBackendPlan result;
  result.total_physical_registers = total_physical_registers;

  if (total_physical_registers == 0U) {
    result.non_base_register_plan =
        plan_scratch_aware_backend_registers(program, 0U);
    return result;
  }

  const std::size_t frame_base_register = total_physical_registers - 1U;
  result.frame_base_physical_register = frame_base_register;
  result.non_base_register_plan =
      plan_scratch_aware_backend_registers(program, frame_base_register);
  if (!result.non_base_register_plan.selection.has_value()) {
    return result;
  }

  result.byte_addressed_frame = layout_scratch_aware_backend_frame(
      *result.non_base_register_plan.selection, frame_layout_config);
  result.addressed_frame = address_scratch_aware_backend_frame(
      *result.byte_addressed_frame, addressing_config);
  validate_frame_base_disjointness(*result.addressed_frame,
                                   frame_base_register);
  return result;
}

}  // namespace algorithms::graphs
