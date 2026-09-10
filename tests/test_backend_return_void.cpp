#include "algorithms/graphs/backend_semantic_cfg_execution.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "algorithms/graphs/ssa_control_semantics.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

SemanticSsaInputInstruction semantic_instruction(
    std::vector<Variable> uses, const std::optional<Variable> definition,
    const SsaScalarOpcode opcode,
    const std::optional<std::int64_t> immediate = std::nullopt) {
  return SemanticSsaInputInstruction{
      std::move(uses), definition, SsaInstructionSemantics{opcode, immediate}};
}

SsaControlTerminatorInput jump_control(const Vertex successor) {
  SsaControlTerminatorInput control;
  control.kind = SsaControlTerminatorKind::jump;
  control.jump_successor = successor;
  return control;
}

SsaControlTerminatorInput return_void_control() {
  SsaControlTerminatorInput control;
  control.termination = SsaControlTerminationKind::return_void;
  return control;
}

BackendFixedFrameBytecodePlan encode_phase56(
    const StackPointerOwnedBackendFramePlan& phase56) {
  const BackendFixedFrameActionPlan phase57 = derive_fixed_frame_actions(phase56);
  const BackendFixedFrameActionLegalityPolicy policy{
      8U, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
  const auto phase59 = lower_fixed_frame_actions_to_symbolic_instructions(phase58);
  return encode_backend_fixed_frame_bytecode(phase59);
}

BackendFixedFrameBytecodePlan plan_for(const OutOfSsaProgram& program,
                                       const std::size_t total_registers = 8U) {
  return encode_phase56(plan_stack_pointer_owned_backend_frame(
      program, total_registers, BackendFrameLayoutConfig{8U, 8U, 16U},
      BackendFrameAddressingConfig{0U},
      BackendStackGrowthDirection::toward_lower_addresses));
}

const StackPointerOwnedBackendFramePlan& phase56(
    const BackendFixedFrameBytecodePlan& plan) {
  return plan.source_plan.source_plan.source_plan.source_plan;
}

const ScratchAwareBaseRelativeBackendFrame& addressed_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  return *phase56(plan)
              .stack_directed_frame_plan.frame_base_plan.addressed_frame;
}

const BackendControlTerminator& backend_control(
    const BackendFixedFrameBytecodePlan& plan, const Vertex block) {
  const auto& selection = *phase56(plan)
                                .stack_directed_frame_plan.frame_base_plan
                                .non_base_register_plan.selection;
  return selection.abstract_lowering.blocks[block].control;
}

std::vector<std::int64_t> initial_registers(
    const BackendFixedFrameBytecodePlan& plan, const std::int64_t fill = 0) {
  std::vector<std::int64_t> registers(phase56(plan).total_physical_registers,
                                      fill);
  registers[*phase56(plan).stack_pointer_physical_register] = 1000000;
  return registers;
}

std::vector<std::int64_t> initial_frame(
    const BackendFixedFrameBytecodePlan& plan, const std::int64_t fill = 0) {
  return std::vector<std::int64_t>(addressed_frame(plan).stack_slots.size(), fill);
}

TEST_CASE(return_void_requires_reachable_payload_free_cfg_sink) {
  Graph graph(2U, true);
  graph.add_edge(0U, 1U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(2U);

  std::vector<SsaControlTerminatorInput> controls(2U);
  controls[0] = return_void_control();
  REQUIRE_THROWS_AS(construct_control_semantic_out_of_ssa(
                        graph, 0U, 0U, blocks, controls),
                    std::invalid_argument);

  controls.assign(2U, SsaControlTerminatorInput{});
  controls[1] = return_void_control();
  const OutOfSsaProgram valid = construct_control_semantic_out_of_ssa(
      graph, 0U, 0U, blocks, controls);
  REQUIRE_EQ(valid.blocks[1].control.termination,
             SsaControlTerminationKind::return_void);

  controls[1].jump_successor = 1U;
  REQUIRE_THROWS_AS(construct_control_semantic_out_of_ssa(
                        graph, 0U, 0U, blocks, controls),
                    std::invalid_argument);

  controls.assign(2U, SsaControlTerminatorInput{});
  controls[1] = return_void_control();
  controls[1].kind = SsaControlTerminatorKind::jump;
  controls[1].jump_successor = 1U;
  REQUIRE_THROWS_AS(construct_control_semantic_out_of_ssa(
                        graph, 0U, 0U, blocks, controls),
                    std::invalid_argument);

  Graph disconnected(2U, true);
  controls.assign(2U, SsaControlTerminatorInput{});
  controls[1] = return_void_control();
  REQUIRE_THROWS_AS(construct_control_semantic_out_of_ssa(
                        disconnected, 0U, 0U, blocks, controls),
                    std::invalid_argument);
}

TEST_CASE(return_void_survives_backend_provenance_and_block_execution) {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  std::vector<SsaControlTerminatorInput> controls(1U);
  controls[0] = return_void_control();

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 0U, blocks, controls);
  const BackendFixedFrameBytecodePlan plan = plan_for(program, 2U);
  const BackendControlTerminator& control = backend_control(plan, 0U);
  REQUIRE_EQ(control.kind, SsaControlTerminatorKind::opaque);
  REQUIRE_EQ(control.termination, SsaControlTerminationKind::return_void);
  REQUIRE(!control.predicate_storage.has_value());
  REQUIRE(!control.jump_target.has_value());
  REQUIRE(!control.nonzero_target.has_value());
  REQUIRE(!control.zero_target.has_value());

  const std::vector<BackendInstructionOracleReply> replies;
  const auto execution = execute_backend_semantic_control_block(
      plan, program, 0U, initial_registers(plan), initial_frame(plan), replies);
  REQUIRE_EQ(execution.status, BackendSemanticControlStatus::returned);
  REQUIRE_EQ(execution.termination_kind, SsaControlTerminationKind::return_void);
  REQUIRE(!execution.predicate_value.has_value());
  REQUIRE(!execution.logical_successor.has_value());
  REQUIRE(!execution.execution_successor.has_value());
}

