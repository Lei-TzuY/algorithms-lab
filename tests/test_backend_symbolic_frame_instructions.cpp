#include "algorithms/graphs/backend_symbolic_frame_instructions.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace algorithms::graphs;

OutOfSsaProgram one_block(const std::size_t variables) {
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

std::int64_t size_to_i64(const std::size_t value) {
  if (value > static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("test size is not int64 representable");
  }
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_add(const std::int64_t left, const std::int64_t right) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error("test replay positive overflow");
  }
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error("test replay negative overflow");
  }
  return left + right;
}

BackendFixedFrameActionPlan manual_phase57_plan(
    const std::size_t frame_size, const std::size_t anchor,
    const BackendStackGrowthDirection direction) {
  if (anchor > frame_size) {
    throw std::invalid_argument("test frame anchor lies outside frame");
  }

  StackPointerOwnedBackendFramePlan source;
  source.total_physical_registers = 2U;
  source.stack_pointer_physical_register = 1U;

  auto& directed = source.stack_directed_frame_plan;
  directed.stack_growth_direction = direction;
  directed.frame_base_plan.total_physical_registers = 1U;
  directed.frame_base_plan.frame_base_physical_register = 0U;

  ScratchAwareBaseRelativeBackendFrame frame;
  frame.frame_size_bytes = frame_size;
  frame.addressing_config.frame_base_byte_anchor = anchor;
  directed.frame_base_plan.addressed_frame = frame;

  StackDirectedBackendFrameEntrySetup setup;
  setup.stack_growth_direction = direction;
  setup.frame_base_physical_register = 0U;
  setup.frame_size_bytes = frame_size;
  setup.frame_base_byte_anchor = anchor;

  if (direction == BackendStackGrowthDirection::toward_lower_addresses) {
    if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
      const std::size_t min_magnitude =
          static_cast<std::size_t>(std::uint64_t{1U} << 63U);
      if (frame_size == min_magnitude) {
        setup.stack_pointer_adjustment =
            std::numeric_limits<std::int64_t>::min();
        setup.frame_base_from_adjusted_stack_pointer = size_to_i64(anchor);
        setup.frame_base_from_entry_stack_pointer =
            anchor == 0U ? std::numeric_limits<std::int64_t>::min()
                         : static_cast<std::int64_t>(
                               std::numeric_limits<std::int64_t>::min() +
                               size_to_i64(anchor));
        directed.entry_setup = setup;
        return derive_fixed_frame_actions(source);
      }
    }

    const std::int64_t signed_frame_size = size_to_i64(frame_size);
    const std::int64_t signed_anchor = size_to_i64(anchor);
    setup.stack_pointer_adjustment = -signed_frame_size;
    setup.frame_base_from_adjusted_stack_pointer = signed_anchor;
    setup.frame_base_from_entry_stack_pointer =
        signed_anchor - signed_frame_size;
  } else {
    const std::int64_t signed_frame_size = size_to_i64(frame_size);
    const std::int64_t signed_anchor = size_to_i64(anchor);
    setup.stack_pointer_adjustment = signed_frame_size;
    setup.frame_base_from_adjusted_stack_pointer =
        signed_anchor - signed_frame_size;
    setup.frame_base_from_entry_stack_pointer = signed_anchor;
  }

  directed.entry_setup = setup;
  return derive_fixed_frame_actions(source);
}

struct RegisterState {
  std::vector<std::int64_t> registers;
};

std::int64_t& register_value(RegisterState& state, const std::size_t id) {
  if (id >= state.registers.size()) {
    state.registers.resize(id + 1U, 0);
  }
  return state.registers[id];
}

void apply_stack_move(RegisterState& state, const std::size_t register_id,
                      const BackendStackPointerMoveDirection direction,
                      const std::size_t byte_count) {
  const std::int64_t magnitude = size_to_i64(byte_count);
  const std::int64_t delta =
      direction == BackendStackPointerMoveDirection::toward_lower_addresses
          ? -magnitude
          : magnitude;
  auto& value = register_value(state, register_id);
  value = checked_add(value, delta);
}

