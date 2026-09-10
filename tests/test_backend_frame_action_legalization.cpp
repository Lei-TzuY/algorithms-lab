#include "algorithms/graphs/backend_frame_action_legalization.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

BackendStackPointerMoveDirection opposite(
    const BackendStackPointerMoveDirection direction) {
  return direction == BackendStackPointerMoveDirection::toward_lower_addresses
             ? BackendStackPointerMoveDirection::toward_higher_addresses
             : BackendStackPointerMoveDirection::toward_lower_addresses;
}

void verify_legalized(const BackendFixedFrameActionPlan& source,
                      const BackendFixedFrameActionLegalityPolicy& policy,
                      const LegalizedBackendFixedFrameActionPlan& actual) {
  REQUIRE_EQ(actual.source_plan, source);
  REQUIRE_EQ(actual.policy, policy);

  if (source.entry_actions.empty()) {
    REQUIRE(actual.entry_actions.empty());
    REQUIRE(actual.exit_actions.empty());
    return;
  }

  REQUIRE_EQ(source.entry_actions.size(), std::size_t{2U});
  REQUIRE_EQ(source.exit_actions.size(), std::size_t{1U});
  const auto* original_entry =
      std::get_if<BackendStackPointerAdjustmentAction>(&source.entry_actions[0]);
  const auto* original_materialization =
      std::get_if<BackendFrameBaseMaterializationAction>(&source.entry_actions[1]);
  const auto* original_exit =
      std::get_if<BackendStackPointerAdjustmentAction>(&source.exit_actions[0]);
  REQUIRE(original_entry != nullptr);
  REQUIRE(original_materialization != nullptr);
  REQUIRE(original_exit != nullptr);

  REQUIRE(!actual.entry_actions.empty());
  const std::size_t chunk_count = actual.entry_actions.size() - 1U;
  REQUIRE_EQ(actual.exit_actions.size(), chunk_count);
  REQUIRE(chunk_count >= 1U);
  REQUIRE_EQ(actual.entry_actions.back(), source.entry_actions[1]);

  std::size_t total = 0U;
  for (std::size_t index = 0U; index < chunk_count; ++index) {
    const auto* chunk = std::get_if<BackendStackPointerAdjustmentAction>(
        &actual.entry_actions[index]);
    REQUIRE(chunk != nullptr);
    REQUIRE_EQ(chunk->stack_pointer_physical_register,
               original_entry->stack_pointer_physical_register);
    REQUIRE_EQ(chunk->direction, original_entry->direction);
    REQUIRE(chunk->byte_count <=
            policy.maximum_stack_adjustment_immediate_magnitude);
    if (original_entry->byte_count != 0U) {
      REQUIRE(chunk->byte_count != 0U);
    }
    REQUIRE(chunk->byte_count <= original_entry->byte_count - total);
    total += chunk->byte_count;

    const auto* inverse = std::get_if<BackendStackPointerAdjustmentAction>(
        &actual.exit_actions[chunk_count - 1U - index]);
    REQUIRE(inverse != nullptr);
    REQUIRE_EQ(inverse->stack_pointer_physical_register,
               original_exit->stack_pointer_physical_register);
    REQUIRE_EQ(inverse->direction, original_exit->direction);
    REQUIRE_EQ(inverse->direction, opposite(chunk->direction));
    REQUIRE_EQ(inverse->byte_count, chunk->byte_count);
  }
  REQUIRE_EQ(total, original_entry->byte_count);
  REQUIRE(original_materialization->displacement_from_adjusted_stack_pointer >=
          policy.minimum_frame_base_immediate);
  REQUIRE(original_materialization->displacement_from_adjusted_stack_pointer <=
          policy.maximum_frame_base_immediate);
}

TEST_CASE(backend_frame_action_legalization_rejects_bad_policy_and_tampering) {
  const auto source = manual_phase57_plan(
      16U, 4U, BackendStackGrowthDirection::toward_lower_addresses);
  const BackendFixedFrameActionLegalityPolicy valid{8U, -16, 16};

  REQUIRE_THROWS_AS(
      legalize_fixed_frame_actions(
          source, BackendFixedFrameActionLegalityPolicy{0U, -16, 16}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      legalize_fixed_frame_actions(
          source, BackendFixedFrameActionLegalityPolicy{8U, 2, 1}),
      std::invalid_argument);

  auto tampered = source;
  auto& adjustment =
      std::get<BackendStackPointerAdjustmentAction>(tampered.entry_actions[0]);
  ++adjustment.byte_count;
  REQUIRE_THROWS_AS(legalize_fixed_frame_actions(tampered, valid),
                    std::logic_error);

  tampered = source;
  auto& materialization = std::get<BackendFrameBaseMaterializationAction>(
      tampered.entry_actions[1]);
  ++materialization.frame_base_physical_register;
  REQUIRE_THROWS_AS(legalize_fixed_frame_actions(tampered, valid),
                    std::logic_error);
}

TEST_CASE(backend_frame_action_legalization_preserves_infeasible_empty_plans) {
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};
  const BackendFixedFrameActionLegalityPolicy policy{7U, -8, 8};

  for (const std::size_t registers : {0U, 1U}) {
    const auto owned = plan_stack_pointer_owned_backend_frame(
        one_block(0U), registers, layout, addressing,
        BackendStackGrowthDirection::toward_lower_addresses);
    const auto source = derive_fixed_frame_actions(owned);
    const auto actual = legalize_fixed_frame_actions(source, policy);
    REQUIRE(source.entry_actions.empty());
    REQUIRE(source.exit_actions.empty());
    verify_legalized(source, policy, actual);
    REQUIRE_EQ(legalize_fixed_frame_actions(source, policy), actual);
  }
}

