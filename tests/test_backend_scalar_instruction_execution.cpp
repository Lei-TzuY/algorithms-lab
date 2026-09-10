#include "algorithms/graphs/backend_instruction_continuation.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "algorithms/graphs/ssa_destruction.hpp"
#include "algorithms/graphs/ssa_scalar_semantics.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

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

BackendOperation semantic_operation(
    std::vector<BackendStorage> inputs, const BackendStorage output,
    const SsaScalarOpcode opcode,
    const std::optional<std::int64_t> immediate = std::nullopt) {
  BackendOperation operation;
  operation.kind = BackendOperationKind::instruction;
  operation.origin_kind = PhiFreeOperationKind::instruction;
  operation.inputs = std::move(inputs);
  operation.output = output;
  operation.instruction_semantics = {opcode, immediate};
  return operation;
}

BackendFixedFrameBytecodePlan canonical_plan(BackendOperation operation) {
  ScratchAwareBackendRegisterSelection selection;
  selection.reserved_scratch_registers = 0U;
  selection.allocatable_registers = 2U;
  selection.abstract_lowering.register_budget = 2U;
  selection.abstract_lowering.stack_slot_count = 0U;

  BackendLoweredBlock block;
  block.reachable = true;
  block.original_block = Vertex{0U};
  block.operations.push_back(std::move(operation));
  selection.abstract_lowering.blocks.push_back(block);
  selection.physicalized_blocks.push_back(block);

  ScratchAwareBackendRegisterPlan register_plan;
  register_plan.total_registers = 2U;
  register_plan.selection = selection;

  FrameBaseReservedBackendPlan base_plan;
  base_plan.total_physical_registers = 3U;
  base_plan.frame_base_physical_register = 2U;
  base_plan.non_base_register_plan = register_plan;
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  base_plan.byte_addressed_frame =
      layout_scratch_aware_backend_frame(*register_plan.selection, layout);
  base_plan.addressed_frame = address_scratch_aware_backend_frame(
      *base_plan.byte_addressed_frame, BackendFrameAddressingConfig{0U});

  StackPointerOwnedBackendFramePlan phase56;
  phase56.total_physical_registers = 4U;
  phase56.stack_pointer_physical_register = 3U;
  phase56.stack_directed_frame_plan = derive_stack_directed_backend_frame_entry(
      base_plan, BackendStackGrowthDirection::toward_lower_addresses);
  return encode_phase56(phase56);
}

SemanticSsaInputInstruction semantic_ssa_instruction(
    std::vector<Variable> uses, const std::optional<Variable> definition,
    const SsaScalarOpcode opcode,
    const std::optional<std::int64_t> immediate = std::nullopt) {
  return SemanticSsaInputInstruction{
      std::move(uses), definition, SsaInstructionSemantics{opcode, immediate}};
}

BackendFixedFrameBytecodePlan mixed_upstream_plan() {
  Graph graph(1U, true);
  std::vector<std::vector<SemanticSsaInputInstruction>> blocks(1U);
  blocks[0] = {
      semantic_ssa_instruction({}, Variable{0U}, SsaScalarOpcode::constant_i64,
                               std::int64_t{7}),
      semantic_ssa_instruction({}, Variable{1U}, SsaScalarOpcode::constant_i64,
                               std::int64_t{5}),
      semantic_ssa_instruction({0U, 1U}, Variable{2U},
                               SsaScalarOpcode::add_i64),
      semantic_ssa_instruction({2U}, Variable{3U}, SsaScalarOpcode::opaque),
      semantic_ssa_instruction({2U, 3U}, Variable{4U},
                               SsaScalarOpcode::multiply_i64),
  };
  const SsaProgram ssa = construct_semantic_ssa(graph, 0U, 5U, blocks);
  const OutOfSsaProgram program = destroy_ssa(graph, ssa);
  const auto phase56 = plan_stack_pointer_owned_backend_frame(
      program, 8U, BackendFrameLayoutConfig{8U, 8U, 16U},
      BackendFrameAddressingConfig{0U},
      BackendStackGrowthDirection::toward_lower_addresses);
  return encode_phase56(phase56);
}

std::vector<std::int64_t> initial_registers(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  std::vector<std::int64_t> registers(phase56.total_physical_registers, 0);
  for (std::size_t index = 0U; index < registers.size(); ++index) {
    registers[index] = static_cast<std::int64_t>(1000U + index);
  }
  return registers;
}

