#include "algorithms/graphs/backend_frame_actions.hpp"
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

BackendStackPointerMoveDirection expected_entry_direction(
    const BackendStackGrowthDirection direction) {
  return direction == BackendStackGrowthDirection::toward_lower_addresses
             ? BackendStackPointerMoveDirection::toward_lower_addresses
             : BackendStackPointerMoveDirection::toward_higher_addresses;
}

BackendStackPointerMoveDirection opposite(
    const BackendStackPointerMoveDirection direction) {
  return direction == BackendStackPointerMoveDirection::toward_lower_addresses
             ? BackendStackPointerMoveDirection::toward_higher_addresses
             : BackendStackPointerMoveDirection::toward_lower_addresses;
}

void verify_actions(const StackPointerOwnedBackendFramePlan& source,
                    const BackendFixedFrameActionPlan& actions) {
  REQUIRE_EQ(actions.source_plan, source);
  const auto& directed = source.stack_directed_frame_plan;
  if (!directed.entry_setup.has_value()) {
    REQUIRE(actions.entry_actions.empty());
    REQUIRE(actions.exit_actions.empty());
    return;
  }

  REQUIRE(source.stack_pointer_physical_register.has_value());
  REQUIRE_EQ(actions.entry_actions.size(), std::size_t{2U});
  REQUIRE_EQ(actions.exit_actions.size(), std::size_t{1U});

  const auto* allocate =
      std::get_if<BackendStackPointerAdjustmentAction>(&actions.entry_actions[0]);
  const auto* materialize =
      std::get_if<BackendFrameBaseMaterializationAction>(&actions.entry_actions[1]);
  const auto* restore =
      std::get_if<BackendStackPointerAdjustmentAction>(&actions.exit_actions[0]);
  REQUIRE(allocate != nullptr);
  REQUIRE(materialize != nullptr);
  REQUIRE(restore != nullptr);

  const auto& setup = *directed.entry_setup;
  const auto entry_direction = expected_entry_direction(setup.stack_growth_direction);
  REQUIRE_EQ(allocate->stack_pointer_physical_register,
             *source.stack_pointer_physical_register);
  REQUIRE_EQ(allocate->direction, entry_direction);
  REQUIRE_EQ(allocate->byte_count, setup.frame_size_bytes);

  REQUIRE_EQ(materialize->frame_base_physical_register,
             setup.frame_base_physical_register);
  REQUIRE_EQ(materialize->stack_pointer_physical_register,
             *source.stack_pointer_physical_register);
  REQUIRE_EQ(materialize->displacement_from_adjusted_stack_pointer,
             setup.frame_base_from_adjusted_stack_pointer);

  REQUIRE_EQ(restore->stack_pointer_physical_register,
             *source.stack_pointer_physical_register);
  REQUIRE_EQ(restore->direction, opposite(entry_direction));
  REQUIRE_EQ(restore->byte_count, allocate->byte_count);
}

TEST_CASE(backend_frame_actions_emit_no_actions_for_infeasible_frames) {
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  for (const std::size_t registers : {0U, 1U}) {
    const auto source = plan_stack_pointer_owned_backend_frame(
        one_block(0U), registers, layout, addressing,
        BackendStackGrowthDirection::toward_lower_addresses);
    const auto actions = derive_fixed_frame_actions(source);
    verify_actions(source, actions);
    REQUIRE(actions.entry_actions.empty());
    REQUIRE(actions.exit_actions.empty());
  }

  OutOfSsaProgram shortage = one_block(2U);
  shortage.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});
  const auto source = plan_stack_pointer_owned_backend_frame(
      shortage, 3U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE(!source.stack_directed_frame_plan.entry_setup.has_value());
  verify_actions(source, derive_fixed_frame_actions(source));
}

TEST_CASE(backend_frame_actions_sequence_both_stack_directions) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  const BackendFrameLayoutConfig layout{6U, 4U, 16U};
  const BackendFrameAddressingConfig addressing{
      8U, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};

  for (const BackendStackGrowthDirection direction : {
           BackendStackGrowthDirection::toward_lower_addresses,
           BackendStackGrowthDirection::toward_higher_addresses}) {
    const auto source = plan_stack_pointer_owned_backend_frame(
        program, 4U, layout, addressing, direction);
    REQUIRE(source.stack_directed_frame_plan.entry_setup.has_value());
    const auto actions = derive_fixed_frame_actions(source);
    verify_actions(source, actions);
    REQUIRE_EQ(derive_fixed_frame_actions(source), actions);
  }
}

