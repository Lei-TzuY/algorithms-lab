#include "algorithms/graphs/backend_instruction_continuation.hpp"
#include "algorithms/graphs/ssa_scalar_semantics.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

const ScratchAwareBaseRelativeBackendFrame& addressed_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  return *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
}

std::map<std::int64_t, std::size_t> displacement_to_slot(
    const ScratchAwareBaseRelativeBackendFrame& frame) {
  std::map<std::int64_t, std::size_t> result;
  for (const auto& slot : frame.stack_slots) {
    const auto [position, inserted] =
        result.emplace(slot.displacement, slot.stack_slot);
    static_cast<void>(position);
    if (!inserted) {
      throw std::logic_error(
          "instruction continuation requires unique frame displacements");
    }
  }
  return result;
}

std::size_t slot_for_storage(
    const BaseRelativeBackendStorage& storage,
    const std::map<std::int64_t, std::size_t>& slots) {
  if (storage.kind != BaseRelativeBackendStorageKind::frame_base_displacement) {
    throw std::logic_error(
        "instruction continuation expected frame-displacement storage");
  }
  const auto found = slots.find(storage.displacement);
  if (found == slots.end()) {
    throw std::logic_error(
        "instruction continuation could not resolve validated frame storage");
  }
  return found->second;
}

std::int64_t transfer_value(
    const BaseRelativeBackendOperation& operation,
    const std::map<std::int64_t, std::size_t>& slots,
    const std::vector<std::int64_t>& registers,
    const std::vector<std::int64_t>& frame_values) {
  if (operation.kind == BackendOperationKind::stack_reload) {
    return frame_values[slot_for_storage(operation.inputs.front(), slots)];
  }
  return registers[operation.inputs.front().physical_register];
}

void write_transfer(
    const BaseRelativeBackendOperation& operation,
    const std::map<std::int64_t, std::size_t>& slots,
    const std::int64_t value, std::vector<std::int64_t>& registers,
    std::vector<std::int64_t>& frame_values) {
  if (operation.kind == BackendOperationKind::stack_store) {
    frame_values[slot_for_storage(*operation.output, slots)] = value;
    return;
  }
  registers[operation.output->physical_register] = value;
}

[[nodiscard]] SsaInstruction semantic_instruction_shape(
    const BaseRelativeBackendOperation& operation) {
  SsaInstruction instruction;
  instruction.uses.assign(operation.inputs.size(), SsaValue{0U, 0U});
  if (operation.output.has_value()) {
    instruction.definition = SsaValue{0U, 0U};
  }
  instruction.semantics = operation.instruction_semantics;
  return instruction;
}

[[nodiscard]] bool has_known_scalar_semantics(
    const BaseRelativeBackendOperation& operation) {
  const SsaInstruction instruction = semantic_instruction_shape(operation);
  return ssa_instruction_has_executable_scalar_semantics(instruction);
}

std::vector<std::size_t> validate_semantics_and_opaque_indices(
    const BaseRelativeBackendBlock& block) {
  std::vector<std::size_t> opaque_indices;
  for (std::size_t index = 0U; index < block.operations.size(); ++index) {
    const auto& operation = block.operations[index];
    if (operation.kind != BackendOperationKind::instruction) {
      if (operation.instruction_semantics != SsaInstructionSemantics{}) {
        throw std::logic_error(
            "instruction continuation transfer carries scalar semantics");
      }
      continue;
    }
    if (!has_known_scalar_semantics(operation)) {
      opaque_indices.push_back(index);
    }
  }
  return opaque_indices;
}

void validate_reply_prefix(
    const BaseRelativeBackendBlock& block,
    const std::vector<std::size_t>& opaque_indices,
    const std::span<const BackendInstructionOracleReply> replies) {
  if (replies.size() > opaque_indices.size()) {
    throw std::invalid_argument(
        "instruction continuation script has extra opaque replies");
  }
  for (std::size_t index = 0U; index < replies.size(); ++index) {
    const std::size_t operation_index = opaque_indices[index];
    const auto& operation = block.operations[operation_index];
    const auto& reply = replies[index];
    if (reply.operation_index != operation_index) {
      throw std::invalid_argument(
          "instruction continuation reply does not match opaque instruction order");
    }
    if (operation.output.has_value() != reply.output_value.has_value()) {
      throw std::invalid_argument(
          "instruction continuation opaque reply has wrong output shape");
    }
  }
}