std::vector<std::int64_t> initial_frame(
    const BackendFixedFrameBytecodePlan& plan) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& addressed =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  return std::vector<std::int64_t>(addressed.stack_slots.size(), 0);
}

const BaseRelativeBackendOperation& find_instruction(
    const BackendFixedFrameBytecodePlan& plan, const SsaScalarOpcode opcode) {
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& operations =
      phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame
          ->blocks[0]
          .operations;
  const auto found = std::find_if(
      operations.begin(), operations.end(), [opcode](const auto& operation) {
        return operation.kind == BackendOperationKind::instruction &&
               operation.instruction_semantics.opcode == opcode;
      });
  if (found == operations.end()) {
    throw std::logic_error("test could not find expected semantic instruction");
  }
  return *found;
}

const BackendInstructionContinuationStep& find_step(
    const BackendInstructionContinuationExecution& execution,
    const SsaScalarOpcode opcode) {
  const auto found = std::find_if(
      execution.steps.begin(), execution.steps.end(),
      [opcode](const auto& step) {
        return step.kind == BackendOperationKind::instruction &&
               step.instruction_semantics.opcode == opcode;
      });
  if (found == execution.steps.end()) {
    throw std::logic_error("test could not find expected semantic step");
  }
  return *found;
}

std::int64_t independent_binary_oracle(const SsaScalarOpcode opcode,
                                       const std::int64_t left,
                                       const std::int64_t right) {
  switch (opcode) {
    case SsaScalarOpcode::add_i64:
      return left + right;
    case SsaScalarOpcode::subtract_i64:
      return left - right;
    case SsaScalarOpcode::multiply_i64:
      return left * right;
    case SsaScalarOpcode::equal_i64:
      return left == right ? 1 : 0;
    case SsaScalarOpcode::less_than_i64:
      return left < right ? 1 : 0;
    case SsaScalarOpcode::opaque:
    case SsaScalarOpcode::constant_i64:
    case SsaScalarOpcode::copy_i64:
      break;
  }
  throw std::logic_error("test oracle received non-binary scalar opcode");
}

TEST_CASE(backend_scalar_execution_propagates_semantics_and_suspends_only_at_opaque) {
  const auto plan = mixed_upstream_plan();
  const auto registers = initial_registers(plan);
  const auto frame = initial_frame(plan);
  const std::vector<BackendInstructionOracleReply> no_replies;

  const auto partial = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, no_replies);
  const auto& opaque = find_instruction(plan, SsaScalarOpcode::opaque);
  const auto& add_step = find_step(partial, SsaScalarOpcode::add_i64);

  REQUIRE_EQ(partial.suspended_at_instruction,
             std::optional<std::size_t>{
                 static_cast<std::size_t>(&opaque - &find_instruction(
                     plan, SsaScalarOpcode::constant_i64))});
  REQUIRE_EQ(partial.consumed_instruction_replies, std::size_t{0U});
  REQUIRE(add_step.instruction_semantically_evaluated);
  REQUIRE_EQ(add_step.derived_instruction_result,
             std::optional<std::int64_t>{12});
  REQUIRE(!add_step.instruction_acknowledged);

  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& operations =
      phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame
          ->blocks[0]
          .operations;
  const auto opaque_it = std::find_if(
      operations.begin(), operations.end(), [](const auto& operation) {
        return operation.kind == BackendOperationKind::instruction &&
               operation.instruction_semantics.opcode == SsaScalarOpcode::opaque;
      });
  REQUIRE(opaque_it != operations.end());
  const std::size_t opaque_index =
      static_cast<std::size_t>(opaque_it - operations.begin());
  const std::vector<BackendInstructionOracleReply> replies = {{opaque_index, 3}};
  const auto complete = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, replies);

  REQUIRE(!complete.suspended_at_instruction.has_value());
  REQUIRE_EQ(complete.consumed_instruction_replies, std::size_t{1U});
  const auto& multiply_step =
      find_step(complete, SsaScalarOpcode::multiply_i64);
  REQUIRE(multiply_step.instruction_semantically_evaluated);
  REQUIRE_EQ(multiply_step.derived_instruction_result,
             std::optional<std::int64_t>{36});
  REQUIRE(!multiply_step.instruction_acknowledged);
  REQUIRE(opaque_it->output.has_value());
  const auto& multiply = find_instruction(plan, SsaScalarOpcode::multiply_i64);
  REQUIRE(multiply.output.has_value());
  REQUIRE_EQ(complete.after_block_registers[multiply.output->physical_register],
             std::int64_t{36});
  REQUIRE_EQ(registers, initial_registers(plan));
  REQUIRE_EQ(frame, initial_frame(plan));
}