TEST_CASE(backend_frame_action_legalization_chunks_both_stack_directions) {
  const BackendFixedFrameActionLegalityPolicy policy{4U, -7, 3};

  for (const BackendStackGrowthDirection direction : {
           BackendStackGrowthDirection::toward_lower_addresses,
           BackendStackGrowthDirection::toward_higher_addresses}) {
    const auto source = manual_phase57_plan(10U, 3U, direction);
    const auto actual = legalize_fixed_frame_actions(source, policy);
    verify_legalized(source, policy, actual);
    REQUIRE_EQ(actual.entry_actions.size(), std::size_t{4U});
    REQUIRE_EQ(actual.exit_actions.size(), std::size_t{3U});

    const std::size_t expected_entry[] = {4U, 4U, 2U};
    const std::size_t expected_exit[] = {2U, 4U, 4U};
    for (std::size_t index = 0U; index < 3U; ++index) {
      REQUIRE_EQ(
          std::get<BackendStackPointerAdjustmentAction>(
              actual.entry_actions[index])
              .byte_count,
          expected_entry[index]);
      REQUIRE_EQ(
          std::get<BackendStackPointerAdjustmentAction>(actual.exit_actions[index])
              .byte_count,
          expected_exit[index]);
    }
  }
}

TEST_CASE(backend_frame_action_legalization_checks_materialization_boundaries) {
  const auto lower = manual_phase57_plan(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses);
  const auto higher = manual_phase57_plan(
      10U, 3U, BackendStackGrowthDirection::toward_higher_addresses);

  const BackendFixedFrameActionLegalityPolicy lower_exact{64U, 3, 3};
  const BackendFixedFrameActionLegalityPolicy higher_exact{64U, -7, -7};
  verify_legalized(lower, lower_exact,
                   legalize_fixed_frame_actions(lower, lower_exact));
  verify_legalized(higher, higher_exact,
                   legalize_fixed_frame_actions(higher, higher_exact));

  REQUIRE_THROWS_AS(
      legalize_fixed_frame_actions(
          lower, BackendFixedFrameActionLegalityPolicy{64U, -10, 2}),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      legalize_fixed_frame_actions(
          higher, BackendFixedFrameActionLegalityPolicy{64U, -6, 10}),
      std::out_of_range);
}

TEST_CASE(backend_frame_action_legalization_replays_zero_and_int64_min_boundaries) {
  const auto zero = manual_phase57_plan(
      0U, 0U, BackendStackGrowthDirection::toward_lower_addresses);
  const BackendFixedFrameActionLegalityPolicy zero_policy{5U, 0, 0};
  const auto zero_actual = legalize_fixed_frame_actions(zero, zero_policy);
  verify_legalized(zero, zero_policy, zero_actual);
  REQUIRE_EQ(zero_actual.entry_actions.size(), std::size_t{2U});
  REQUIRE_EQ(zero_actual.exit_actions.size(), std::size_t{1U});
  REQUIRE_EQ(std::get<BackendStackPointerAdjustmentAction>(
                 zero_actual.entry_actions[0])
                 .byte_count,
             std::size_t{0U});

  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t huge =
        static_cast<std::size_t>(std::uint64_t{1U} << 63U);
    const std::size_t half =
        static_cast<std::size_t>(std::uint64_t{1U} << 62U);
    const auto source = manual_phase57_plan(
        huge, 0U, BackendStackGrowthDirection::toward_lower_addresses);
    const BackendFixedFrameActionLegalityPolicy policy{half, 0, 0};
    const auto actual = legalize_fixed_frame_actions(source, policy);
    verify_legalized(source, policy, actual);
    REQUIRE_EQ(actual.entry_actions.size(), std::size_t{3U});
    REQUIRE_EQ(actual.exit_actions.size(), std::size_t{2U});
    REQUIRE_EQ(std::get<BackendStackPointerAdjustmentAction>(
                   actual.entry_actions[0])
                   .byte_count,
               half);
    REQUIRE_EQ(std::get<BackendStackPointerAdjustmentAction>(
                   actual.entry_actions[1])
                   .byte_count,
               half);
  }
}

TEST_CASE(backend_frame_action_legalization_randomized_real_phase57_replay) {
  std::mt19937_64 random(0x58A6710ULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};

  for (std::size_t trial = 0U; trial < 320U; ++trial) {
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
      const auto owned = plan_stack_pointer_owned_backend_frame(
          program, total_registers, layout, addressing, direction);
      const auto source = derive_fixed_frame_actions(owned);
      const auto actual = legalize_fixed_frame_actions(source, policy);
      verify_legalized(source, policy, actual);
      REQUIRE_EQ(legalize_fixed_frame_actions(source, policy), actual);
    }
  }
}

}  // namespace
