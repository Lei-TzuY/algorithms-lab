#include "algorithms/graphs/backend_cfg_path_continuation.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

const StackPointerOwnedBackendFramePlan& phase56_plan(
    const BackendFixedFrameBytecodePlan& plan) {
  return plan.source_plan.source_plan.source_plan.source_plan;
}

const ScratchAwareBaseRelativeBackendFrame& require_program_binding(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program) {
  if (!program.graph.directed()) {
    throw std::invalid_argument(
        "CFG path continuation requires a directed out-of-SSA graph");
  }
  if (program.graph.vertex_count() == 0U ||
      program.blocks.size() != program.graph.vertex_count() ||
      program.start >= program.graph.vertex_count()) {
    throw std::invalid_argument(
        "CFG path continuation received malformed out-of-SSA control flow");
  }

  const auto& phase56 = phase56_plan(plan);
  const auto& directed = phase56.stack_directed_frame_plan;
  const auto& base = directed.frame_base_plan;
  if (!phase56.stack_pointer_physical_register.has_value() ||
      !base.frame_base_physical_register.has_value() ||
      !base.byte_addressed_frame.has_value() ||
      !base.addressed_frame.has_value()) {
    throw std::invalid_argument(
        "CFG path continuation requires complete backend frame provenance");
  }

  const StackPointerOwnedBackendFramePlan rebuilt =
      plan_stack_pointer_owned_backend_frame(
          program, phase56.total_physical_registers,
          base.byte_addressed_frame->config,
          base.addressed_frame->addressing_config,
          directed.stack_growth_direction);
  if (rebuilt != phase56) {
    throw std::invalid_argument(
        "CFG path continuation program does not reproduce backend provenance");
  }
  if (base.addressed_frame->blocks.size() != program.graph.vertex_count()) {
    throw std::logic_error(
        "CFG path continuation backend block domain disagrees with bound program");
  }
  return *base.addressed_frame;
}

bool has_cfg_edge(const Graph& graph, const Vertex from, const Vertex to) {
  const auto& neighbors = graph.neighbors(from);
  return std::any_of(neighbors.begin(), neighbors.end(),
                     [to](const auto& edge) { return edge.to == to; });
}

void validate_path(const OutOfSsaProgram& program,
                   const ScratchAwareBaseRelativeBackendFrame& frame,
                   const std::span<const BackendCfgPathBlockRequest> path) {
  if (path.empty()) {
    throw std::invalid_argument(
        "CFG path continuation requires a non-empty path");
  }
  if (path.front().block != program.start) {
    throw std::invalid_argument(
        "CFG path continuation must begin at program start");
  }
  for (std::size_t position = 0U; position < path.size(); ++position) {
    const Vertex block = path[position].block;
    if (block >= program.graph.vertex_count()) {
      throw std::out_of_range(
          "CFG path continuation block index out of range");
    }
    if (!program.blocks[block].reachable || !frame.blocks[block].reachable) {
      throw std::invalid_argument(
          "CFG path continuation cannot visit an unreachable block");
    }
    if (position != 0U &&
        !has_cfg_edge(program.graph, path[position - 1U].block, block)) {
      throw std::invalid_argument(
          "CFG path continuation contains a non-edge transition");
    }
  }
}

}  // namespace

BackendCfgPathContinuationExecution execute_backend_cfg_path_continuation(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    const std::span<const BackendCfgPathBlockRequest> path,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values) {
  const auto& frame = require_program_binding(plan, program);
  validate_path(program, frame, path);

  const auto& phase56 = phase56_plan(plan);
  const std::size_t stack_pointer = *phase56.stack_pointer_physical_register;
  const std::size_t frame_base = *phase56.stack_directed_frame_plan.frame_base_plan
                                      .frame_base_physical_register;
  if (stack_pointer >= initial_registers.size() ||
      frame_base >= initial_registers.size()) {
    throw std::out_of_range(
        "CFG path continuation register file omits reserved frame registers");
  }

  std::vector<std::int64_t> registers(initial_registers.begin(),
                                      initial_registers.end());
  std::vector<std::int64_t> frame_values(initial_frame_slot_values.begin(),
                                         initial_frame_slot_values.end());

  BackendCfgPathContinuationExecution result;
  result.block_executions.reserve(path.size());

  for (std::size_t position = 0U; position < path.size(); ++position) {
    std::vector<std::int64_t> block_initial = registers;
    if (position != 0U) {
      block_initial[stack_pointer] = initial_registers[stack_pointer];
      block_initial[frame_base] = initial_registers[frame_base];
    }

    BackendInstructionContinuationExecution execution =
        execute_backend_instruction_continuation_block(
            plan, path[position].block, block_initial, frame_values,
            path[position].instruction_replies);

    if (position != 0U && execution.after_entry_registers != registers) {
      throw std::logic_error(
          "CFG path continuation failed exact inter-block register handoff");
    }

    registers = execution.after_block_registers;
    frame_values = execution.frame_slot_values;
    const bool suspended = execution.suspended_at_instruction.has_value();
    result.block_executions.push_back(std::move(execution));
    if (suspended) {
      result.suspended_at_path_position = position;
      break;
    }
    ++result.completed_path_blocks;
  }

  result.final_registers = std::move(registers);
  result.final_frame_slot_values = std::move(frame_values);
  return result;
}

}  // namespace algorithms::graphs