TEST_CASE(backend_scalar_execution_rejects_reply_override_and_malformed_semantics) {
  const BackendStorage r0{BackendStorageKind::physical_register, 0U};
  const BackendStorage r1{BackendStorageKind::physical_register, 1U};
  const auto known = canonical_plan(semantic_operation(
      {r0, r1}, r1, SsaScalarOpcode::add_i64));
  const std::vector<std::int64_t> registers = {4, 9, 700, 1000};
  const std::vector<std::int64_t> frame;
  const std::vector<BackendInstructionOracleReply> injected = {{0U, 99}};
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        known, 0U, registers, frame, injected),
                    std::invalid_argument);

  BackendOperation malformed = semantic_operation(
      {r0}, r1, SsaScalarOpcode::add_i64);
  const auto malformed_plan = canonical_plan(std::move(malformed));
  const std::vector<BackendInstructionOracleReply> no_replies;
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        malformed_plan, 0U, registers, frame, no_replies),
                    std::invalid_argument);

  BackendOperation corrupted_transfer;
  corrupted_transfer.kind = BackendOperationKind::register_move;
  corrupted_transfer.origin_kind = PhiFreeOperationKind::instruction;
  corrupted_transfer.inputs = {r0};
  corrupted_transfer.output = r1;
  corrupted_transfer.instruction_semantics = {SsaScalarOpcode::copy_i64,
                                               std::nullopt};
  const auto corrupted_plan = canonical_plan(std::move(corrupted_transfer));
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        corrupted_plan, 0U, registers, frame, no_replies),
                    std::logic_error);
}

TEST_CASE(backend_scalar_execution_propagates_checked_overflow) {
  const BackendStorage r0{BackendStorageKind::physical_register, 0U};
  const BackendStorage r1{BackendStorageKind::physical_register, 1U};
  const auto plan = canonical_plan(semantic_operation(
      {r0, r1}, r1, SsaScalarOpcode::add_i64));
  const std::vector<std::int64_t> registers = {
      std::numeric_limits<std::int64_t>::max(), 1, 700, 1000};
  const std::vector<std::int64_t> frame;
  const std::vector<BackendInstructionOracleReply> no_replies;
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        plan, 0U, registers, frame, no_replies),
                    std::overflow_error);
}

TEST_CASE(backend_scalar_execution_matches_independent_random_binary_oracle) {
  std::mt19937_64 rng(0x67C01171A710ULL);
  std::uniform_int_distribution<std::int64_t> values(-1000, 1000);
  std::uniform_int_distribution<unsigned int> opcode_index(0U, 4U);
  const std::vector<SsaScalarOpcode> opcodes = {
      SsaScalarOpcode::add_i64, SsaScalarOpcode::subtract_i64,
      SsaScalarOpcode::multiply_i64, SsaScalarOpcode::equal_i64,
      SsaScalarOpcode::less_than_i64};
  const BackendStorage r0{BackendStorageKind::physical_register, 0U};
  const BackendStorage r1{BackendStorageKind::physical_register, 1U};
  const std::vector<BackendInstructionOracleReply> no_replies;
  const std::vector<std::int64_t> frame;

  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::int64_t left = values(rng);
    const std::int64_t right = values(rng);
    const SsaScalarOpcode opcode =
        opcodes[static_cast<std::size_t>(opcode_index(rng))];
    const auto plan = canonical_plan(
        semantic_operation({r0, r1}, r1, opcode));
    const std::vector<std::int64_t> registers = {left, right, 700, 1000};
    const auto result = execute_backend_instruction_continuation_block(
        plan, 0U, registers, frame, no_replies);
    const std::int64_t expected =
        independent_binary_oracle(opcode, left, right);

    REQUIRE(!result.suspended_at_instruction.has_value());
    REQUIRE_EQ(result.consumed_instruction_replies, std::size_t{0U});
    REQUIRE_EQ(result.after_block_registers[1], expected);
    REQUIRE_EQ(result.steps.size(), std::size_t{1U});
    REQUIRE(result.steps[0].instruction_semantically_evaluated);
    REQUIRE_EQ(result.steps[0].derived_instruction_result,
               std::optional<std::int64_t>{expected});
    REQUIRE(!result.steps[0].instruction_acknowledged);
  }
}

}  // namespace
