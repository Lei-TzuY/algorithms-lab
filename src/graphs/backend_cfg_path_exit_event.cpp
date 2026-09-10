#include "algorithms/graphs/backend_cfg_path_exit_event.hpp"
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

}  // namespace

BackendCfgPathExitEventExecution execute_backend_cfg_path_exit_event(
    const BackendFixedFrameBytecodePlan& plan, const OutOfSsaProgram& program,
    const std::span<const BackendCfgPathBlockRequest> path,
    const std::span<const std::int64_t> initial_registers,
    const std::span<const std::int64_t> initial_frame_slot_values) {
  BackendCfgPathExitEventExecution result;
  result.path_execution = execute_backend_cfg_path_continuation(
      plan, program, path, initial_registers, initial_frame_slot_values);
  result.final_registers = result.path_execution.final_registers;
  result.final_frame_slot_values = result.path_execution.final_frame_slot_values;

  if (result.path_execution.suspended_at_path_position.has_value()) {
    return result;
  }
  if (result.path_execution.completed_path_blocks != path.size()) {
    throw std::logic_error(
        "CFG exit event requires a completely executed caller path");
  }

  const auto& phase56 = phase56_plan(plan);
  if (!phase56.stack_pointer_physical_register.has_value() ||
      !phase56.stack_directed_frame_plan.frame_base_plan
           .frame_base_physical_register.has_value()) {
    throw std::logic_error(
        "CFG exit event requires canonical reserved frame registers");
  }
  const std::size_t stack_pointer = *phase56.stack_pointer_physical_register;
  const std::size_t frame_base = *phase56.stack_directed_frame_plan.frame_base_plan
                                      .frame_base_physical_register;
  if (stack_pointer >= initial_registers.size() ||
      frame_base >= initial_registers.size() ||
      stack_pointer >= result.final_registers.size() ||
      frame_base >= result.final_registers.size()) {
    throw std::out_of_range(
        "CFG exit event register file omits reserved frame registers");
  }

  std::vector<std::int64_t> replay_input = result.final_registers;
  replay_input[stack_pointer] = initial_registers[stack_pointer];
  replay_input[frame_base] = initial_registers[frame_base];

  BackendFixedFrameBytecodeExecutionSnapshots fixed_frame =
      execute_backend_fixed_frame_bytecode(plan, replay_input);
  if (fixed_frame.after_entry_registers != result.final_registers) {
    throw std::logic_error(
        "CFG exit event failed canonical pre-exit state reconstruction");
  }

  result.final_registers = fixed_frame.after_exit_registers;
  result.fixed_frame_exit_replay = std::move(fixed_frame);
  result.exit_event_consumed = true;
  return result;
}

}  // namespace algorithms::graphs
