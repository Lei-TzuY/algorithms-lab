#include "algorithms/graphs/backend_instruction_continuation.hpp"
#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
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

BackendStorage physical(const std::size_t reg) {
  return BackendStorage{BackendStorageKind::physical_register, reg};
}

BackendStorage stack_cell(const std::size_t slot) {
  return BackendStorage{BackendStorageKind::stack_slot, slot};
}

BackendOperation operation(const BackendOperationKind kind,
                           std::vector<BackendStorage> inputs,
                           std::optional<BackendStorage> output,
                           const std::size_t origin_index) {
  BackendOperation result;
  result.kind = kind;
  result.origin_kind = PhiFreeOperationKind::instruction;
  result.origin_index = origin_index;
  result.inputs = std::move(inputs);
  result.output = output;
  return result;
}

BackendOperation transfer(const BackendOperationKind kind,
                          const BackendStorage input,
                          const BackendStorage output,
                          const std::size_t origin_index) {
  return operation(kind, {input}, output, origin_index);
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

BackendFixedFrameBytecodePlan canonical_plan(
    std::vector<BackendOperation> operations, const bool reachable = true) {
  ScratchAwareBackendRegisterSelection selection;
  selection.reserved_scratch_registers = 0U;
  selection.allocatable_registers = 2U;
  selection.abstract_lowering.register_budget = 2U;
  selection.abstract_lowering.stack_slot_count = 2U;

  BackendLoweredBlock block;
  block.reachable = reachable;
  block.original_block = Vertex{0U};
  block.operations = std::move(operations);
  selection.abstract_lowering.blocks.push_back(block);
  selection.physicalized_blocks.push_back(block);

  ScratchAwareBackendRegisterPlan register_plan;
  register_plan.total_registers = 2U;
  register_plan.selection = selection;

  FrameBaseReservedBackendPlan base_plan;
  base_plan.total_physical_registers = 3U;
  base_plan.frame_base_physical_register = 2U;
  base_plan.non_base_register_plan = register_plan;

  const BackendFrameLayoutConfig layout_config{8U, 8U, 16U};
  base_plan.byte_addressed_frame =
      layout_scratch_aware_backend_frame(*register_plan.selection, layout_config);
  const BackendFrameAddressingConfig addressing_config{0U};
  base_plan.addressed_frame = address_scratch_aware_backend_frame(
      *base_plan.byte_addressed_frame, addressing_config);

  StackPointerOwnedBackendFramePlan phase56;
  phase56.total_physical_registers = 4U;
  phase56.stack_pointer_physical_register = 3U;
  phase56.stack_directed_frame_plan = derive_stack_directed_backend_frame_entry(
      base_plan, BackendStackGrowthDirection::toward_lower_addresses);
  return encode_phase56(phase56);
}

OutOfSsaProgram one_block_program(const std::size_t variables) {
  OutOfSsaProgram program;
  program.start = 0U;
  program.variable_count = variables;
  program.graph = Graph(1U, true);
  program.blocks.resize(1U);
  program.blocks[0].reachable = true;
  program.blocks[0].original_block = 0U;
  for (std::size_t variable = 0U; variable < variables; ++variable) {
    program.initial_values.push_back({variable, 0U});
  }
  return program;
}

BackendFixedFrameBytecodePlan upstream_spilled_instruction_plan() {
  OutOfSsaProgram program = one_block_program(4U);
  for (std::size_t variable = 4U; variable != 0U; --variable) {
    program.blocks[0].instructions.push_back(
        SsaInstruction{{SsaValue{variable - 1U, 0U}}, std::nullopt});
  }

  const BackendFrameLayoutConfig layout_config{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing_config{0U};
  const auto phase56 = plan_stack_pointer_owned_backend_frame(
      program, 4U, layout_config, addressing_config,
      BackendStackGrowthDirection::toward_lower_addresses);
  return encode_phase56(phase56);
}

TEST_CASE(backend_instruction_continuation_supplies_result_then_executes_store) {
  const auto plan = canonical_plan({
      transfer(BackendOperationKind::stack_reload, stack_cell(0U), physical(0U),
               10U),
      operation(BackendOperationKind::instruction, {physical(0U)}, physical(1U),
                11U),
      transfer(BackendOperationKind::stack_store, physical(1U), stack_cell(1U),
               12U),
  });
  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {41, 99};
  const std::vector<BackendInstructionOracleReply> replies = {{1U, 707}};

  const auto result = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, replies);

  REQUIRE(!result.suspended_at_instruction.has_value());
  REQUIRE_EQ(result.consumed_instruction_replies, std::size_t{1U});
  REQUIRE_EQ(result.after_block_registers[0], std::int64_t{41});
  REQUIRE_EQ(result.after_block_registers[1], std::int64_t{707});
  REQUIRE_EQ(result.frame_slot_values,
             (std::vector<std::int64_t>{41, 707}));
  REQUIRE_EQ(result.steps.size(), std::size_t{3U});
  REQUIRE_EQ(result.steps[1].kind, BackendOperationKind::instruction);
  REQUIRE_EQ(result.steps[1].instruction_inputs,
             (std::vector<std::int64_t>{41}));
  REQUIRE(result.steps[1].instruction_acknowledged);
  REQUIRE_EQ(result.steps[1].supplied_instruction_result,
             std::optional<std::int64_t>{707});
  REQUIRE_EQ(result.steps[2].transferred_value,
             std::optional<std::int64_t>{707});
  REQUIRE_EQ(registers, (std::vector<std::int64_t>{11, 22, 777, 1000}));
  REQUIRE_EQ(frame, (std::vector<std::int64_t>{41, 99}));
}

TEST_CASE(backend_instruction_continuation_supports_script_prefix_and_multiple_barriers) {
  const auto plan = canonical_plan({
      operation(BackendOperationKind::instruction, {physical(0U)}, physical(1U),
                20U),
      transfer(BackendOperationKind::register_move, physical(1U), physical(0U),
               21U),
      operation(BackendOperationKind::instruction,
                {physical(0U), physical(1U)}, physical(1U), 22U),
      transfer(BackendOperationKind::stack_store, physical(1U), stack_cell(0U),
               23U),
  });
  const std::vector<std::int64_t> registers = {5, 9, 700, 1000};
  const std::vector<std::int64_t> frame = {1, 2};

  const std::vector<BackendInstructionOracleReply> one_reply = {{0U, 31}};
  const auto partial = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, one_reply);
  REQUIRE_EQ(partial.suspended_at_instruction,
             std::optional<std::size_t>{2U});
  REQUIRE_EQ(partial.consumed_instruction_replies, std::size_t{1U});
  REQUIRE_EQ(partial.after_block_registers[0], std::int64_t{31});
  REQUIRE_EQ(partial.after_block_registers[1], std::int64_t{31});
  REQUIRE_EQ(partial.frame_slot_values, frame);
  REQUIRE_EQ(partial.steps.back().instruction_inputs,
             (std::vector<std::int64_t>{31, 31}));
  REQUIRE(!partial.steps.back().instruction_acknowledged);

  const std::vector<BackendInstructionOracleReply> all_replies = {
      {0U, 31}, {2U, -44}};
  const auto complete = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, all_replies);
  REQUIRE(!complete.suspended_at_instruction.has_value());
  REQUIRE_EQ(complete.consumed_instruction_replies, std::size_t{2U});
  REQUIRE_EQ(complete.frame_slot_values,
             (std::vector<std::int64_t>{-44, 2}));
  REQUIRE_EQ(complete.after_block_registers[1], std::int64_t{-44});
}

