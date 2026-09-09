#include "algorithms/graphs/backend_frame_addressing.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
using namespace algorithms::graphs;

SsaCopyLocation loc(std::size_t variable) {
  return SsaCopyLocation::from_value({variable, 0U});
}

OutOfSsaProgram one_block(std::size_t variables) {
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

BaseRelativeBackendStorage expected_storage(
    ByteAddressedBackendStorage source,
    const ScratchAwareBaseRelativeBackendFrame& addressed) {
  if (source.kind == ByteAddressedBackendStorageKind::physical_register) {
    return {BaseRelativeBackendStorageKind::physical_register, source.index, 0};
  }
  REQUIRE_EQ(source.kind, ByteAddressedBackendStorageKind::frame_byte_offset);
  for (const auto& slot : addressed.stack_slots) {
    if (slot.byte_offset == source.index) {
      return {BaseRelativeBackendStorageKind::frame_base_displacement, 0U,
              slot.displacement};
    }
  }
  REQUIRE(false);
  return {};
}

void verify_mapping(const ScratchAwareByteAddressedBackendFrame& frame,
                    const ScratchAwareBaseRelativeBackendFrame& addressed) {
  REQUIRE_EQ(addressed.frame_layout_config, frame.config);
  REQUIRE_EQ(addressed.total_registers, frame.total_registers);
  REQUIRE_EQ(addressed.reserved_scratch_registers,
             frame.reserved_scratch_registers);
  REQUIRE_EQ(addressed.allocatable_registers, frame.allocatable_registers);
  REQUIRE_EQ(addressed.scratch_physical_registers,
             frame.scratch_physical_registers);
  REQUIRE_EQ(addressed.frame_size_bytes, frame.frame_size_bytes);
  REQUIRE_EQ(addressed.stack_slots.size(), frame.stack_slots.size());

  for (std::size_t index = 0U; index < frame.stack_slots.size(); ++index) {
    const auto& source = frame.stack_slots[index];
    const auto& target = addressed.stack_slots[index];
    REQUIRE_EQ(target.stack_slot, source.stack_slot);
    REQUIRE_EQ(target.byte_offset, source.byte_offset);
    REQUIRE_EQ(target.byte_size, source.byte_size);
    REQUIRE(target.displacement >= addressed.addressing_config.minimum_displacement);
    REQUIRE(target.displacement <= addressed.addressing_config.maximum_displacement);
  }
  REQUIRE_EQ(addressed.class_storage.size(), frame.class_storage.size());
  for (std::size_t index = 0U; index < frame.class_storage.size(); ++index) {
    REQUIRE_EQ(addressed.class_storage[index].class_id,
               frame.class_storage[index].class_id);
    REQUIRE_EQ(addressed.class_storage[index].storage,
               expected_storage(frame.class_storage[index].storage, addressed));
  }
  REQUIRE_EQ(addressed.location_storage.size(), frame.location_storage.size());
  for (std::size_t index = 0U; index < frame.location_storage.size(); ++index) {
    REQUIRE_EQ(addressed.location_storage[index].location,
               frame.location_storage[index].location);
    REQUIRE_EQ(addressed.location_storage[index].class_id,
               frame.location_storage[index].class_id);
    REQUIRE_EQ(addressed.location_storage[index].storage,
               expected_storage(frame.location_storage[index].storage, addressed));
  }
  REQUIRE_EQ(addressed.blocks.size(), frame.blocks.size());
  for (std::size_t block = 0U; block < frame.blocks.size(); ++block) {
    const auto& source_block = frame.blocks[block];
    const auto& target_block = addressed.blocks[block];
    REQUIRE_EQ(target_block.reachable, source_block.reachable);
    REQUIRE_EQ(target_block.original_block, source_block.original_block);
    REQUIRE_EQ(target_block.operations.size(), source_block.operations.size());
    for (std::size_t op = 0U; op < source_block.operations.size(); ++op) {
      const auto& source = source_block.operations[op];
      const auto& target = target_block.operations[op];
      REQUIRE_EQ(target.kind, source.kind);
      REQUIRE_EQ(target.origin_kind, source.origin_kind);
      REQUIRE_EQ(target.origin_index, source.origin_index);
      REQUIRE_EQ(target.inputs.size(), source.inputs.size());
      for (std::size_t input = 0U; input < source.inputs.size(); ++input) {
        REQUIRE_EQ(target.inputs[input],
                   expected_storage(source.inputs[input], addressed));
      }
      REQUIRE_EQ(target.output.has_value(), source.output.has_value());
      if (source.output.has_value()) {
        REQUIRE_EQ(*target.output, expected_storage(*source.output, addressed));
      }
    }
  }
}

TEST_CASE(backend_frame_addressing_positive_negative_and_zero_frame) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  const auto frame = layout_scratch_aware_backend_frame(
      *plan.selection, BackendFrameLayoutConfig{6U, 4U, 16U});
  REQUIRE(!frame.stack_slots.empty());

  const auto positive = address_scratch_aware_backend_frame(
      frame, {0U, 0, std::numeric_limits<std::int64_t>::max()});
  verify_mapping(frame, positive);
  for (const auto& slot : positive.stack_slots) REQUIRE(slot.displacement >= 0);

  const auto negative = address_scratch_aware_backend_frame(
      frame, {frame.frame_size_bytes,
              std::numeric_limits<std::int64_t>::min(), 0});
  verify_mapping(frame, negative);
  for (const auto& slot : negative.stack_slots) REQUIRE(slot.displacement <= 0);
  REQUIRE_EQ(address_scratch_aware_backend_frame(frame, negative.addressing_config),
             negative);

  const auto empty_plan = plan_scratch_aware_backend_registers(one_block(1U), 1U);
  REQUIRE(empty_plan.selection.has_value());
  const auto empty = layout_scratch_aware_backend_frame(
      *empty_plan.selection, BackendFrameLayoutConfig{8U, 8U, 16U});
  REQUIRE_EQ(empty.frame_size_bytes, 0U);
  verify_mapping(empty, address_scratch_aware_backend_frame(empty, {0U, 0, 0}));
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(empty, {1U, -1, 1}),
                    std::out_of_range);
}

