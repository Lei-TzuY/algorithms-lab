#include "algorithms/graphs/backend_spill_transfer_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

const StackPointerOwnedBackendFramePlan& phase56_plan(
    const BackendFixedFrameBytecodePlan& plan) {
  return plan.source_plan.source_plan.source_plan.source_plan;
}

const ScratchAwareBaseRelativeBackendFrame& require_canonical_addressed_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = phase56_plan(plan);
  const auto& directed = phase56.stack_directed_frame_plan;
  const auto& base_plan = directed.frame_base_plan;
  if (!base_plan.non_base_register_plan.selection.has_value() ||
      !base_plan.byte_addressed_frame.has_value() ||
      !base_plan.addressed_frame.has_value()) {
    throw std::invalid_argument(
        "spill-transfer execution requires complete successful frame provenance");
  }

  const ScratchAwareByteAddressedBackendFrame rebuilt_byte_frame =
      layout_scratch_aware_backend_frame(
          *base_plan.non_base_register_plan.selection,
          base_plan.byte_addressed_frame->config);
  if (rebuilt_byte_frame != *base_plan.byte_addressed_frame) {
    throw std::logic_error(
        "spill-transfer Phase-52 byte frame disagrees with retained selection");
  }

  const ScratchAwareBaseRelativeBackendFrame rebuilt_addressed_frame =
      address_scratch_aware_backend_frame(
          rebuilt_byte_frame, base_plan.addressed_frame->addressing_config);
  if (rebuilt_addressed_frame != *base_plan.addressed_frame) {
    throw std::logic_error(
        "spill-transfer Phase-53 addressed body is not canonical");
  }

  const StackDirectedBackendFramePlan rebuilt_directed =
      derive_stack_directed_backend_frame_entry(
          base_plan, directed.stack_growth_direction);
  if (rebuilt_directed != directed) {
    throw std::logic_error(
        "spill-transfer Phase-55 frame-entry provenance is not canonical");
  }
  return *base_plan.addressed_frame;
}

struct ValidatedStorageDomain {
  std::map<std::int64_t, std::size_t> displacement_to_slot;
  std::size_t planned_register_count{0U};
  std::size_t body_register_limit{0U};
};

ValidatedStorageDomain validate_storage_domain(
    const BackendFixedFrameBytecodePlan& plan,
    const ScratchAwareBaseRelativeBackendFrame& frame,
    const std::size_t register_count, const std::size_t frame_value_count) {
  if (frame_value_count != frame.stack_slots.size()) {
    throw std::invalid_argument(
        "spill-transfer frame state must contain one value per stack slot");
  }

  ValidatedStorageDomain domain;
  const auto& phase56 = phase56_plan(plan);
  domain.planned_register_count = phase56.total_physical_registers;
  const auto& frame_base =
      phase56.stack_directed_frame_plan.frame_base_plan
          .frame_base_physical_register;
  if (!frame_base.has_value()) {
    throw std::logic_error(
        "spill-transfer addressed frame is missing frame-base ownership");
  }
  domain.body_register_limit = *frame_base;

  std::vector<bool> seen_slot(frame.stack_slots.size(), false);
  for (const auto& slot : frame.stack_slots) {
    if (slot.stack_slot >= seen_slot.size() || seen_slot[slot.stack_slot]) {
      throw std::logic_error(
          "spill-transfer execution requires dense unique stack-slot ids");
    }
    seen_slot[slot.stack_slot] = true;
    const auto [position, inserted] =
        domain.displacement_to_slot.emplace(slot.displacement, slot.stack_slot);
    static_cast<void>(position);
    if (!inserted) {
      throw std::logic_error(
          "spill-transfer execution requires unique frame displacements");
    }
  }

  if (register_count < domain.planned_register_count) {
    throw std::out_of_range(
        "spill-transfer register file is smaller than planned register budget");
  }
  return domain;
}

void validate_physical_register(const BaseRelativeBackendStorage& storage,
                                const ValidatedStorageDomain& domain,
                                const std::size_t register_count) {
  if (storage.kind != BaseRelativeBackendStorageKind::physical_register) {
    throw std::logic_error("spill-transfer expected physical-register storage");
  }
  if (storage.physical_register >= domain.planned_register_count ||
      storage.physical_register >= register_count) {
    throw std::out_of_range(
        "spill-transfer operation references register outside finite file");
  }
  if (storage.physical_register >= domain.body_register_limit) {
    throw std::logic_error(
        "spill-transfer body register overlaps reserved frame/SP ownership");
  }
}

std::size_t validate_frame_slot(const BaseRelativeBackendStorage& storage,
                                const ValidatedStorageDomain& domain) {
  if (storage.kind != BaseRelativeBackendStorageKind::frame_base_displacement) {
    throw std::logic_error("spill-transfer expected frame storage");
  }
  const auto found = domain.displacement_to_slot.find(storage.displacement);
  if (found == domain.displacement_to_slot.end()) {
    throw std::out_of_range(
        "spill-transfer operation references unknown frame displacement");
  }
  return found->second;
}

