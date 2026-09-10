#include "algorithms/graphs/backend_semantic_cfg_execution.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"

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

struct ReservedFrameRegisters {
  std::size_t stack_pointer;
  std::size_t frame_base;
};

[[nodiscard]] ReservedFrameRegisters require_reserved_frame_registers(
    const BackendFixedFrameBytecodePlan& plan,
    const std::span<const std::int64_t> initial_registers) {
  const auto& phase56 = phase56_plan(plan);
  const auto& frame_base_plan =
      phase56.stack_directed_frame_plan.frame_base_plan;
  if (!phase56.stack_pointer_physical_register.has_value() ||
      !frame_base_plan.frame_base_physical_register.has_value()) {
    throw std::invalid_argument(
        "semantic CFG execution requires reserved frame registers");
  }

  const ReservedFrameRegisters result{
      *phase56.stack_pointer_physical_register,
      *frame_base_plan.frame_base_physical_register};
  if (result.stack_pointer >= initial_registers.size() ||
      result.frame_base >= initial_registers.size()) {
    throw std::out_of_range(
        "semantic CFG execution register file omits reserved frame registers");
  }
  return result;
}

}  // namespace

BackendSemanticCfgExecution execute_backend_semantic_cfg(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    const std::size_t visit_budget,
    const std::span<const BackendSemanticCfgVisitRequest> visit_requests,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values) {
  if (visit_budget == 0U) {
    throw std::invalid_argument(
        "semantic CFG execution requires a positive visit budget");
  }
  if (visit_requests.size() != visit_budget) {
    throw std::invalid_argument(
        "semantic CFG execution requires one reply script per allowed visit");
  }
  if (program.start >= program.blocks.size()) {
    throw std::invalid_argument("semantic CFG execution has invalid program start");
  }

  const ReservedFrameRegisters reserved =
      require_reserved_frame_registers(plan, initial_registers);
  std::vector<std::int64_t> registers(initial_registers.begin(),
                                      initial_registers.end());
  std::vector<std::int64_t> frame_values(initial_frame_slot_values.begin(),
                                         initial_frame_slot_values.end());

  BackendSemanticCfgExecution result;
  result.visits.reserve(visit_budget);
  Vertex current_block = program.start;

  for (std::size_t visit = 0U; visit < visit_budget; ++visit) {
    std::vector<std::int64_t> block_initial = registers;
    if (visit != 0U) {
      block_initial[reserved.stack_pointer] =
          initial_registers[reserved.stack_pointer];
      block_initial[reserved.frame_base] = initial_registers[reserved.frame_base];
    }

    BackendSemanticControlExecution execution =
        execute_backend_semantic_control_block(
            plan, program, current_block, block_initial, frame_values,
            visit_requests[visit].instruction_replies);

    if (visit != 0U &&
        execution.block_execution.after_entry_registers != registers) {
      throw std::logic_error(
          "semantic CFG execution failed exact inter-block register handoff");
    }

    registers = execution.block_execution.after_block_registers;
    frame_values = execution.block_execution.frame_slot_values;
    const BackendSemanticControlStatus status = execution.status;
    const std::optional<Vertex> execution_successor =
        execution.execution_successor;
    result.visits.push_back(std::move(execution));

    if (status == BackendSemanticControlStatus::body_suspended) {
      result.stop_reason = BackendSemanticCfgStopReason::body_suspended;
      result.suspended_at_visit = visit;
      break;
    }

    ++result.completed_visits;
    if (status == BackendSemanticControlStatus::opaque_control) {
      result.stop_reason = BackendSemanticCfgStopReason::opaque_control;
      break;
    }
    if (status != BackendSemanticControlStatus::successor_selected ||
        !execution_successor.has_value()) {
      throw std::logic_error(
          "semantic CFG execution received inconsistent successor status");
    }

    current_block = *execution_successor;
    if (visit + 1U == visit_budget) {
      result.stop_reason = BackendSemanticCfgStopReason::visit_budget_exhausted;
      result.next_execution_block = current_block;
    }
  }

  result.final_registers = std::move(registers);
  result.final_frame_slot_values = std::move(frame_values);
  return result;
}

}  // namespace algorithms::graphs
