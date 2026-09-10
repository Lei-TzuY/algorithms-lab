#include "algorithms/graphs/backend_stack_pointer_reservation.hpp"
#include "test_framework.hpp"

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

void require_storage_below(const BaseRelativeBackendStorage& storage,
                           const std::size_t limit) {
  if (storage.kind == BaseRelativeBackendStorageKind::physical_register) {
    REQUIRE(storage.physical_register < limit);
  }
}

void verify_register_ownership(const StackPointerOwnedBackendFramePlan& plan) {
  if (plan.total_physical_registers == 0U) {
    REQUIRE(!plan.stack_pointer_physical_register.has_value());
    REQUIRE(!plan.stack_directed_frame_plan.frame_base_plan
                 .frame_base_physical_register.has_value());
    REQUIRE(!plan.stack_directed_frame_plan.entry_setup.has_value());
    return;
  }

  REQUIRE(plan.stack_pointer_physical_register.has_value());
  const std::size_t stack_pointer = *plan.stack_pointer_physical_register;
  REQUIRE_EQ(stack_pointer + 1U, plan.total_physical_registers);
  const auto& base_plan = plan.stack_directed_frame_plan.frame_base_plan;
  REQUIRE_EQ(base_plan.total_physical_registers, stack_pointer);

  if (stack_pointer == 0U) {
    REQUIRE(!base_plan.frame_base_physical_register.has_value());
    REQUIRE(!base_plan.addressed_frame.has_value());
    REQUIRE(!plan.stack_directed_frame_plan.entry_setup.has_value());
    return;
  }

  REQUIRE(base_plan.frame_base_physical_register.has_value());
  const std::size_t frame_base = *base_plan.frame_base_physical_register;
  REQUIRE_EQ(frame_base + 1U, stack_pointer);
  REQUIRE(frame_base < stack_pointer);

  if (!base_plan.addressed_frame.has_value()) {
    REQUIRE(!plan.stack_directed_frame_plan.entry_setup.has_value());
    return;
  }

  REQUIRE(plan.stack_directed_frame_plan.entry_setup.has_value());
  REQUIRE_EQ(plan.stack_directed_frame_plan.entry_setup
                 ->frame_base_physical_register,
             frame_base);
  const auto& frame = *base_plan.addressed_frame;
  REQUIRE_EQ(frame.total_registers, frame_base);
  for (const std::size_t scratch : frame.scratch_physical_registers) {
    REQUIRE(scratch < frame_base);
  }
  for (const auto& item : frame.class_storage) {
    require_storage_below(item.storage, frame_base);
  }
  for (const auto& item : frame.location_storage) {
    require_storage_below(item.storage, frame_base);
  }
  for (const auto& block : frame.blocks) {
    for (const auto& operation : block.operations) {
      for (const auto& input : operation.inputs) {
        require_storage_below(input, frame_base);
      }
      if (operation.output.has_value()) {
        require_storage_below(*operation.output, frame_base);
      }
    }
  }
}

void verify_against_manual_composition(
    const OutOfSsaProgram& program, const std::size_t total_registers,
    const BackendFrameLayoutConfig layout,
    const BackendFrameAddressingConfig addressing,
    const BackendStackGrowthDirection direction,
    const StackPointerOwnedBackendFramePlan& actual) {
  REQUIRE_EQ(actual.total_physical_registers, total_registers);
  const std::size_t nested_registers =
      total_registers == 0U ? 0U : total_registers - 1U;
  if (total_registers == 0U) {
    REQUIRE(!actual.stack_pointer_physical_register.has_value());
  } else {
    REQUIRE_EQ(actual.stack_pointer_physical_register,
               std::optional<std::size_t>{total_registers - 1U});
  }

  const auto manual_base = plan_frame_base_reserved_backend(
      program, nested_registers, layout, addressing);
  const auto manual_directed =
      derive_stack_directed_backend_frame_entry(manual_base, direction);
  REQUIRE_EQ(actual.stack_directed_frame_plan, manual_directed);
  verify_register_ownership(actual);
}