TEST_CASE(backend_frame_addressing_rejects_policy_and_malformed_phase52) {
  OutOfSsaProgram program = one_block(3U);
  for (std::size_t value = 0U; value < 3U; ++value) {
    program.blocks[0].instructions.push_back({{{value, 0U}}, std::nullopt});
  }
  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  const auto frame = layout_scratch_aware_backend_frame(
      *plan.selection, BackendFrameLayoutConfig{6U, 4U, 16U});
  REQUIRE(!frame.stack_slots.empty());

  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(frame, {0U, 1, 0}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(
                        frame, {frame.frame_size_bytes + 1U, -128, 127}),
                    std::out_of_range);
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(
                        frame, {frame.frame_size_bytes, -1, 0}),
                    std::out_of_range);

  auto malformed = frame;
  malformed.stack_slots[0].byte_offset += 1U;
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(malformed, {0U, -128, 127}),
                    std::logic_error);
  malformed = frame;
  malformed.total_registers += 1U;
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(malformed, {0U, -128, 127}),
                    std::logic_error);
}

TEST_CASE(backend_frame_addressing_checks_full_int64_boundary) {
  if (std::numeric_limits<std::size_t>::max() <=
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
    return;
  }
  const std::size_t half =
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / 2) + 1U;
  ScratchAwareByteAddressedBackendFrame frame;
  frame.config = BackendFrameLayoutConfig{half, 1U, 1U};
  frame.stack_slots = {{0U, 0U, half}, {1U, half, half}, {2U, half * 2U, half}};
  frame.frame_size_bytes = half * 3U;
  const std::size_t min_magnitude =
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 1U;

  const auto exact_min = address_scratch_aware_backend_frame(
      frame, {min_magnitude, std::numeric_limits<std::int64_t>::min(),
              std::numeric_limits<std::int64_t>::max()});
  REQUIRE_EQ(exact_min.stack_slots[0].displacement,
             std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(exact_min.stack_slots[2].displacement, 0);
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(
                        frame, {0U, std::numeric_limits<std::int64_t>::min(),
                                std::numeric_limits<std::int64_t>::max()}),
                    std::overflow_error);
  REQUIRE_THROWS_AS(address_scratch_aware_backend_frame(
                        frame, {frame.frame_size_bytes,
                                std::numeric_limits<std::int64_t>::min(),
                                std::numeric_limits<std::int64_t>::max()}),
                    std::overflow_error);
}

TEST_CASE(backend_frame_addressing_randomized_replays_phase52_pipeline) {
  std::mt19937_64 random(0x53BA5EULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};
  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const std::size_t count = 1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);
    for (std::size_t i = 0U; i < static_cast<std::size_t>(random() % 5U); ++i) {
      program.blocks[0].entry_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }
    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 6U);
    for (std::size_t i = 0U; i < instruction_count; ++i) {
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
    for (std::size_t i = 0U; i < static_cast<std::size_t>(random() % 4U); ++i) {
      program.blocks[0].exit_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const auto plan = plan_scratch_aware_backend_registers(
        program, static_cast<std::size_t>(random() % 6U));
    if (!plan.selection.has_value()) continue;
    const std::size_t slot_alignment =
        alignments[static_cast<std::size_t>(random() % 5U)];
    std::size_t frame_alignment = slot_alignment;
    while (frame_alignment < 32U && (random() & 1U) != 0U) frame_alignment *= 2U;
    const auto frame = layout_scratch_aware_backend_frame(
        *plan.selection,
        {1U + static_cast<std::size_t>(random() % 24U), slot_alignment,
         frame_alignment});

    std::size_t anchor = 0U;
    if (frame.frame_size_bytes != 0U) {
      const std::uint64_t choice = random() % 3U;
      anchor = choice == 0U ? 0U
               : choice == 1U ? frame.frame_size_bytes
                               : static_cast<std::size_t>(
                                     random() % (frame.frame_size_bytes + 1U));
    }
    const BackendFrameAddressingConfig config{
        anchor, std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max()};
    const auto addressed = address_scratch_aware_backend_frame(frame, config);
    verify_mapping(frame, addressed);
    REQUIRE_EQ(address_scratch_aware_backend_frame(frame, config), addressed);
  }
}

}  // namespace
