#include "algorithms/graphs/backend_semantic_control.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "algorithms/graphs/ssa_control_semantics.hpp"
#include "test_framework.hpp"

#include <algorithm>
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
  const auto phase56 = plan_stack_pointer_owned_backend_frame(
      program, total_registers, BackendFrameLayoutConfig{8U, 8U, 16U},
      BackendFrameAddressingConfig{0U},
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE(phase56.stack_pointer_physical_register.has_value());
  REQUIRE(phase56.stack_directed_frame_plan.frame_base_plan
              .frame_base_physical_register.has_value());
  REQUIRE(phase56.stack_directed_frame_plan.frame_base_plan
              .addressed_frame.has_value());
  return encode_phase56(phase56);
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

const BackendControlTerminator& backend_control(
    const BackendFixedFrameBytecodePlan& plan, const Vertex block) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& selection = *phase56.stack_directed_frame_plan.frame_base_plan
                                .non_base_register_plan.selection;
  return selection.abstract_lowering.blocks[block].control;
}

bool has_interference(const PhiFreeRegisterAllocation& allocation,
                      const SsaCopyLocation first,
                      const SsaCopyLocation second) {
  return std::any_of(
      allocation.interference_edges.begin(), allocation.interference_edges.end(),
      [first, second](const RegisterInterferenceEdge& edge) {
        return (edge.first == first && edge.second == second) ||
               (edge.first == second && edge.second == first);
      });
}

TEST_CASE(backend_semantic_control_selects_explicit_target_not_adjacency_order) {
  for (const std::int64_t predicate_value : {std::int64_t{0}, std::int64_t{7}}) {
    Graph graph(3U, true);
    graph.add_edge(0U, 2U);
    graph.add_edge(0U, 1U);

    std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
    blocks[0].push_back(semantic_instruction(
        {}, Variable{0U}, SsaScalarOpcode::constant_i64, predicate_value));
    std::vector<SsaControlTerminatorInput> controls(3U);
    controls[0] = branch_control(0U, 1U, 2U);

    const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
        graph, 0U, 1U, blocks, controls);
    const auto plan = plan_for(program);
    const auto registers = initial_registers(plan);
    const auto frame = initial_frame(plan);
    const std::vector<BackendInstructionOracleReply> replies;

    const auto result = execute_backend_semantic_control_block(
        plan, program, 0U, registers, frame, replies);
    REQUIRE_EQ(result.status, BackendSemanticControlStatus::successor_selected);
    REQUIRE_EQ(result.control_kind,
               SsaControlTerminatorKind::branch_if_nonzero);
    REQUIRE_EQ(result.predicate_value,
               std::optional<std::int64_t>{predicate_value});
    const Vertex expected = predicate_value == 0 ? Vertex{2U} : Vertex{1U};
    REQUIRE_EQ(result.logical_successor, std::optional<Vertex>{expected});
    REQUIRE_EQ(result.execution_successor, std::optional<Vertex>{expected});
  }
}

TEST_CASE(backend_semantic_control_predicate_is_a_real_block_end_liveness_use) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);

  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  blocks[0] = {
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::constant_i64,
                           std::int64_t{0}),
      semantic_instruction({}, Variable{1U}, SsaScalarOpcode::constant_i64,
                           std::int64_t{9}),
  };
  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = branch_control(0U, 1U, 2U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 2U, blocks, controls);
  REQUIRE(program.blocks[0].control.predicate.has_value());
  REQUIRE(program.blocks[0].instructions[1].definition.has_value());
  const SsaCopyLocation predicate = *program.blocks[0].control.predicate;
  const SsaCopyLocation later = SsaCopyLocation::from_value(
      *program.blocks[0].instructions[1].definition);

  const PhiFreeRegisterAllocation allocation =
      allocate_phi_free_registers(program, 1U);
  REQUIRE(has_interference(allocation, predicate, later));
}

TEST_CASE(backend_semantic_control_reads_spilled_predicate_from_frame) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = branch_control(0U, 1U, 2U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  const auto plan = plan_for(program, 2U);
  const BackendControlTerminator& control = backend_control(plan, 0U);
  REQUIRE(control.predicate_storage.has_value());
  REQUIRE_EQ(control.predicate_storage->kind, BackendStorageKind::stack_slot);

  const auto registers = initial_registers(plan);
  auto frame = initial_frame(plan);
  REQUIRE(control.predicate_storage->index < frame.size());
  const std::vector<BackendInstructionOracleReply> replies;

  frame[control.predicate_storage->index] = -4;
  const auto nonzero = execute_backend_semantic_control_block(
      plan, program, 0U, registers, frame, replies);
  REQUIRE_EQ(nonzero.predicate_value, std::optional<std::int64_t>{-4});
  REQUIRE_EQ(nonzero.logical_successor, std::optional<Vertex>{1U});

  frame[control.predicate_storage->index] = 0;
  const auto zero = execute_backend_semantic_control_block(
      plan, program, 0U, registers, frame, replies);
  REQUIRE_EQ(zero.predicate_value, std::optional<std::int64_t>{0});
  REQUIRE_EQ(zero.logical_successor, std::optional<Vertex>{2U});
}