TEST_CASE(backend_stack_pointer_ownership_handles_zero_one_and_two_registers) {
  const OutOfSsaProgram empty = one_block(0U);
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  for (const std::size_t registers : {0U, 1U, 2U}) {
    const auto plan = plan_stack_pointer_owned_backend_frame(
        empty, registers, layout, addressing,
        BackendStackGrowthDirection::toward_lower_addresses);
    verify_against_manual_composition(
        empty, registers, layout, addressing,
        BackendStackGrowthDirection::toward_lower_addresses, plan);
  }

  const auto two = plan_stack_pointer_owned_backend_frame(
      empty, 2U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE_EQ(two.stack_pointer_physical_register,
             std::optional<std::size_t>{1U});
  REQUIRE_EQ(two.stack_directed_frame_plan.frame_base_plan
                 .frame_base_physical_register,
             std::optional<std::size_t>{0U});
  REQUIRE(two.stack_directed_frame_plan.entry_setup.has_value());
}

TEST_CASE(backend_stack_pointer_ownership_preserves_spill_and_scratch_hierarchy) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  const BackendFrameLayoutConfig layout{6U, 4U, 16U};
  const BackendFrameAddressingConfig addressing{
      16U, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};

  const auto plan = plan_stack_pointer_owned_backend_frame(
      program, 4U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE_EQ(plan.stack_pointer_physical_register,
             std::optional<std::size_t>{3U});
  REQUIRE_EQ(plan.stack_directed_frame_plan.frame_base_plan
                 .frame_base_physical_register,
             std::optional<std::size_t>{2U});
  REQUIRE(plan.stack_directed_frame_plan.frame_base_plan.addressed_frame
              .has_value());
  REQUIRE_EQ(plan.stack_directed_frame_plan.frame_base_plan.addressed_frame
                 ->scratch_physical_registers,
             std::vector<std::size_t>{1U});
  verify_register_ownership(plan);
}

TEST_CASE(backend_stack_pointer_ownership_preserves_nested_register_shortage) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  const auto plan = plan_stack_pointer_owned_backend_frame(
      program, 3U, layout, addressing,
      BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE_EQ(plan.stack_pointer_physical_register,
             std::optional<std::size_t>{2U});
  REQUIRE_EQ(plan.stack_directed_frame_plan.frame_base_plan
                 .frame_base_physical_register,
             std::optional<std::size_t>{1U});
  REQUIRE(!plan.stack_directed_frame_plan.frame_base_plan.non_base_register_plan
               .selection.has_value());
  REQUIRE(!plan.stack_directed_frame_plan.frame_base_plan.addressed_frame
               .has_value());
  REQUIRE(!plan.stack_directed_frame_plan.entry_setup.has_value());
  verify_register_ownership(plan);
}

TEST_CASE(backend_stack_pointer_ownership_composes_both_stack_directions) {
  const OutOfSsaProgram empty = one_block(0U);
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  for (const BackendStackGrowthDirection direction : {
           BackendStackGrowthDirection::toward_lower_addresses,
           BackendStackGrowthDirection::toward_higher_addresses}) {
    const auto actual = plan_stack_pointer_owned_backend_frame(
        empty, 2U, layout, addressing, direction);
    verify_against_manual_composition(empty, 2U, layout, addressing, direction,
                                      actual);
    REQUIRE_EQ(plan_stack_pointer_owned_backend_frame(
                   empty, 2U, layout, addressing, direction),
               actual);
  }

  REQUIRE_THROWS_AS(plan_stack_pointer_owned_backend_frame(
                        empty, 2U, layout, addressing,
                        static_cast<BackendStackGrowthDirection>(255U)),
                    std::invalid_argument);
}

TEST_CASE(backend_stack_pointer_ownership_propagates_nested_configuration_errors) {
  const OutOfSsaProgram empty = one_block(0U);
  REQUIRE_THROWS_AS(plan_stack_pointer_owned_backend_frame(
                        empty, 2U, BackendFrameLayoutConfig{8U, 0U, 16U},
                        BackendFrameAddressingConfig{0U, -64, 64},
                        BackendStackGrowthDirection::toward_lower_addresses),
                    std::invalid_argument);
}

TEST_CASE(backend_stack_pointer_ownership_randomized_matches_manual_composition) {
  std::mt19937_64 random(0x56A11CEULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};
  for (std::size_t trial = 0U; trial < 320U; ++trial) {
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
        static_cast<std::size_t>(random() % 9U);
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
      const auto actual = plan_stack_pointer_owned_backend_frame(
          program, total_registers, layout, addressing, direction);
      verify_against_manual_composition(program, total_registers, layout,
                                        addressing, direction, actual);
      REQUIRE_EQ(plan_stack_pointer_owned_backend_frame(
                     program, total_registers, layout, addressing, direction),
                 actual);
    }
  }
}

}  // namespace