TEST_CASE(backend_instruction_continuation_validates_reply_shape_and_order) {
  const auto plan = canonical_plan({
      operation(BackendOperationKind::instruction, {physical(0U)}, physical(1U),
                30U),
      operation(BackendOperationKind::instruction, {physical(1U)}, std::nullopt,
                31U),
  });
  const std::vector<std::int64_t> registers = {5, 9, 700, 1000};
  const std::vector<std::int64_t> frame = {1, 2};

  const std::vector<BackendInstructionOracleReply> empty;
  const auto suspended = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, empty);
  REQUIRE_EQ(suspended.suspended_at_instruction,
             std::optional<std::size_t>{0U});
  REQUIRE_EQ(suspended.steps.back().instruction_inputs,
             (std::vector<std::int64_t>{5}));

  const std::vector<BackendInstructionOracleReply> wrong_index = {{1U, 8}};
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        plan, 0U, registers, frame, wrong_index),
                    std::invalid_argument);

  const std::vector<BackendInstructionOracleReply> missing_result = {
      {0U, std::nullopt}};
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        plan, 0U, registers, frame, missing_result),
                    std::invalid_argument);

  const std::vector<BackendInstructionOracleReply> value_for_no_output = {
      {0U, 8}, {1U, 9}};
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        plan, 0U, registers, frame, value_for_no_output),
                    std::invalid_argument);

  const std::vector<BackendInstructionOracleReply> correct = {
      {0U, 8}, {1U, std::nullopt}};
  const auto complete = execute_backend_instruction_continuation_block(
      plan, 0U, registers, frame, correct);
  REQUIRE(!complete.suspended_at_instruction.has_value());
  REQUIRE_EQ(complete.consumed_instruction_replies, std::size_t{2U});
  REQUIRE(complete.steps[1].instruction_acknowledged);
  REQUIRE(!complete.steps[1].supplied_instruction_result.has_value());

  const std::vector<BackendInstructionOracleReply> extra = {
      {0U, 8}, {1U, std::nullopt}, {1U, std::nullopt}};
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        plan, 0U, registers, frame, extra),
                    std::invalid_argument);
}

