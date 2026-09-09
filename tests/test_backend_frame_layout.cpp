#include "algorithms/graphs/backend_frame_layout.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

SsaCopyLocation loc(const std::size_t variable) {
  return SsaCopyLocation::from_value({variable, 0U});
}

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

ByteAddressedBackendStorage expected_storage(
    const BackendStorage source,
    const ScratchAwareByteAddressedBackendFrame& frame) {
  if (source.kind == BackendStorageKind::physical_register) {
    return {ByteAddressedBackendStorageKind::physical_register, source.index};
  }
  REQUIRE_EQ(source.kind, BackendStorageKind::stack_slot);
  REQUIRE(source.index < frame.stack_slots.size());
  return {ByteAddressedBackendStorageKind::frame_byte_offset,
          frame.stack_slots[source.index].byte_offset};
}

void verify_layout(const ScratchAwareBackendRegisterSelection& selection,
                   const ScratchAwareByteAddressedBackendFrame& frame) {
  REQUIRE_EQ(frame.total_registers,
             selection.allocatable_registers +
                 selection.reserved_scratch_registers);
  REQUIRE_EQ(frame.reserved_scratch_registers,
             selection.reserved_scratch_registers);
  REQUIRE_EQ(frame.allocatable_registers, selection.allocatable_registers);
  REQUIRE_EQ(frame.scratch_physical_registers,
             selection.scratch_physical_registers);
  REQUIRE_EQ(frame.stack_slots.size(),
             selection.abstract_lowering.stack_slot_count);
  REQUIRE_EQ(frame.class_storage.size(),
             selection.abstract_lowering.class_storage.size());
  REQUIRE_EQ(frame.location_storage.size(),
             selection.abstract_lowering.location_storage.size());
  REQUIRE_EQ(frame.blocks.size(), selection.physicalized_blocks.size());

  std::size_t previous_end = 0U;
  for (std::size_t slot = 0U; slot < frame.stack_slots.size(); ++slot) {
    const BackendFrameSlotLayout& item = frame.stack_slots[slot];
    REQUIRE_EQ(item.stack_slot, slot);
    REQUIRE_EQ(item.byte_size, frame.config.stack_slot_size_bytes);
    REQUIRE_EQ(item.byte_offset % frame.config.stack_slot_alignment_bytes, 0U);
    REQUIRE(item.byte_offset >= previous_end);
    REQUIRE(item.byte_offset <=
            std::numeric_limits<std::size_t>::max() - item.byte_size);
    previous_end = item.byte_offset + item.byte_size;
  }
  REQUIRE(frame.frame_size_bytes >= previous_end);
  REQUIRE_EQ(frame.frame_size_bytes % frame.config.frame_alignment_bytes, 0U);

  for (std::size_t index = 0U; index < frame.class_storage.size(); ++index) {
    const BackendClassStorage& source =
        selection.abstract_lowering.class_storage[index];
    const ByteAddressedBackendClassStorage& target = frame.class_storage[index];
    REQUIRE_EQ(target.class_id, source.class_id);
    REQUIRE_EQ(target.storage, expected_storage(source.storage, frame));
  }
  for (std::size_t index = 0U; index < frame.location_storage.size(); ++index) {
    const BackendLocationStorage& source =
        selection.abstract_lowering.location_storage[index];
    const ByteAddressedBackendLocationStorage& target =
        frame.location_storage[index];
    REQUIRE_EQ(target.location, source.location);
    REQUIRE_EQ(target.class_id, source.class_id);
    REQUIRE_EQ(target.storage, expected_storage(source.storage, frame));
  }

  for (std::size_t block = 0U; block < frame.blocks.size(); ++block) {
    const BackendLoweredBlock& source = selection.physicalized_blocks[block];
    const ByteAddressedBackendBlock& target = frame.blocks[block];
    REQUIRE_EQ(target.reachable, source.reachable);
    REQUIRE_EQ(target.original_block, source.original_block);
    REQUIRE_EQ(target.operations.size(), source.operations.size());
    for (std::size_t operation = 0U; operation < source.operations.size();
         ++operation) {
      const BackendOperation& source_operation = source.operations[operation];
      const ByteAddressedBackendOperation& target_operation =
          target.operations[operation];
      REQUIRE_EQ(target_operation.kind, source_operation.kind);
      REQUIRE_EQ(target_operation.origin_kind, source_operation.origin_kind);
      REQUIRE_EQ(target_operation.origin_index, source_operation.origin_index);
      REQUIRE_EQ(target_operation.inputs.size(), source_operation.inputs.size());
      for (std::size_t input = 0U; input < source_operation.inputs.size();
           ++input) {
        REQUIRE_EQ(target_operation.inputs[input],
                   expected_storage(source_operation.inputs[input], frame));
      }
      REQUIRE_EQ(target_operation.output.has_value(),
                 source_operation.output.has_value());
      if (source_operation.output.has_value()) {
        REQUIRE_EQ(*target_operation.output,
                   expected_storage(*source_operation.output, frame));
      }
    }
  }
}

