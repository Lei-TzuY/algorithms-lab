#include "algorithms/graphs/backend_frame_entry_setup.hpp"
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

std::int64_t reference_signed_difference(const std::size_t first,
                                         const std::size_t second) {
  if (first >= second) {
    const std::uintmax_t magnitude =
        static_cast<std::uintmax_t>(first - second);
    REQUIRE(magnitude <= static_cast<std::uintmax_t>(
                             std::numeric_limits<std::int64_t>::max()));
    return static_cast<std::int64_t>(first - second);
  }
  const std::size_t magnitude = second - first;
  const std::uintmax_t wide = static_cast<std::uintmax_t>(magnitude);
  const std::uintmax_t negative_limit =
      static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
  REQUIRE(wide <= negative_limit);
  if (wide == negative_limit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(magnitude);
}

void verify_setup(const FrameBaseReservedBackendPlan& source,
                  const BackendStackGrowthDirection direction,
                  const StackDirectedBackendFramePlan& actual) {
  REQUIRE_EQ(actual.frame_base_plan, source);
  REQUIRE_EQ(actual.stack_growth_direction, direction);
  if (!source.addressed_frame.has_value()) {
    REQUIRE(!actual.entry_setup.has_value());
    return;
  }

  REQUIRE(actual.entry_setup.has_value());
  const auto& setup = *actual.entry_setup;
  const auto& frame = *source.addressed_frame;
  const std::size_t frame_size = frame.frame_size_bytes;
  const std::size_t anchor = frame.addressing_config.frame_base_byte_anchor;
  REQUIRE_EQ(setup.frame_base_physical_register,
             *source.frame_base_physical_register);
  REQUIRE_EQ(setup.frame_size_bytes, frame_size);
  REQUIRE_EQ(setup.frame_base_byte_anchor, anchor);
  REQUIRE_EQ(setup.stack_growth_direction, direction);

  if (direction == BackendStackGrowthDirection::toward_lower_addresses) {
    REQUIRE_EQ(setup.stack_pointer_adjustment,
               reference_signed_difference(0U, frame_size));
    REQUIRE_EQ(setup.frame_base_from_adjusted_stack_pointer,
               reference_signed_difference(anchor, 0U));
    REQUIRE_EQ(setup.frame_base_from_entry_stack_pointer,
               reference_signed_difference(anchor, frame_size));
  } else {
    REQUIRE_EQ(setup.stack_pointer_adjustment,
               reference_signed_difference(frame_size, 0U));
    REQUIRE_EQ(setup.frame_base_from_adjusted_stack_pointer,
               reference_signed_difference(anchor, frame_size));
    REQUIRE_EQ(setup.frame_base_from_entry_stack_pointer,
               reference_signed_difference(anchor, 0U));
  }

  REQUIRE_EQ(setup.stack_slots.size(), frame.stack_slots.size());
  for (std::size_t index = 0U; index < frame.stack_slots.size(); ++index) {
    const auto& source_slot = frame.stack_slots[index];
    const auto& slot = setup.stack_slots[index];
    REQUIRE_EQ(slot.stack_slot, source_slot.stack_slot);
    REQUIRE_EQ(slot.byte_offset, source_slot.byte_offset);
    REQUIRE_EQ(slot.byte_size, source_slot.byte_size);
    REQUIRE_EQ(slot.frame_base_displacement, source_slot.displacement);
    const std::int64_t direct =
        direction == BackendStackGrowthDirection::toward_lower_addresses
            ? reference_signed_difference(source_slot.byte_offset, frame_size)
            : reference_signed_difference(source_slot.byte_offset, 0U);
    REQUIRE_EQ(slot.entry_stack_pointer_displacement, direct);
    REQUIRE_EQ(setup.frame_base_from_entry_stack_pointer +
                   source_slot.displacement,
               direct);
  }
}

TEST_CASE(backend_frame_entry_setup_preserves_infeasible_phase54_plans) {
  const OutOfSsaProgram empty = one_block(0U);
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  const auto zero = plan_frame_base_reserved_backend(empty, 0U, layout, addressing);
  verify_setup(zero, BackendStackGrowthDirection::toward_lower_addresses,
               derive_stack_directed_backend_frame_entry(
                   zero, BackendStackGrowthDirection::toward_lower_addresses));
  verify_setup(zero, BackendStackGrowthDirection::toward_higher_addresses,
               derive_stack_directed_backend_frame_entry(
                   zero, BackendStackGrowthDirection::toward_higher_addresses));

  OutOfSsaProgram two_values = one_block(2U);
  two_values.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});
  const auto shortage = plan_frame_base_reserved_backend(
      two_values, 2U, layout, addressing);
  REQUIRE(!shortage.addressed_frame.has_value());
  verify_setup(shortage, BackendStackGrowthDirection::toward_lower_addresses,
               derive_stack_directed_backend_frame_entry(
                   shortage, BackendStackGrowthDirection::toward_lower_addresses));
}