BackendInstructionContinuationStep transfer_step(
    const BackendSpillTransferExecutionStep& step) {
  BackendInstructionContinuationStep result;
  result.operation_index = step.operation_index;
  result.kind = step.kind;
  result.origin_kind = step.origin_kind;
  result.origin_index = step.origin_index;
  result.transferred_value = step.transferred_value;
  return result;
}

}  // namespace

BackendInstructionContinuationExecution
execute_backend_instruction_continuation_block(
    const BackendFixedFrameBytecodePlan& plan, const std::size_t block_index,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values,
    const std::span<const BackendInstructionOracleReply> instruction_replies) {
  const BackendSpillTransferBlockExecution prefix =
      execute_backend_spill_transfer_block(plan, block_index, initial_registers,
                                           initial_frame_slot_values);

  const auto& frame = addressed_frame(plan);
  const auto& block = frame.blocks[block_index];
  const std::vector<std::size_t> opaque_indices =
      validate_semantics_and_opaque_indices(block);
  validate_reply_prefix(block, opaque_indices, instruction_replies);
  const auto slots = displacement_to_slot(frame);

  BackendInstructionContinuationExecution result;
  result.block_index = block_index;
  result.after_entry_registers = prefix.after_entry_registers;
  result.after_block_registers = prefix.after_block_registers;
  result.frame_slot_values = prefix.frame_slot_values;
  result.steps.reserve(block.operations.size());

  for (const auto& step : prefix.steps) {
    if (step.kind == BackendOperationKind::instruction) {
      break;
    }
    result.steps.push_back(transfer_step(step));
  }

  if (!prefix.suspended_at_instruction.has_value()) {
    return result;
  }

  std::size_t reply_index = 0U;
  for (std::size_t index = *prefix.suspended_at_instruction;
       index < block.operations.size(); ++index) {
    const auto& operation = block.operations[index];
    if (operation.kind == BackendOperationKind::instruction) {
      BackendInstructionContinuationStep step;
      step.operation_index = index;
      step.kind = operation.kind;
      step.origin_kind = operation.origin_kind;
      step.origin_index = operation.origin_index;
      step.instruction_semantics = operation.instruction_semantics;
      step.instruction_inputs.reserve(operation.inputs.size());
      for (const auto& input : operation.inputs) {
        step.instruction_inputs.push_back(
            result.after_block_registers[input.physical_register]);
      }

      const SsaInstruction semantic_shape = semantic_instruction_shape(operation);
      if (ssa_instruction_has_executable_scalar_semantics(semantic_shape)) {
        const std::int64_t derived = evaluate_ssa_scalar_instruction(
            semantic_shape,
            std::span<const std::int64_t>{step.instruction_inputs});
        result.after_block_registers[operation.output->physical_register] = derived;
        step.instruction_semantically_evaluated = true;
        step.derived_instruction_result = derived;
        result.steps.push_back(std::move(step));
        continue;
      }

      if (reply_index >= instruction_replies.size()) {
        result.steps.push_back(std::move(step));
        result.suspended_at_instruction = index;
        break;
      }

      const auto& reply = instruction_replies[reply_index];
      step.instruction_acknowledged = true;
      step.supplied_instruction_result = reply.output_value;
      if (operation.output.has_value()) {
        result.after_block_registers[operation.output->physical_register] =
            *reply.output_value;
      }
      result.steps.push_back(std::move(step));
      ++reply_index;
      result.consumed_instruction_replies = reply_index;
      continue;
    }

    const std::int64_t value =
        transfer_value(operation, slots, result.after_block_registers,
                       result.frame_slot_values);
    write_transfer(operation, slots, value, result.after_block_registers,
                   result.frame_slot_values);

    BackendInstructionContinuationStep step;
    step.operation_index = index;
    step.kind = operation.kind;
    step.origin_kind = operation.origin_kind;
    step.origin_index = operation.origin_index;
    step.transferred_value = value;
    result.steps.push_back(std::move(step));
  }

  return result;
}

}  // namespace algorithms::graphs
