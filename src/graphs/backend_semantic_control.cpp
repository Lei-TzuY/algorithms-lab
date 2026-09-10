#include "algorithms/graphs/backend_semantic_control.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace algorithms::graphs {
namespace {

const StackPointerOwnedBackendFramePlan& phase56_plan(
    const BackendFixedFrameBytecodePlan& plan) {
  return plan.source_plan.source_plan.source_plan.source_plan;
}

const ScratchAwareBackendRegisterSelection& require_program_binding(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program) {
  if (!program.graph.directed()) {
    throw std::invalid_argument(
        "semantic control execution requires a directed out-of-SSA graph");
  }
  if (program.graph.vertex_count() == 0U ||
      program.blocks.size() != program.graph.vertex_count() ||
      program.start >= program.graph.vertex_count()) {
    throw std::invalid_argument(
        "semantic control execution received malformed out-of-SSA control flow");
  }

  const auto& phase56 = phase56_plan(plan);
  const auto& directed = phase56.stack_directed_frame_plan;
  const auto& base = directed.frame_base_plan;
  if (!phase56.stack_pointer_physical_register.has_value() ||
      !base.frame_base_physical_register.has_value() ||
      !base.byte_addressed_frame.has_value() ||
      !base.addressed_frame.has_value() ||
      !base.non_base_register_plan.selection.has_value()) {
    throw std::invalid_argument(
        "semantic control execution requires complete backend provenance");
  }

  const StackPointerOwnedBackendFramePlan rebuilt =
      plan_stack_pointer_owned_backend_frame(
          program, phase56.total_physical_registers,
          base.byte_addressed_frame->config,
          base.addressed_frame->addressing_config,
          directed.stack_growth_direction);
  if (rebuilt != phase56) {
    throw std::invalid_argument(
        "semantic control program does not reproduce backend provenance");
  }

  const auto& selection = *base.non_base_register_plan.selection;
  if (selection.abstract_lowering.blocks.size() != program.blocks.size() ||
      selection.physicalized_blocks.size() != program.blocks.size()) {
    throw std::logic_error(
        "semantic control backend block domain disagrees with bound program");
  }
  for (std::size_t block = 0U; block < program.blocks.size(); ++block) {
    if (selection.abstract_lowering.blocks[block].control !=
        selection.physicalized_blocks[block].control) {
      throw std::logic_error(
          "semantic control physicalization changed control provenance");
    }
  }
  return selection;
}

[[nodiscard]] bool has_cfg_edge(const Graph& graph, const Vertex from,
                                const Vertex to) {
  if (from >= graph.vertex_count() || to >= graph.vertex_count()) {
    return false;
  }
  const auto& neighbors = graph.neighbors(from);
  return std::any_of(neighbors.begin(), neighbors.end(),
                     [to](const Edge& edge) { return edge.to == to; });
}

[[nodiscard]] std::int64_t read_predicate(
    const BackendStorage storage,
    const BackendInstructionContinuationExecution& execution) {
  switch (storage.kind) {
    case BackendStorageKind::physical_register:
      if (storage.index >= execution.after_block_registers.size()) {
        throw std::logic_error(
            "semantic control predicate register is outside register file");
      }
      return execution.after_block_registers[storage.index];
    case BackendStorageKind::stack_slot:
      if (storage.index >= execution.frame_slot_values.size()) {
        throw std::logic_error(
            "semantic control predicate stack slot is outside frame");
      }
      return execution.frame_slot_values[storage.index];
    case BackendStorageKind::spill_scratch_register:
      throw std::logic_error(
          "semantic control predicate cannot use transient scratch storage");
  }
  throw std::logic_error("semantic control predicate has unknown storage kind");
}

void validate_target(const OutOfSsaProgram& program, const Vertex block,
                     const SsaLoweredControlTarget& target) {
  if (target.logical_successor >= program.graph.vertex_count() ||
      !has_cfg_edge(program.graph, block, target.execution_successor)) {
    throw std::logic_error(
        "semantic control target disagrees with lowered execution CFG");
  }
}

}  // namespace

BackendSemanticControlExecution execute_backend_semantic_control_block(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    const Vertex block_index,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values,
    const std::span<const BackendInstructionOracleReply> instruction_replies) {
  const ScratchAwareBackendRegisterSelection& selection =
      require_program_binding(plan, program);
  if (block_index >= program.blocks.size()) {
    throw std::out_of_range("semantic control block index out of range");
  }
  if (!program.blocks[block_index].reachable) {
    throw std::invalid_argument(
        "semantic control cannot execute an unreachable block");
  }

  const BackendControlTerminator& control =
      selection.abstract_lowering.blocks[block_index].control;
  BackendSemanticControlExecution result;
  result.control_kind = control.kind;
  result.block_execution = execute_backend_instruction_continuation_block(
      plan, block_index, initial_registers, initial_frame_slot_values,
      instruction_replies);

  if (result.block_execution.suspended_at_instruction.has_value()) {
    result.status = BackendSemanticControlStatus::body_suspended;
    return result;
  }

  switch (control.kind) {
    case SsaControlTerminatorKind::opaque:
      if (control.predicate_storage.has_value() || control.jump_target.has_value() ||
          control.nonzero_target.has_value() || control.zero_target.has_value()) {
        throw std::logic_error("semantic control opaque descriptor carries payload");
      }
      result.status = BackendSemanticControlStatus::opaque_control;
      return result;
    case SsaControlTerminatorKind::jump:
      if (control.predicate_storage.has_value() || !control.jump_target.has_value() ||
          control.nonzero_target.has_value() || control.zero_target.has_value()) {
        throw std::logic_error("semantic control jump descriptor is malformed");
      }
      validate_target(program, block_index, *control.jump_target);
      result.status = BackendSemanticControlStatus::successor_selected;
      result.logical_successor = control.jump_target->logical_successor;
      result.execution_successor = control.jump_target->execution_successor;
      return result;
    case SsaControlTerminatorKind::branch_if_nonzero: {
      if (!control.predicate_storage.has_value() || control.jump_target.has_value() ||
          !control.nonzero_target.has_value() || !control.zero_target.has_value()) {
        throw std::logic_error(
            "semantic control conditional descriptor is malformed");
      }
      validate_target(program, block_index, *control.nonzero_target);
      validate_target(program, block_index, *control.zero_target);
      const std::int64_t predicate =
          read_predicate(*control.predicate_storage, result.block_execution);
      result.predicate_value = predicate;
      const SsaLoweredControlTarget& target =
          predicate != 0 ? *control.nonzero_target : *control.zero_target;
      result.status = BackendSemanticControlStatus::successor_selected;
      result.logical_successor = target.logical_successor;
      result.execution_successor = target.execution_successor;
      return result;
    }
  }
  throw std::logic_error("semantic control has unknown terminator kind");
}

}  // namespace algorithms::graphs