TEST_CASE(backend_frame_entry_setup_composes_both_stack_directions) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  const BackendFrameLayoutConfig layout{6U, 4U, 16U};

  const auto preliminary = plan_frame_base_reserved_backend(
      program, 3U, layout,
      {0U, std::numeric_limits<std::int64_t>::min(),
       std::numeric_limits<std::int64_t>::max()});
  REQUIRE(preliminary.addressed_frame.has_value());
  const std::size_t anchor = preliminary.addressed_frame->frame_size_bytes / 2U;
  const auto source = plan_frame_base_reserved_backend(
      program, 3U, layout,
      {anchor, std::numeric_limits<std::int64_t>::min(),
       std::numeric_limits<std::int64_t>::max()});

  const auto lower = derive_stack_directed_backend_frame_entry(
      source, BackendStackGrowthDirection::toward_lower_addresses);
  const auto higher = derive_stack_directed_backend_frame_entry(
      source, BackendStackGrowthDirection::toward_higher_addresses);
  verify_setup(source, BackendStackGrowthDirection::toward_lower_addresses,
               lower);
  verify_setup(source, BackendStackGrowthDirection::toward_higher_addresses,
               higher);
  REQUIRE_EQ(derive_stack_directed_backend_frame_entry(
                 source, BackendStackGrowthDirection::toward_lower_addresses),
             lower);
}

TEST_CASE(backend_frame_entry_setup_rejects_malformed_source_and_unknown_direction) {
  const OutOfSsaProgram empty = one_block(0U);
  auto source = plan_frame_base_reserved_backend(
      empty, 1U, BackendFrameLayoutConfig{8U, 8U, 16U},
      BackendFrameAddressingConfig{0U, -64, 64});
  REQUIRE(source.addressed_frame.has_value());

  auto wrong_base = source;
  wrong_base.frame_base_physical_register = 1U;
  REQUIRE_THROWS_AS(derive_stack_directed_backend_frame_entry(
                        wrong_base,
                        BackendStackGrowthDirection::toward_lower_addresses),
                    std::logic_error);

  auto mismatched_presence = source;
  mismatched_presence.byte_addressed_frame.reset();
  REQUIRE_THROWS_AS(derive_stack_directed_backend_frame_entry(
                        mismatched_presence,
                        BackendStackGrowthDirection::toward_lower_addresses),
                    std::logic_error);

  REQUIRE_THROWS_AS(derive_stack_directed_backend_frame_entry(
                        source,
                        static_cast<BackendStackGrowthDirection>(255U)),
                    std::invalid_argument);
}

TEST_CASE(backend_frame_entry_setup_checked_signed_boundaries) {
#if SIZE_MAX > INT64_MAX
  const std::size_t huge =
      static_cast<std::size_t>(std::uint64_t{1U} << 63U);
  FrameBaseReservedBackendPlan source;
  source.total_physical_registers = 1U;
  source.frame_base_physical_register = 0U;
  source.non_base_register_plan.total_registers = 0U;

  ScratchAwareByteAddressedBackendFrame byte_frame;
  byte_frame.total_registers = 0U;
  byte_frame.frame_size_bytes = huge;
  source.byte_addressed_frame = byte_frame;

  ScratchAwareBaseRelativeBackendFrame addressed;
  addressed.total_registers = 0U;
  addressed.frame_size_bytes = huge;
  addressed.addressing_config.frame_base_byte_anchor = 0U;
  source.addressed_frame = addressed;

  const auto lower = derive_stack_directed_backend_frame_entry(
      source, BackendStackGrowthDirection::toward_lower_addresses);
  REQUIRE(lower.entry_setup.has_value());
  REQUIRE_EQ(lower.entry_setup->stack_pointer_adjustment,
             std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(lower.entry_setup->frame_base_from_entry_stack_pointer,
             std::numeric_limits<std::int64_t>::min());
  REQUIRE_THROWS_AS(derive_stack_directed_backend_frame_entry(
                        source,
                        BackendStackGrowthDirection::toward_higher_addresses),
                    std::overflow_error);
#endif
}

TEST_CASE(backend_frame_entry_setup_randomized_replays_phase54_coordinates) {
  std::mt19937_64 random(0x55E17AULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};
  for (std::size_t trial = 0U; trial < 300U; ++trial) {
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
        static_cast<std::size_t>(random() % 8U);
    const std::size_t slot_alignment =
        alignments[static_cast<std::size_t>(random() % 5U)];
    std::size_t frame_alignment = slot_alignment;
    while (frame_alignment < 32U && (random() & 1U) != 0U) {
      frame_alignment *= 2U;
    }
    const BackendFrameLayoutConfig layout{
        1U + static_cast<std::size_t>(random() % 24U), slot_alignment,
        frame_alignment};

    const auto preliminary = plan_frame_base_reserved_backend(
        program, total_registers, layout,
        {0U, std::numeric_limits<std::int64_t>::min(),
         std::numeric_limits<std::int64_t>::max()});
    std::size_t anchor = 0U;
    if (preliminary.addressed_frame.has_value()) {
      anchor = preliminary.addressed_frame->frame_size_bytes == 0U
                   ? 0U
                   : static_cast<std::size_t>(
                         random() % (preliminary.addressed_frame->frame_size_bytes + 1U));
    }
    const auto source = plan_frame_base_reserved_backend(
        program, total_registers, layout,
        {anchor, std::numeric_limits<std::int64_t>::min(),
         std::numeric_limits<std::int64_t>::max()});

    for (const BackendStackGrowthDirection direction : {
             BackendStackGrowthDirection::toward_lower_addresses,
             BackendStackGrowthDirection::toward_higher_addresses}) {
      const auto actual =
          derive_stack_directed_backend_frame_entry(source, direction);
      verify_setup(source, direction, actual);
      REQUIRE_EQ(derive_stack_directed_backend_frame_entry(source, direction),
                 actual);
    }
  }
}

}  // namespace