TEST_CASE(backend_instruction_continuation_preserves_phase62_validation_boundary) {
  const auto clean = canonical_plan({
      operation(BackendOperationKind::instruction, {physical(0U)}, physical(1U),
                40U),
  });
  const std::vector<std::int64_t> registers = {5, 9, 700, 1000};
  const std::vector<std::int64_t> frame = {1, 2};
  const std::vector<BackendInstructionOracleReply> replies = {{0U, 17}};

  auto byte_tamper = clean;
  byte_tamper.bytes.front() ^= 0xffU;
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        byte_tamper, 0U, registers, frame, replies),
                    std::invalid_argument);

  auto body_tamper = clean;
  auto& phase56 = body_tamper.source_plan.source_plan.source_plan.source_plan;
  auto& addressed =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  ++addressed.blocks[0].operations[0].origin_index;
  REQUIRE_THROWS_AS(execute_backend_instruction_continuation_block(
                        body_tamper, 0U, registers, frame, replies),
                    std::logic_error);
}

TEST_CASE(backend_instruction_continuation_integrates_real_upstream_instruction_plan) {
  const auto plan = upstream_spilled_instruction_plan();
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& addressed =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  const auto& operations = addressed.blocks[0].operations;

  std::vector<BackendInstructionOracleReply> replies;
  bool saw_reload = false;
  for (std::size_t index = 0U; index < operations.size(); ++index) {
    if (operations[index].kind == BackendOperationKind::stack_reload) {
      saw_reload = true;
    }
    if (operations[index].kind == BackendOperationKind::instruction) {
      REQUIRE(!operations[index].output.has_value());
      replies.push_back({index, std::nullopt});
    }
  }
  REQUIRE(saw_reload);
  REQUIRE(!replies.empty());

  std::vector<std::int64_t> initial_registers(
      phase56.total_physical_registers, 0);
  for (std::size_t index = 0U; index < initial_registers.size(); ++index) {
    initial_registers[index] = static_cast<std::int64_t>(100U + index);
  }
  std::vector<std::int64_t> initial_frame(addressed.stack_slots.size(), 0);
  for (std::size_t index = 0U; index < initial_frame.size(); ++index) {
    initial_frame[index] = static_cast<std::int64_t>(900U + index);
  }

  const auto result = execute_backend_instruction_continuation_block(
      plan, 0U, initial_registers, initial_frame, replies);
  REQUIRE(!result.suspended_at_instruction.has_value());
  REQUIRE_EQ(result.consumed_instruction_replies, replies.size());
  REQUIRE_EQ(static_cast<std::size_t>(std::count_if(
                 result.steps.begin(), result.steps.end(), [](const auto& step) {
                   return step.kind == BackendOperationKind::instruction &&
                          step.instruction_acknowledged;
                 })),
             replies.size());
}

