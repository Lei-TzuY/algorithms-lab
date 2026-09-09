#include "algorithms/graphs/backend_frame_base_reservation.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
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

void require_storage_below_base(const BaseRelativeBackendStorage& storage,
                                const std::size_t base) {
  if (storage.kind == BaseRelativeBackendStorageKind::physical_register) {
    REQUIRE(storage.physical_register < base);
  }
}

void verify_disjoint(const FrameBaseReservedBackendPlan& plan) {
  REQUIRE(plan.frame_base_physical_register.has_value());
  REQUIRE(plan.addressed_frame.has_value());
  const std::size_t base = *plan.frame_base_physical_register;
  const auto& frame = *plan.addressed_frame;
  REQUIRE_EQ(base + 1U, plan.total_physical_registers);
  REQUIRE_EQ(frame.total_registers, base);
  for (const std::size_t scratch : frame.scratch_physical_registers) {
    REQUIRE(scratch < base);
  }
  for (const auto& item : frame.class_storage) {
    require_storage_below_base(item.storage, base);
  }
  for (const auto& item : frame.location_storage) {
    require_storage_below_base(item.storage, base);
  }
  for (const auto& block : frame.blocks) {
    for (const auto& operation : block.operations) {
      for (const auto& input : operation.inputs) {
        require_storage_below_base(input, base);
      }
      if (operation.output.has_value()) {
        require_storage_below_base(*operation.output, base);
      }
    }
  }
}

void verify_against_manual_pipeline(
    const OutOfSsaProgram& program, const std::size_t total_registers,
    const BackendFrameLayoutConfig layout,
    const BackendFrameAddressingConfig addressing,
    const FrameBaseReservedBackendPlan& actual) {
  REQUIRE_EQ(actual.total_physical_registers, total_registers);
  if (total_registers == 0U) {
    REQUIRE(!actual.frame_base_physical_register.has_value());
    REQUIRE(!actual.byte_addressed_frame.has_value());
    REQUIRE(!actual.addressed_frame.has_value());
    REQUIRE_EQ(actual.non_base_register_plan,
               plan_scratch_aware_backend_registers(program, 0U));
    return;
  }

  const std::size_t base = total_registers - 1U;
  REQUIRE_EQ(actual.frame_base_physical_register,
             std::optional<std::size_t>{base});
  const auto manual_plan = plan_scratch_aware_backend_registers(program, base);
  REQUIRE_EQ(actual.non_base_register_plan, manual_plan);
  if (!manual_plan.selection.has_value()) {
    REQUIRE(!actual.byte_addressed_frame.has_value());
    REQUIRE(!actual.addressed_frame.has_value());
    return;
  }

  const auto manual_frame =
      layout_scratch_aware_backend_frame(*manual_plan.selection, layout);
  const auto manual_addressed =
      address_scratch_aware_backend_frame(manual_frame, addressing);
  REQUIRE_EQ(actual.byte_addressed_frame,
             std::optional<ScratchAwareByteAddressedBackendFrame>{manual_frame});
  REQUIRE_EQ(actual.addressed_frame,
             std::optional<ScratchAwareBaseRelativeBackendFrame>{manual_addressed});
  verify_disjoint(actual);
}

TEST_CASE(backend_frame_base_reservation_handles_zero_and_single_register) {
  const OutOfSsaProgram empty = one_block(0U);
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  const auto zero = plan_frame_base_reserved_backend(empty, 0U, layout, addressing);
  verify_against_manual_pipeline(empty, 0U, layout, addressing, zero);

  const auto one = plan_frame_base_reserved_backend(empty, 1U, layout, addressing);
  verify_against_manual_pipeline(empty, 1U, layout, addressing, one);
  REQUIRE(one.addressed_frame.has_value());
  REQUIRE_EQ(*one.frame_base_physical_register, 0U);
  REQUIRE_EQ(one.addressed_frame->total_registers, 0U);
}

TEST_CASE(backend_frame_base_reservation_preserves_scratch_and_spill_pipeline) {
  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});

  const BackendFrameLayoutConfig layout{6U, 4U, 16U};
  const auto plan = plan_frame_base_reserved_backend(
      program, 3U, layout,
      {16U, std::numeric_limits<std::int64_t>::min(),
       std::numeric_limits<std::int64_t>::max()});
  REQUIRE(plan.non_base_register_plan.selection.has_value());
  REQUIRE_EQ(plan.non_base_register_plan.selection->reserved_scratch_registers,
             1U);
  REQUIRE_EQ(plan.non_base_register_plan.selection->scratch_physical_registers,
             std::vector<std::size_t>{1U});
  REQUIRE_EQ(*plan.frame_base_physical_register, 2U);
  verify_disjoint(plan);
  REQUIRE(!plan.addressed_frame->stack_slots.empty());
}

TEST_CASE(backend_frame_base_reservation_reports_non_base_infeasibility) {
  OutOfSsaProgram program = one_block(2U);
  program.blocks[0].instructions.push_back(
      {{{0U, 0U}, {1U, 0U}}, std::nullopt});
  const BackendFrameLayoutConfig layout{8U, 8U, 16U};
  const BackendFrameAddressingConfig addressing{0U, -64, 64};

  const auto plan = plan_frame_base_reserved_backend(program, 2U, layout, addressing);
  REQUIRE_EQ(plan.frame_base_physical_register,
             std::optional<std::size_t>{1U});
  REQUIRE(!plan.non_base_register_plan.selection.has_value());
  REQUIRE(!plan.byte_addressed_frame.has_value());
  REQUIRE(!plan.addressed_frame.has_value());
  verify_against_manual_pipeline(program, 2U, layout, addressing, plan);
}

TEST_CASE(backend_frame_base_reservation_propagates_frame_configuration_errors) {
  const OutOfSsaProgram empty = one_block(0U);
  REQUIRE_THROWS_AS(plan_frame_base_reserved_backend(
                        empty, 1U, BackendFrameLayoutConfig{8U, 0U, 16U},
                        BackendFrameAddressingConfig{0U, -64, 64}),
                    std::invalid_argument);

  OutOfSsaProgram program = one_block(3U);
  program.blocks[0].instructions.push_back({{{0U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{1U, 0U}}, std::nullopt});
  program.blocks[0].instructions.push_back({{{2U, 0U}}, std::nullopt});
  REQUIRE_THROWS_AS(plan_frame_base_reserved_backend(
                        program, 3U, BackendFrameLayoutConfig{8U, 8U, 16U},
                        BackendFrameAddressingConfig{0U, 1, 0}),
                    std::invalid_argument);
}

TEST_CASE(backend_frame_base_reservation_randomized_matches_manual_pipeline) {
  std::mt19937_64 random(0x54BA5EULL);
  constexpr std::size_t alignments[] = {1U, 2U, 4U, 8U, 16U};
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
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
        static_cast<std::size_t>(random() % 7U);
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

    const auto actual = plan_frame_base_reserved_backend(
        program, total_registers, layout, addressing);
    verify_against_manual_pipeline(program, total_registers, layout, addressing,
                                   actual);
    REQUIRE_EQ(plan_frame_base_reserved_backend(program, total_registers, layout,
                                                addressing),
               actual);
  }
}

}  // namespace
