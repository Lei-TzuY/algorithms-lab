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

SsaControlTerminatorInput branch_control(const Variable predicate,
                                         const Vertex nonzero,
                                         const Vertex zero) {
  SsaControlTerminatorInput control;
  control.kind = SsaControlTerminatorKind::branch_if_nonzero;
  control.predicate = predicate;
  control.nonzero_successor = nonzero;
  control.zero_successor = zero;
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

std::vector<std::int64_t> initial_registers(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  return std::vector<std::int64_t>(phase56.total_physical_registers, 0);
}

std::vector<std::int64_t> initial_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& frame =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  return std::vector<std::int64_t>(frame.stack_slots.size(), 0);
}

void require_handoff(const BackendSemanticCfgExecution& execution) {
  for (std::size_t visit = 1U; visit < execution.visits.size(); ++visit) {
    REQUIRE_EQ(execution.visits[visit].block_execution.after_entry_registers,
               execution.visits[visit - 1U]
                   .block_execution.after_block_registers);
  }
}

TEST_CASE(semantic_cfg_follows_explicit_control_not_adjacency_order) {
  Graph graph(4U, true);
  graph.add_edge(0U, 2U);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 3U);
  graph.add_edge(2U, 3U);

  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(4U);
  blocks[0].push_back(semantic_instruction(
      {}, Variable{0U}, SsaScalarOpcode::constant_i64, std::int64_t{7}));
  std::vector<SsaControlTerminatorInput> controls(4U);
  controls[0] = branch_control(0U, 1U, 2U);
  controls[1] = jump_control(3U);
  controls[2] = jump_control(3U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  const auto plan = plan_for(program);
  const std::vector<BackendSemanticCfgVisitRequest> visits(4U);
  const auto execution = execute_backend_semantic_cfg(
      plan, program, 4U, visits, initial_registers(plan), initial_frame(plan));

  REQUIRE_EQ(execution.stop_reason,
             BackendSemanticCfgStopReason::opaque_control);
  REQUIRE_EQ(execution.completed_visits, std::size_t{3U});
  REQUIRE_EQ(execution.visits.size(), std::size_t{3U});
  REQUIRE_EQ(execution.visits[0].block_execution.block_index, Vertex{0U});
  REQUIRE_EQ(execution.visits[1].block_execution.block_index, Vertex{1U});
  REQUIRE_EQ(execution.visits[2].block_execution.block_index, Vertex{3U});
  REQUIRE(!execution.next_execution_block.has_value());
  REQUIRE(!execution.suspended_at_visit.has_value());
  require_handoff(execution);
}

TEST_CASE(semantic_cfg_consumes_opaque_replies_per_dynamic_visit_in_loop) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 1U);
  graph.add_edge(1U, 2U);

  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  blocks[1].push_back(
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::opaque));
  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = jump_control(1U);
  controls[1] = branch_control(0U, 1U, 2U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  REQUIRE(program.blocks[1].control.nonzero_target.has_value());
  const SsaLoweredControlTarget loop_back =
      *program.blocks[1].control.nonzero_target;
  REQUIRE_EQ(loop_back.logical_successor, Vertex{1U});
  REQUIRE(loop_back.execution_successor != loop_back.logical_successor);

  const auto plan = plan_for(program);
  const std::vector<BackendInstructionOracleReply> no_replies;
  const auto probe = execute_backend_semantic_control_block(
      plan, program, 1U, initial_registers(plan), initial_frame(plan), no_replies);
  REQUIRE_EQ(probe.status, BackendSemanticControlStatus::body_suspended);
  REQUIRE(probe.block_execution.suspended_at_instruction.has_value());
  const std::size_t opaque_operation =
      *probe.block_execution.suspended_at_instruction;

  std::vector<BackendSemanticCfgVisitRequest> visits(5U);
  visits[1].instruction_replies.push_back(
      BackendInstructionOracleReply{opaque_operation, std::int64_t{1}});
  visits[3].instruction_replies.push_back(
      BackendInstructionOracleReply{opaque_operation, std::int64_t{0}});

  const auto execution = execute_backend_semantic_cfg(
      plan, program, 5U, visits, initial_registers(plan), initial_frame(plan));
  REQUIRE_EQ(execution.stop_reason,
             BackendSemanticCfgStopReason::opaque_control);
  REQUIRE_EQ(execution.completed_visits, std::size_t{5U});
  REQUIRE_EQ(execution.visits.size(), std::size_t{5U});
  REQUIRE_EQ(execution.visits[0].block_execution.block_index, Vertex{0U});
  REQUIRE_EQ(execution.visits[1].block_execution.block_index, Vertex{1U});
  REQUIRE_EQ(execution.visits[2].block_execution.block_index,
             loop_back.execution_successor);
  REQUIRE_EQ(execution.visits[3].block_execution.block_index, Vertex{1U});
  REQUIRE_EQ(execution.visits[4].block_execution.block_index, Vertex{2U});
  REQUIRE_EQ(execution.visits[1].logical_successor,
             std::optional<Vertex>{1U});
  REQUIRE_EQ(execution.visits[1].execution_successor,
             std::optional<Vertex>{loop_back.execution_successor});
  REQUIRE_EQ(execution.visits[1].predicate_value,
             std::optional<std::int64_t>{1});
  REQUIRE_EQ(execution.visits[3].predicate_value,
             std::optional<std::int64_t>{0});
  require_handoff(execution);
}