void replay_action(const BackendFixedFrameAction& action, RegisterState& state) {
  if (const auto* adjustment =
          std::get_if<BackendStackPointerAdjustmentAction>(&action)) {
    apply_stack_move(state, adjustment->stack_pointer_physical_register,
                     adjustment->direction, adjustment->byte_count);
    return;
  }
  const auto* materialization =
      std::get_if<BackendFrameBaseMaterializationAction>(&action);
  REQUIRE(materialization != nullptr);
  const std::int64_t stack_pointer =
      register_value(state, materialization->stack_pointer_physical_register);
  register_value(state, materialization->frame_base_physical_register) =
      checked_add(stack_pointer,
                  materialization->displacement_from_adjusted_stack_pointer);
}

void replay_instruction(const BackendSymbolicFixedFrameInstruction& instruction,
                        RegisterState& state) {
  if (const auto* decrement =
          std::get_if<BackendStackPointerDecrementImmediateInstruction>(
              &instruction)) {
    apply_stack_move(
        state, decrement->stack_pointer_physical_register,
        BackendStackPointerMoveDirection::toward_lower_addresses,
        decrement->immediate_byte_count);
    return;
  }
  if (const auto* increment =
          std::get_if<BackendStackPointerIncrementImmediateInstruction>(
              &instruction)) {
    apply_stack_move(
        state, increment->stack_pointer_physical_register,
        BackendStackPointerMoveDirection::toward_higher_addresses,
        increment->immediate_byte_count);
    return;
  }
  const auto* materialization =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &instruction);
  REQUIRE(materialization != nullptr);
  const std::int64_t stack_pointer =
      register_value(state, materialization->stack_pointer_physical_register);
  register_value(state, materialization->frame_base_physical_register) =
      checked_add(stack_pointer, materialization->immediate_displacement);
}

void replay_actions(const std::vector<BackendFixedFrameAction>& actions,
                    RegisterState& state) {
  for (const auto& action : actions) {
    replay_action(action, state);
  }
}

void replay_instructions(
    const std::vector<BackendSymbolicFixedFrameInstruction>& instructions,
    RegisterState& state) {
  for (const auto& instruction : instructions) {
    replay_instruction(instruction, state);
  }
}

TEST_CASE(backend_symbolic_frame_instructions_reject_tampered_phase58_witness) {
  const auto phase57 = manual_phase57_plan(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses);
  const BackendFixedFrameActionLegalityPolicy policy{4U, -16, 16};
  const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
  const auto lowered =
      lower_fixed_frame_actions_to_symbolic_instructions(phase58);
  REQUIRE_EQ(lowered.source_plan, phase58);

  auto tampered = phase58;
  ++std::get<BackendStackPointerAdjustmentAction>(tampered.entry_actions[0])
        .byte_count;
  REQUIRE_THROWS_AS(
      lower_fixed_frame_actions_to_symbolic_instructions(tampered),
      std::logic_error);

  tampered = phase58;
  tampered.policy.maximum_stack_adjustment_immediate_magnitude = 5U;
  REQUIRE_THROWS_AS(
      lower_fixed_frame_actions_to_symbolic_instructions(tampered),
      std::logic_error);
}