TEST_CASE(backend_frame_actions_preserve_int64_min_inverse_as_direction_magnitude) {
  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t huge =
        static_cast<std::size_t>(std::uint64_t{1U} << 63U);

    StackPointerOwnedBackendFramePlan source;
    source.total_physical_registers = 2U;
    source.stack_pointer_physical_register = 1U;
    auto& directed = source.stack_directed_frame_plan;
    directed.stack_growth_direction =
        BackendStackGrowthDirection::toward_lower_addresses;
    directed.frame_base_plan.total_physical_registers = 1U;
    directed.frame_base_plan.frame_base_physical_register = 0U;

    ScratchAwareBaseRelativeBackendFrame frame;
    frame.total_registers = 0U;
    frame.frame_size_bytes = huge;
    frame.addressing_config.frame_base_byte_anchor = 0U;
    directed.frame_base_plan.addressed_frame = frame;

    StackDirectedBackendFrameEntrySetup setup;
    setup.stack_growth_direction =
        BackendStackGrowthDirection::toward_lower_addresses;
    setup.frame_base_physical_register = 0U;
    setup.frame_size_bytes = huge;
    setup.frame_base_byte_anchor = 0U;
    setup.stack_pointer_adjustment =
        std::numeric_limits<std::int64_t>::min();
    setup.frame_base_from_adjusted_stack_pointer = 0;
    setup.frame_base_from_entry_stack_pointer =
        std::numeric_limits<std::int64_t>::min();
    directed.entry_setup = setup;

    const auto actions = derive_fixed_frame_actions(source);
    verify_actions(source, actions);
    const auto& entry =
        std::get<BackendStackPointerAdjustmentAction>(actions.entry_actions[0]);
    const auto& exit =
        std::get<BackendStackPointerAdjustmentAction>(actions.exit_actions[0]);
    REQUIRE_EQ(entry.byte_count, huge);
    REQUIRE_EQ(exit.byte_count, huge);
    REQUIRE_EQ(entry.direction,
               BackendStackPointerMoveDirection::toward_lower_addresses);
    REQUIRE_EQ(exit.direction,
               BackendStackPointerMoveDirection::toward_higher_addresses);
  }
}

TEST_CASE(backend_frame_actions_reject_malformed_phase56_witnesses) {
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};
  auto valid = plan_stack_pointer_owned_backend_frame(
      one_block(0U), 2U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE(valid.stack_directed_frame_plan.entry_setup.has_value());

  auto wrong_stack = valid;
  wrong_stack.stack_pointer_physical_register = 0U;
  REQUIRE_THROWS_AS(derive_fixed_frame_actions(wrong_stack), std::logic_error);

  auto missing_frame = valid;
  missing_frame.stack_directed_frame_plan.frame_base_plan.addressed_frame.reset();
  REQUIRE_THROWS_AS(derive_fixed_frame_actions(missing_frame), std::logic_error);

  auto wrong_size = valid;
  ++wrong_size.stack_directed_frame_plan.entry_setup->frame_size_bytes;
  REQUIRE_THROWS_AS(derive_fixed_frame_actions(wrong_size), std::logic_error);

  auto unknown_direction = valid;
  unknown_direction.stack_directed_frame_plan.stack_growth_direction =
      static_cast<BackendStackGrowthDirection>(255U);
  REQUIRE_THROWS_AS(derive_fixed_frame_actions(unknown_direction),
                    std::invalid_argument);
}

TEST_CASE(backend_frame_actions_randomized_replay_phase56_coordinates) {
  std::mt19937_64 random(0x57AC710ULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};

  for (std::size_t trial = 0U; trial < 360U; ++trial) {
    const std::size_t count = 1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);
    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 6U);
    for (std::size_t index = 0U; index < instruction_count; ++index) {
      SsaInstruction instruction;
      for (std::size_t use = 0U;
           use < static_cast<std::size_t>(random() % (count + 2U)); ++use) {
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

    const std::size_t nested_registers =
        total_registers == 0U ? 0U : total_registers - 1U;
    const auto preliminary = plan_frame_base_reserved_backend(
        program, nested_registers, layout,
        {0U, std::numeric_limits<std::int64_t>::min(),
         std::numeric_limits<std::int64_t>::max()});
    std::size_t anchor = 0U;
    if (preliminary.addressed_frame.has_value()) {
      anchor = preliminary.addressed_frame->frame_size_bytes == 0U
                   ? 0U
                   : static_cast<std::size_t>(
                         random() %
                         (preliminary.addressed_frame->frame_size_bytes + 1U));
    }
    const BackendFrameAddressingConfig addressing{
        anchor, std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max()};

    for (const BackendStackGrowthDirection direction : {
             BackendStackGrowthDirection::toward_lower_addresses,
             BackendStackGrowthDirection::toward_higher_addresses}) {
      const auto source = plan_stack_pointer_owned_backend_frame(
          program, total_registers, layout, addressing, direction);
      const auto actual = derive_fixed_frame_actions(source);
      verify_actions(source, actual);
      REQUIRE_EQ(derive_fixed_frame_actions(source), actual);
    }
  }
}

}  // namespace
