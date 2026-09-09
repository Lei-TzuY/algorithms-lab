#include "algorithms/graphs/backend_register_reservation.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void validate_persistent_storage(const BackendStorage storage,
                                 const std::size_t allocatable_registers) {
  if (storage.kind == BackendStorageKind::physical_register) {
    if (storage.index >= allocatable_registers) {
      throw std::logic_error(
          "scratch-aware reservation assigned register crosses reserved suffix");
    }
    return;
  }
  if (storage.kind == BackendStorageKind::stack_slot) {
    return;
  }
  throw std::logic_error(
      "scratch-aware reservation found scratch in persistent storage");
}

[[nodiscard]] BackendStorage physicalize_storage(
    const BackendStorage storage, const std::size_t total_registers,
    const std::size_t allocatable_registers,
    const std::size_t reserved_scratch_registers) {
  if (storage.kind == BackendStorageKind::physical_register) {
    validate_persistent_storage(storage, allocatable_registers);
    return storage;
  }
  if (storage.kind == BackendStorageKind::stack_slot) {
    return storage;
  }
  if (storage.index >= reserved_scratch_registers) {
    throw std::logic_error(
        "scratch-aware reservation abstract scratch exceeds reserved suffix");
  }
  const std::size_t physical_register = allocatable_registers + storage.index;
  if (physical_register >= total_registers) {
    throw std::logic_error(
        "scratch-aware reservation physical scratch exceeds register file");
  }
  return {BackendStorageKind::physical_register, physical_register};
}

[[nodiscard]] std::vector<BackendLoweredBlock> physicalize_blocks(
    const PhiFreeBackendSpillLowering& lowering,
    const std::size_t total_registers,
    const std::size_t allocatable_registers,
    const std::size_t reserved_scratch_registers) {
  for (const BackendClassStorage& item : lowering.class_storage) {
    validate_persistent_storage(item.storage, allocatable_registers);
  }
  for (const BackendLocationStorage& item : lowering.location_storage) {
    validate_persistent_storage(item.storage, allocatable_registers);
  }

  std::vector<BackendLoweredBlock> result = lowering.blocks;
  for (BackendLoweredBlock& block : result) {
    for (BackendOperation& operation : block.operations) {
      for (BackendStorage& input : operation.inputs) {
        input = physicalize_storage(input, total_registers,
                                    allocatable_registers,
                                    reserved_scratch_registers);
      }
      if (operation.output.has_value()) {
        operation.output = physicalize_storage(
            *operation.output, total_registers, allocatable_registers,
            reserved_scratch_registers);
      }
    }
  }
  return result;
}

}  // namespace

ScratchAwareBackendRegisterPlan plan_scratch_aware_backend_registers(
    const OutOfSsaProgram& program, const std::size_t total_registers) {
  ScratchAwareBackendRegisterPlan result;
  result.total_registers = total_registers;

  for (std::size_t reserved = 0U;; ++reserved) {
    const std::size_t allocatable = total_registers - reserved;
    PhiFreeCoalescedRegisterAllocation allocation =
        coalesce_phi_free_registers(program, allocatable);
    PhiFreeBackendSpillLowering lowering =
        lower_phi_free_backend_storage(program, allocation);
    const bool feasible = lowering.max_scratch_registers <= reserved;

    result.attempts.push_back(BackendRegisterReservationAttempt{
        reserved, allocatable, lowering.max_scratch_registers,
        lowering.stack_slot_count, allocation.spills.size(), feasible});

    if (feasible) {
      ScratchAwareBackendRegisterSelection selection;
      selection.reserved_scratch_registers = reserved;
      selection.allocatable_registers = allocatable;
      selection.allocation = std::move(allocation);
      selection.abstract_lowering = std::move(lowering);
      selection.scratch_physical_registers.reserve(
          selection.abstract_lowering.max_scratch_registers);
      for (std::size_t scratch = 0U;
           scratch < selection.abstract_lowering.max_scratch_registers;
           ++scratch) {
        selection.scratch_physical_registers.push_back(allocatable + scratch);
      }
      selection.physicalized_blocks = physicalize_blocks(
          selection.abstract_lowering, total_registers, allocatable, reserved);
      result.selection = std::move(selection);
      break;
    }

    if (reserved == total_registers) {
      break;
    }
  }

  return result;
}

}  // namespace algorithms::graphs