TEST_CASE(backend_symbolic_frame_instructions_select_explicit_forms) {
  const BackendFixedFrameActionLegalityPolicy policy{4U, -16, 16};
  for (const BackendStackGrowthDirection direction : {
           BackendStackGrowthDirection::toward_lower_addresses,
           BackendStackGrowthDirection::toward_higher_addresses}) {
    const auto phase57 = manual_phase57_plan(10U, 3U, direction);
    const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
    const auto actual =
        lower_fixed_frame_actions_to_symbolic_instructions(phase58);

    REQUIRE_EQ(actual.entry_instructions.size(), std::size_t{4U});
    REQUIRE_EQ(actual.exit_instructions.size(), std::size_t{3U});
    const std::size_t entry_immediates[] = {4U, 4U, 2U};
    const std::size_t exit_immediates[] = {2U, 4U, 4U};

    for (std::size_t index = 0U; index < 3U; ++index) {
      if (direction == BackendStackGrowthDirection::toward_lower_addresses) {
        const auto* entry =
            std::get_if<BackendStackPointerDecrementImmediateInstruction>(
                &actual.entry_instructions[index]);
        const auto* exit =
            std::get_if<BackendStackPointerIncrementImmediateInstruction>(
                &actual.exit_instructions[index]);
        REQUIRE(entry != nullptr);
        REQUIRE(exit != nullptr);
        REQUIRE_EQ(entry->stack_pointer_physical_register, std::size_t{1U});
        REQUIRE_EQ(exit->stack_pointer_physical_register, std::size_t{1U});
        REQUIRE_EQ(entry->immediate_byte_count, entry_immediates[index]);
        REQUIRE_EQ(exit->immediate_byte_count, exit_immediates[index]);
      } else {
        const auto* entry =
            std::get_if<BackendStackPointerIncrementImmediateInstruction>(
                &actual.entry_instructions[index]);
        const auto* exit =
            std::get_if<BackendStackPointerDecrementImmediateInstruction>(
                &actual.exit_instructions[index]);
        REQUIRE(entry != nullptr);
        REQUIRE(exit != nullptr);
        REQUIRE_EQ(entry->stack_pointer_physical_register, std::size_t{1U});
        REQUIRE_EQ(exit->stack_pointer_physical_register, std::size_t{1U});
        REQUIRE_EQ(entry->immediate_byte_count, entry_immediates[index]);
        REQUIRE_EQ(exit->immediate_byte_count, exit_immediates[index]);
      }
    }

    const auto* frame_base =
        std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
            &actual.entry_instructions[3]);
    REQUIRE(frame_base != nullptr);
    REQUIRE_EQ(frame_base->frame_base_physical_register, std::size_t{0U});
    REQUIRE_EQ(frame_base->stack_pointer_physical_register, std::size_t{1U});
    REQUIRE_EQ(frame_base->immediate_displacement,
               direction == BackendStackGrowthDirection::toward_lower_addresses
                   ? std::int64_t{3}
                   : std::int64_t{-7});
  }
}

TEST_CASE(backend_symbolic_frame_instructions_preserve_empty_and_boundaries) {
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};
  const BackendFixedFrameActionLegalityPolicy policy{7U, -16, 16};
  const auto owned = plan_stack_pointer_owned_backend_frame(
      one_block(0U), 0U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  const auto empty_phase57 = derive_fixed_frame_actions(owned);
  const auto empty_phase58 =
      legalize_fixed_frame_actions(empty_phase57, policy);
  const auto empty =
      lower_fixed_frame_actions_to_symbolic_instructions(empty_phase58);
  REQUIRE(empty.entry_instructions.empty());
  REQUIRE(empty.exit_instructions.empty());
  REQUIRE_EQ(empty.source_plan, empty_phase58);

  const auto zero_phase57 = manual_phase57_plan(
      0U, 0U, BackendStackGrowthDirection::toward_lower_addresses);
  const auto zero_phase58 = legalize_fixed_frame_actions(
      zero_phase57, BackendFixedFrameActionLegalityPolicy{5U, 0, 0});
  const auto zero =
      lower_fixed_frame_actions_to_symbolic_instructions(zero_phase58);
  REQUIRE_EQ(zero.entry_instructions.size(), std::size_t{2U});
  REQUIRE_EQ(zero.exit_instructions.size(), std::size_t{1U});
  REQUIRE_EQ(
      std::get<BackendStackPointerDecrementImmediateInstruction>(
          zero.entry_instructions[0])
          .immediate_byte_count,
      std::size_t{0U});
  REQUIRE_EQ(
      std::get<BackendStackPointerIncrementImmediateInstruction>(
          zero.exit_instructions[0])
          .immediate_byte_count,
      std::size_t{0U});

  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t huge =
        static_cast<std::size_t>(std::uint64_t{1U} << 63U);
    const std::size_t half =
        static_cast<std::size_t>(std::uint64_t{1U} << 62U);
    const auto huge_phase57 = manual_phase57_plan(
        huge, 0U, BackendStackGrowthDirection::toward_lower_addresses);
    const auto huge_phase58 = legalize_fixed_frame_actions(
        huge_phase57, BackendFixedFrameActionLegalityPolicy{half, 0, 0});
    const auto huge_actual =
        lower_fixed_frame_actions_to_symbolic_instructions(huge_phase58);
    REQUIRE_EQ(huge_actual.entry_instructions.size(), std::size_t{3U});
    REQUIRE_EQ(huge_actual.exit_instructions.size(), std::size_t{2U});
    REQUIRE_EQ(
        std::get<BackendStackPointerDecrementImmediateInstruction>(
            huge_actual.entry_instructions[0])
            .immediate_byte_count,
        half);
    REQUIRE_EQ(
        std::get<BackendStackPointerDecrementImmediateInstruction>(
            huge_actual.entry_instructions[1])
            .immediate_byte_count,
        half);
    REQUIRE_EQ(
        std::get<BackendStackPointerIncrementImmediateInstruction>(
            huge_actual.exit_instructions[0])
            .immediate_byte_count,
        half);
  }
}