TEST_CASE(return_void_cfg_executes_canonical_fixed_frame_exit_once) {
  Graph graph(2U, true);
  graph.add_edge(0U, 1U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(2U);
  for (Variable variable = 0U; variable < 4U; ++variable) {
    blocks[0].push_back(semantic_instruction(
        {}, variable, SsaScalarOpcode::constant_i64,
        std::int64_t{10} + static_cast<std::int64_t>(variable)));
    blocks[1].push_back(semantic_instruction(
        {variable}, Variable{variable + 4U}, SsaScalarOpcode::copy_i64));
  }
  std::vector<SsaControlTerminatorInput> controls(2U);
  controls[0] = jump_control(1U);
  controls[1] = return_void_control();

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 8U, blocks, controls);
  const BackendFixedFrameBytecodePlan plan = plan_for(program, 4U);
  REQUIRE(!addressed_frame(plan).stack_slots.empty());

  const auto registers = initial_registers(plan, 700);
  const auto frame = initial_frame(plan, 31);
  const std::vector<BackendSemanticCfgVisitRequest> visits(5U);
  const auto execution = execute_backend_semantic_cfg(
      plan, program, visits.size(), visits, registers, frame);

  REQUIRE_EQ(execution.stop_reason, BackendSemanticCfgStopReason::returned);
  REQUIRE_EQ(execution.completed_visits, std::size_t{2U});
  REQUIRE_EQ(execution.visits.size(), std::size_t{2U});
  REQUIRE_EQ(execution.visits.back().status,
             BackendSemanticControlStatus::returned);
  REQUIRE_EQ(execution.visits.back().termination_kind,
             SsaControlTerminationKind::return_void);
  REQUIRE(!execution.suspended_at_visit.has_value());
  REQUIRE(!execution.next_execution_block.has_value());
  REQUIRE(execution.fixed_frame_exit_replay.has_value());

  const auto& replay = *execution.fixed_frame_exit_replay;
  const auto& pre_exit = execution.visits.back().block_execution;
  REQUIRE_EQ(replay.after_entry_registers, pre_exit.after_block_registers);
  REQUIRE_EQ(execution.final_registers, replay.after_exit_registers);
  REQUIRE_EQ(execution.final_frame_slot_values, pre_exit.frame_slot_values);

  const std::size_t stack_pointer = *phase56(plan).stack_pointer_physical_register;
  REQUIRE(pre_exit.after_block_registers[stack_pointer] != registers[stack_pointer]);
  REQUIRE_EQ(execution.final_registers[stack_pointer], registers[stack_pointer]);
}

TEST_CASE(return_void_waits_for_body_completion_and_never_uses_budget_as_return) {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  blocks[0].push_back(
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::opaque));
  std::vector<SsaControlTerminatorInput> controls(1U);
  controls[0] = return_void_control();

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  const BackendFixedFrameBytecodePlan plan = plan_for(program, 3U);
  const auto registers = initial_registers(plan, 200);
  const auto frame = initial_frame(plan, 41);

  std::vector<BackendSemanticCfgVisitRequest> visits(1U);
  const auto suspended = execute_backend_semantic_cfg(
      plan, program, 1U, visits, registers, frame);
  REQUIRE_EQ(suspended.stop_reason,
             BackendSemanticCfgStopReason::body_suspended);
  REQUIRE_EQ(suspended.completed_visits, std::size_t{0U});
  REQUIRE_EQ(suspended.visits.size(), std::size_t{1U});
  REQUIRE(!suspended.fixed_frame_exit_replay.has_value());
  REQUIRE(suspended.visits[0].block_execution.suspended_at_instruction.has_value());

  const std::size_t operation =
      *suspended.visits[0].block_execution.suspended_at_instruction;
  visits[0].instruction_replies.push_back(
      BackendInstructionOracleReply{operation, std::int64_t{42}});
  const auto returned = execute_backend_semantic_cfg(
      plan, program, 1U, visits, registers, frame);
  REQUIRE_EQ(returned.stop_reason, BackendSemanticCfgStopReason::returned);
  REQUIRE_EQ(returned.completed_visits, std::size_t{1U});
  REQUIRE(returned.fixed_frame_exit_replay.has_value());
  REQUIRE(!returned.next_execution_block.has_value());
}

}  // namespace