TEST_CASE(backend_frame_layout_assigns_deterministic_aligned_offsets) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});

  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  REQUIRE(plan.selection->abstract_lowering.stack_slot_count > 0U);

  const BackendFrameLayoutConfig config{6U, 4U, 16U};
  const auto frame =
      layout_scratch_aware_backend_frame(*plan.selection, config);
  REQUIRE_EQ(frame.config, config);
  verify_layout(*plan.selection, frame);
  REQUIRE_EQ(layout_scratch_aware_backend_frame(*plan.selection, config), frame);

  for (std::size_t slot = 0U; slot < frame.stack_slots.size(); ++slot) {
    REQUIRE_EQ(frame.stack_slots[slot].byte_offset, slot * 8U);
  }
}

TEST_CASE(backend_frame_layout_zero_slots_uses_zero_frame_bytes) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});
  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  REQUIRE_EQ(plan.selection->abstract_lowering.stack_slot_count, 0U);

  const auto frame = layout_scratch_aware_backend_frame(
      *plan.selection, BackendFrameLayoutConfig{8U, 8U, 32U});
  REQUIRE(frame.stack_slots.empty());
  REQUIRE_EQ(frame.frame_size_bytes, 0U);
  verify_layout(*plan.selection, frame);
}

TEST_CASE(backend_frame_layout_validates_alignment_and_checked_size_arithmetic) {
  OutOfSsaProgram program = one_block(1U);
  const auto plan = plan_scratch_aware_backend_registers(program, 1U);
  REQUIRE(plan.selection.has_value());

  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        *plan.selection,
                        BackendFrameLayoutConfig{0U, 1U, 1U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        *plan.selection,
                        BackendFrameLayoutConfig{8U, 0U, 8U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        *plan.selection,
                        BackendFrameLayoutConfig{8U, 3U, 8U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        *plan.selection,
                        BackendFrameLayoutConfig{8U, 8U, 4U}),
                    std::invalid_argument);

  ScratchAwareBackendRegisterSelection overflow = *plan.selection;
  overflow.abstract_lowering.stack_slot_count = 2U;
  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        overflow,
                        BackendFrameLayoutConfig{
                            std::numeric_limits<std::size_t>::max(), 1U, 1U}),
                    std::overflow_error);
}

TEST_CASE(backend_frame_layout_rejects_inconsistent_phase51_witness) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  const auto plan = plan_scratch_aware_backend_registers(program, 2U);
  REQUIRE(plan.selection.has_value());
  REQUIRE(!plan.selection->scratch_physical_registers.empty());

  ScratchAwareBackendRegisterSelection malformed = *plan.selection;
  malformed.scratch_physical_registers[0] = 0U;
  REQUIRE_THROWS_AS(layout_scratch_aware_backend_frame(
                        malformed, BackendFrameLayoutConfig{}),
                    std::logic_error);
}

TEST_CASE(backend_frame_layout_randomized_replays_phase51_storage_and_provenance) {
  std::mt19937_64 random(0x52F4A93ULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};

  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const std::size_t count =
        1U + static_cast<std::size_t>(random() % 7U);
    OutOfSsaProgram program = one_block(count);

    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(random() % 5U); ++index) {
      program.blocks[0].entry_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const std::size_t instruction_count =
        1U + static_cast<std::size_t>(random() % 6U);
    for (std::size_t instruction_index = 0U;
         instruction_index < instruction_count; ++instruction_index) {
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

    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(random() % 4U); ++index) {
      program.blocks[0].exit_moves.push_back(
          {loc(static_cast<std::size_t>(random() % count)),
           loc(static_cast<std::size_t>(random() % count))});
    }

    const auto plan = plan_scratch_aware_backend_registers(
        program, static_cast<std::size_t>(random() % 6U));
    if (!plan.selection.has_value()) {
      continue;
    }

    const std::size_t slot_alignment =
        alignments[static_cast<std::size_t>(random() % 5U)];
    std::size_t frame_alignment = slot_alignment;
    while (frame_alignment < 32U && (random() & 1U) != 0U) {
      frame_alignment *= 2U;
    }
    const BackendFrameLayoutConfig config{
        1U + static_cast<std::size_t>(random() % 24U), slot_alignment,
        frame_alignment};

    const auto frame =
        layout_scratch_aware_backend_frame(*plan.selection, config);
    verify_layout(*plan.selection, frame);
    REQUIRE_EQ(layout_scratch_aware_backend_frame(*plan.selection, config),
               frame);
  }
}

}  // namespace