TEST_CASE(semantic_cfg_stops_on_body_suspension_before_control) {
  Graph graph(2U, true);
  graph.add_edge(0U, 1U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(2U);
  blocks[1].push_back(
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::opaque));
  std::vector<SsaControlTerminatorInput> controls(2U);
  controls[0] = jump_control(1U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  const auto plan = plan_for(program);
  const std::vector<BackendSemanticCfgVisitRequest> visits(2U);
  const auto execution = execute_backend_semantic_cfg(
      plan, program, 2U, visits, initial_registers(plan), initial_frame(plan));
  REQUIRE_EQ(execution.stop_reason,
             BackendSemanticCfgStopReason::body_suspended);
  REQUIRE_EQ(execution.completed_visits, std::size_t{1U});
  REQUIRE_EQ(execution.visits.size(), std::size_t{2U});
  REQUIRE_EQ(execution.suspended_at_visit, std::optional<std::size_t>{1U});
  REQUIRE_EQ(execution.visits[1].status,
             BackendSemanticControlStatus::body_suspended);
  REQUIRE(!execution.visits[1].logical_successor.has_value());
  REQUIRE(!execution.next_execution_block.has_value());
  require_handoff(execution);
}

TEST_CASE(semantic_cfg_budget_bounds_loop_without_claiming_termination) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 1U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = jump_control(1U);
  controls[1] = jump_control(2U);
  controls[2] = jump_control(1U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 0U, blocks, controls);
  const auto plan = plan_for(program, 2U);
  const std::vector<BackendSemanticCfgVisitRequest> visits(5U);
  const auto execution = execute_backend_semantic_cfg(
      plan, program, 5U, visits, initial_registers(plan), initial_frame(plan));

  REQUIRE_EQ(execution.stop_reason,
             BackendSemanticCfgStopReason::visit_budget_exhausted);
  REQUIRE_EQ(execution.completed_visits, std::size_t{5U});
  REQUIRE_EQ(execution.visits.size(), std::size_t{5U});
  REQUIRE_EQ(execution.next_execution_block, std::optional<Vertex>{1U});
  REQUIRE_EQ(execution.visits[0].block_execution.block_index, Vertex{0U});
  REQUIRE_EQ(execution.visits[1].block_execution.block_index, Vertex{1U});
  REQUIRE_EQ(execution.visits[2].block_execution.block_index, Vertex{2U});
  REQUIRE_EQ(execution.visits[3].block_execution.block_index, Vertex{1U});
  REQUIRE_EQ(execution.visits[4].block_execution.block_index, Vertex{2U});
  require_handoff(execution);
}

TEST_CASE(semantic_cfg_executes_concrete_phi_split_before_logical_successor) {
  Graph graph(3U, true);
  graph.add_edge(0U, 2U);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);

  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  blocks[0] = {
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::constant_i64,
                           std::int64_t{5}),
      semantic_instruction({}, Variable{1U}, SsaScalarOpcode::constant_i64,
                           std::int64_t{1}),
  };
  blocks[1].push_back(semantic_instruction(
      {}, Variable{0U}, SsaScalarOpcode::constant_i64, std::int64_t{7}));
  blocks[2].push_back(
      semantic_instruction({0U}, Variable{2U}, SsaScalarOpcode::copy_i64));
  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = branch_control(1U, 2U, 1U);
  controls[1] = jump_control(2U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 3U, blocks, controls);
  REQUIRE(program.blocks[0].control.nonzero_target.has_value());
  const SsaLoweredControlTarget target =
      *program.blocks[0].control.nonzero_target;
  REQUIRE_EQ(target.logical_successor, Vertex{2U});
  REQUIRE(target.execution_successor != target.logical_successor);

  const auto plan = plan_for(program);
  const std::vector<BackendSemanticCfgVisitRequest> visits(3U);
  const auto execution = execute_backend_semantic_cfg(
      plan, program, 3U, visits, initial_registers(plan), initial_frame(plan));

  REQUIRE_EQ(execution.visits.size(), std::size_t{3U});
  REQUIRE_EQ(execution.visits[0].logical_successor,
             std::optional<Vertex>{2U});
  REQUIRE_EQ(execution.visits[0].execution_successor,
             std::optional<Vertex>{target.execution_successor});
  REQUIRE_EQ(execution.visits[1].block_execution.block_index,
             target.execution_successor);
  REQUIRE_EQ(execution.visits[2].block_execution.block_index, Vertex{2U});
  REQUIRE_EQ(execution.stop_reason,
             BackendSemanticCfgStopReason::opaque_control);
  require_handoff(execution);
}

TEST_CASE(semantic_cfg_validates_budget_and_request_shape) {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  std::vector<SsaControlTerminatorInput> controls(1U);
  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 0U, blocks, controls);
  const auto plan = plan_for(program, 2U);
  const auto registers = initial_registers(plan);
  const auto frame = initial_frame(plan);
  const std::vector<BackendSemanticCfgVisitRequest> none;
  const std::vector<BackendSemanticCfgVisitRequest> one(1U);

  REQUIRE_THROWS_AS(execute_backend_semantic_cfg(
                        plan, program, 0U, none, registers, frame),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(execute_backend_semantic_cfg(
                        plan, program, 2U, one, registers, frame),
                    std::invalid_argument);
}

}  // namespace