TEST_CASE(backend_instruction_continuation_matches_independent_script_replay) {
  std::mt19937_64 rng(0x63C01171A710ULL);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000000,
                                                                 1000000);
  std::uniform_int_distribution<unsigned int> kind_distribution(0U, 4U);
  std::uniform_int_distribution<unsigned int> register_distribution(0U, 1U);
  std::uniform_int_distribution<unsigned int> slot_distribution(0U, 1U);

  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    std::vector<BackendOperation> operations;
    operations.reserve(60U);
    std::size_t instruction_count = 0U;
    for (std::size_t index = 0U; index < 60U; ++index) {
      const unsigned int kind = kind_distribution(rng);
      if (kind == 0U) {
        operations.push_back(transfer(
            BackendOperationKind::register_move,
            physical(static_cast<std::size_t>(register_distribution(rng))),
            physical(static_cast<std::size_t>(register_distribution(rng))),
            index));
      } else if (kind == 1U) {
        operations.push_back(transfer(
            BackendOperationKind::stack_reload,
            stack_cell(static_cast<std::size_t>(slot_distribution(rng))),
            physical(static_cast<std::size_t>(register_distribution(rng))),
            index));
      } else if (kind == 2U) {
        operations.push_back(transfer(
            BackendOperationKind::stack_store,
            physical(static_cast<std::size_t>(register_distribution(rng))),
            stack_cell(static_cast<std::size_t>(slot_distribution(rng))),
            index));
      } else {
        std::vector<BackendStorage> inputs;
        const std::size_t input_count = static_cast<std::size_t>(rng() % 3U);
        for (std::size_t input = 0U; input < input_count; ++input) {
          inputs.push_back(
              physical(static_cast<std::size_t>(register_distribution(rng))));
        }
        const bool has_output = kind == 3U;
        operations.push_back(operation(
            BackendOperationKind::instruction, std::move(inputs),
            has_output
                ? std::optional<BackendStorage>{physical(static_cast<std::size_t>(
                      register_distribution(rng)))}
                : std::nullopt,
            index));
        ++instruction_count;
      }
    }

    const auto plan = canonical_plan(operations);
    const std::vector<std::int64_t> initial_registers = {
        value_distribution(rng), value_distribution(rng),
        value_distribution(rng), value_distribution(rng)};
    const std::vector<std::int64_t> initial_frame = {
        value_distribution(rng), value_distribution(rng)};

    const std::size_t reply_limit = instruction_count == 0U
                                        ? 0U
                                        : static_cast<std::size_t>(
                                              rng() % (instruction_count + 1U));
    std::vector<BackendInstructionOracleReply> replies;
    replies.reserve(reply_limit);
    for (std::size_t index = 0U; index < operations.size() &&
                                 replies.size() < reply_limit;
         ++index) {
      if (operations[index].kind != BackendOperationKind::instruction) {
        continue;
      }
      replies.push_back(
          {index, operations[index].output.has_value()
                      ? std::optional<std::int64_t>{value_distribution(rng)}
                      : std::nullopt});
    }

    std::vector<std::int64_t> expected_registers =
        execute_backend_fixed_frame_bytecode(plan, initial_registers)
            .after_entry_registers;
    std::vector<std::int64_t> expected_frame = initial_frame;
    std::vector<BackendInstructionContinuationStep> expected_steps;
    std::size_t reply_index = 0U;
    std::optional<std::size_t> expected_suspension;

    for (std::size_t index = 0U; index < operations.size(); ++index) {
      const auto& item = operations[index];
      BackendInstructionContinuationStep step;
      step.operation_index = index;
      step.kind = item.kind;
      step.origin_kind = item.origin_kind;
      step.origin_index = item.origin_index;

      if (item.kind == BackendOperationKind::instruction) {
        for (const BackendStorage input : item.inputs) {
          step.instruction_inputs.push_back(expected_registers[input.index]);
        }
        if (reply_index >= replies.size()) {
          expected_steps.push_back(std::move(step));
          expected_suspension = index;
          break;
        }
        step.instruction_acknowledged = true;
        step.supplied_instruction_result = replies[reply_index].output_value;
        if (item.output.has_value()) {
          expected_registers[item.output->index] =
              *replies[reply_index].output_value;
        }
        ++reply_index;
        expected_steps.push_back(std::move(step));
        continue;
      }

      std::int64_t value = 0;
      if (item.kind == BackendOperationKind::stack_reload) {
        value = expected_frame[item.inputs.front().index];
        expected_registers[item.output->index] = value;
      } else if (item.kind == BackendOperationKind::stack_store) {
        value = expected_registers[item.inputs.front().index];
        expected_frame[item.output->index] = value;
      } else {
        value = expected_registers[item.inputs.front().index];
        expected_registers[item.output->index] = value;
      }
      step.transferred_value = value;
      expected_steps.push_back(std::move(step));
    }

    const auto actual = execute_backend_instruction_continuation_block(
        plan, 0U, initial_registers, initial_frame, replies);
    REQUIRE_EQ(actual.after_block_registers, expected_registers);
    REQUIRE_EQ(actual.frame_slot_values, expected_frame);
    REQUIRE_EQ(actual.consumed_instruction_replies, reply_index);
    REQUIRE_EQ(actual.suspended_at_instruction, expected_suspension);
    REQUIRE_EQ(actual.steps, expected_steps);
    REQUIRE_EQ(initial_registers.size(), std::size_t{4U});
    REQUIRE_EQ(initial_frame.size(), std::size_t{2U});
  }
}

}  // namespace