TEST_CASE(backend_symbolic_frame_instructions_randomized_upstream_replay) {
  std::mt19937_64 random(0x59B011CULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};

  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const std::size_t count = 1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);
    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 6U);
    for (std::size_t index = 0U; index < instruction_count; ++index) {
      SsaInstruction instruction;
      const std::size_t use_count =
          static_cast<std::size_t>(random() % (count + 2U));
      for (std::size_t use = 0U; use < use_count; ++use) {
        instruction.uses.push_back(
            {static_cast<std::size_t>(random() % count), 0U});
      }
      if ((random() & 1U) != 0U) {
        instruction.definition =
            SsaValue{static_cast<std::size_t>(random() % count), 0U};
      }
      program.blocks[0].instructions.push_back(std::move(instruction));
    }

    const std::size_t total_registers =
        static_cast<std::size_t>(random() % 10U);
    const std::size_t slot_alignment =
        alignments[static_cast<std::size_t>(random() % 5U)];
    std::size_t frame_alignment = slot_alignment;
    while (frame_alignment < 32U && (random() & 1U) != 0U) {
      frame_alignment *= 2U;
    }
    const BackendFrameLayoutConfig layout{
        1U + static_cast<std::size_t>(random() % 24U), slot_alignment,
        frame_alignment};
    const BackendFrameAddressingConfig addressing{
        0U, std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max()};
    const BackendFixedFrameActionLegalityPolicy policy{
        1U + static_cast<std::size_t>(random() % 31U),
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max()};

    for (const BackendStackGrowthDirection direction : {
             BackendStackGrowthDirection::toward_lower_addresses,
             BackendStackGrowthDirection::toward_higher_addresses}) {
      const auto owned_plan = plan_stack_pointer_owned_backend_frame(
          program, total_registers, layout, addressing, direction);
      const auto phase57 = derive_fixed_frame_actions(owned_plan);
      const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
      const auto actual =
          lower_fixed_frame_actions_to_symbolic_instructions(phase58);

      REQUIRE_EQ(actual.source_plan, phase58);
      REQUIRE_EQ(actual.entry_instructions.size(), phase58.entry_actions.size());
      REQUIRE_EQ(actual.exit_instructions.size(), phase58.exit_actions.size());
      REQUIRE_EQ(lower_fixed_frame_actions_to_symbolic_instructions(phase58),
                 actual);

      if (phase58.entry_actions.empty()) {
        REQUIRE(actual.entry_instructions.empty());
        REQUIRE(actual.exit_instructions.empty());
        continue;
      }

      const auto* entry_adjustment =
          std::get_if<BackendStackPointerAdjustmentAction>(
              &phase58.entry_actions.front());
      REQUIRE(entry_adjustment != nullptr);

      RegisterState action_state;
      RegisterState instruction_state;
      register_value(action_state,
                     entry_adjustment->stack_pointer_physical_register) =
          1000000;
      register_value(instruction_state,
                     entry_adjustment->stack_pointer_physical_register) =
          1000000;

      replay_actions(phase58.entry_actions, action_state);
      replay_instructions(actual.entry_instructions, instruction_state);
      REQUIRE_EQ(instruction_state.registers, action_state.registers);

      replay_actions(phase58.exit_actions, action_state);
      replay_instructions(actual.exit_instructions, instruction_state);
      REQUIRE_EQ(instruction_state.registers, action_state.registers);
      REQUIRE_EQ(
          register_value(instruction_state,
                         entry_adjustment->stack_pointer_physical_register),
          std::int64_t{1000000});
    }
  }
}

}  // namespace