TEST_CASE(backend_semantic_control_maps_critical_edge_through_phi_split_block) {
  Graph graph(3U, true);
  graph.add_edge(0U, 2U);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);

  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  blocks[0].push_back(semantic_instruction(
      {}, Variable{0U}, SsaScalarOpcode::constant_i64, std::int64_t{5}));
  blocks[1].push_back(semantic_instruction(
      {}, Variable{0U}, SsaScalarOpcode::constant_i64, std::int64_t{7}));
  blocks[2].push_back(
      semantic_instruction({0U}, Variable{2U}, SsaScalarOpcode::copy_i64));

  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = branch_control(1U, 2U, 1U);
  controls[1] = jump_control(2U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 3U, blocks, controls);
  REQUIRE(program.blocks.size() > graph.vertex_count());
  REQUIRE(program.blocks[0].control.nonzero_target.has_value());
  const SsaLoweredControlTarget target =
      *program.blocks[0].control.nonzero_target;
  REQUIRE_EQ(target.logical_successor, Vertex{2U});
  REQUIRE(target.execution_successor >= graph.vertex_count());
  REQUIRE(target.execution_successor < program.blocks.size());
  REQUIRE(!program.blocks[target.execution_successor].original_block.has_value());
  REQUIRE_EQ(program.blocks[target.execution_successor].control.kind,
             SsaControlTerminatorKind::jump);
  REQUIRE(program.blocks[target.execution_successor].control.jump_target.has_value());
  REQUIRE_EQ(program.blocks[target.execution_successor]
                 .control.jump_target->execution_successor,
             Vertex{2U});
}

TEST_CASE(backend_semantic_control_preserves_body_suspension_and_opaque_control) {
  Graph graph(2U, true);
  graph.add_edge(0U, 1U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(2U);
  blocks[0].push_back(
      semantic_instruction({}, Variable{0U}, SsaScalarOpcode::opaque));
  std::vector<SsaControlTerminatorInput> controls(2U);
  controls[0] = jump_control(1U);

  const OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 1U, blocks, controls);
  const auto plan = plan_for(program);
  const auto registers = initial_registers(plan);
  const auto frame = initial_frame(plan);
  const std::vector<BackendInstructionOracleReply> no_replies;

  const auto suspended = execute_backend_semantic_control_block(
      plan, program, 0U, registers, frame, no_replies);
  REQUIRE_EQ(suspended.status, BackendSemanticControlStatus::body_suspended);
  REQUIRE(!suspended.logical_successor.has_value());
  REQUIRE(!suspended.execution_successor.has_value());
  REQUIRE(suspended.block_execution.suspended_at_instruction.has_value());

  const std::size_t operation_index =
      *suspended.block_execution.suspended_at_instruction;
  const std::vector<BackendInstructionOracleReply> replies = {
      {operation_index, std::int64_t{42}}};
  const auto complete = execute_backend_semantic_control_block(
      plan, program, 0U, registers, frame, replies);
  REQUIRE_EQ(complete.status, BackendSemanticControlStatus::successor_selected);
  REQUIRE_EQ(complete.logical_successor, std::optional<Vertex>{1U});

  Graph opaque_graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> opaque_blocks(1U);
  std::vector<SsaControlTerminatorInput> opaque_controls(1U);
  const OutOfSsaProgram opaque_program = construct_control_semantic_out_of_ssa(
      opaque_graph, 0U, 0U, opaque_blocks, opaque_controls);
  const auto opaque_plan = plan_for(opaque_program, 2U);
  const auto opaque = execute_backend_semantic_control_block(
      opaque_plan, opaque_program, 0U, initial_registers(opaque_plan),
      initial_frame(opaque_plan), no_replies);
  REQUIRE_EQ(opaque.status, BackendSemanticControlStatus::opaque_control);
  REQUIRE(!opaque.logical_successor.has_value());
}

TEST_CASE(backend_semantic_control_rejects_invalid_input_and_target_tampering) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 2U);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(3U);
  std::vector<SsaControlTerminatorInput> invalid_controls(3U);
  invalid_controls[0] = jump_control(0U);
  REQUIRE_THROWS_AS(construct_control_semantic_out_of_ssa(
                        graph, 0U, 0U, blocks, invalid_controls),
                    std::invalid_argument);

  std::vector<SsaControlTerminatorInput> controls(3U);
  controls[0] = jump_control(1U);
  OutOfSsaProgram program = construct_control_semantic_out_of_ssa(
      graph, 0U, 0U, blocks, controls);
  const auto plan = plan_for(program, 2U);
  program.blocks[0].control.jump_target = SsaLoweredControlTarget{2U, 2U};
  const std::vector<BackendInstructionOracleReply> replies;
  REQUIRE_THROWS_AS(execute_backend_semantic_control_block(
                        plan, program, 0U, initial_registers(plan),
                        initial_frame(plan), replies),
                    std::invalid_argument);
}

}  // namespace