void validate_transfer_operation(const BaseRelativeBackendOperation& operation,
                                 const ValidatedStorageDomain& domain,
                                 const std::size_t register_count) {
  if (operation.kind == BackendOperationKind::instruction) {
    for (const auto& input : operation.inputs) {
      validate_physical_register(input, domain, register_count);
    }
    if (operation.output.has_value()) {
      validate_physical_register(*operation.output, domain, register_count);
    }
    return;
  }

  if (operation.inputs.size() != 1U || !operation.output.has_value()) {
    throw std::logic_error(
        "spill-transfer operation must have exactly one input and one output");
  }

  const auto& input = operation.inputs.front();
  const auto& output = *operation.output;
  switch (operation.kind) {
    case BackendOperationKind::register_move:
      validate_physical_register(input, domain, register_count);
      validate_physical_register(output, domain, register_count);
      return;
    case BackendOperationKind::stack_reload:
      static_cast<void>(validate_frame_slot(input, domain));
      validate_physical_register(output, domain, register_count);
      return;
    case BackendOperationKind::stack_store:
      validate_physical_register(input, domain, register_count);
      static_cast<void>(validate_frame_slot(output, domain));
      return;
    case BackendOperationKind::instruction:
      return;
  }
  throw std::logic_error("spill-transfer operation has unknown kind");
}

std::int64_t read_transfer_input(
    const BaseRelativeBackendOperation& operation,
    const ValidatedStorageDomain& domain,
    const std::vector<std::int64_t>& registers,
    const std::vector<std::int64_t>& frame_values) {
  const auto& input = operation.inputs.front();
  if (operation.kind == BackendOperationKind::stack_reload) {
    return frame_values[validate_frame_slot(input, domain)];
  }
  return registers[input.physical_register];
}

void write_transfer_output(const BaseRelativeBackendOperation& operation,
                           const ValidatedStorageDomain& domain,
                           const std::int64_t value,
                           std::vector<std::int64_t>& registers,
                           std::vector<std::int64_t>& frame_values) {
  const auto& output = *operation.output;
  if (operation.kind == BackendOperationKind::stack_store) {
    frame_values[validate_frame_slot(output, domain)] = value;
    return;
  }
  registers[output.physical_register] = value;
}

}  // namespace

BackendSpillTransferBlockExecution execute_backend_spill_transfer_block(
    const BackendFixedFrameBytecodePlan& plan, const std::size_t block_index,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values) {
  // Reuse the sealed Phase-61 trust boundary for strict bytecode decoding,
  // canonical entry/exit identity, finite register preflight, and checked
  // fixed-frame arithmetic.
  const BackendFixedFrameBytecodeExecutionSnapshots fixed_frame =
      execute_backend_fixed_frame_bytecode(plan, initial_registers);

  // Phase 61 does not independently rebuild the nested addressed body. Phase 62
  // does, because it consumes that body as executable storage-transfer input.
  const auto& frame = require_canonical_addressed_frame(plan);
  if (block_index >= frame.blocks.size()) {
    throw std::out_of_range("spill-transfer block index out of range");
  }
  const auto& block = frame.blocks[block_index];
  if (!block.reachable) {
    throw std::invalid_argument(
        "spill-transfer execution requires a reachable backend block");
  }

  const ValidatedStorageDomain domain = validate_storage_domain(
      plan, frame, initial_registers.size(), initial_frame_slot_values.size());

  // Preflight the complete selected block before transfer mutation. Even
  // operations after an opaque barrier must be structurally valid.
  for (const auto& operation : block.operations) {
    validate_transfer_operation(operation, domain, initial_registers.size());
  }

  BackendSpillTransferBlockExecution result;
  result.block_index = block_index;
  result.after_entry_registers = fixed_frame.after_entry_registers;
  result.after_block_registers = fixed_frame.after_entry_registers;
  result.frame_slot_values.assign(initial_frame_slot_values.begin(),
                                  initial_frame_slot_values.end());
  result.steps.reserve(block.operations.size());

  for (std::size_t index = 0U; index < block.operations.size(); ++index) {
    const auto& operation = block.operations[index];
    if (operation.kind == BackendOperationKind::instruction) {
      result.steps.push_back(BackendSpillTransferExecutionStep{
          index, operation.kind, operation.origin_kind, operation.origin_index,
          std::nullopt});
      result.suspended_at_instruction = index;
      break;
    }

    const std::int64_t value =
        read_transfer_input(operation, domain, result.after_block_registers,
                            result.frame_slot_values);
    write_transfer_output(operation, domain, value, result.after_block_registers,
                          result.frame_slot_values);
    result.steps.push_back(BackendSpillTransferExecutionStep{
        index, operation.kind, operation.origin_kind, operation.origin_index,
        value});
  }

  return result;
}

}  // namespace algorithms::graphs
