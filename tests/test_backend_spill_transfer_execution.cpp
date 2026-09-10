#include "algorithms/graphs/backend_spill_transfer_execution.hpp"
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

BackendFixedFrameBytecodePlan canonical_transfer_plan(
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

std::size_t slot_for_displacement(
    const ScratchAwareBaseRelativeBackendFrame& frame,
    const std::int64_t displacement) {
  for (const auto& slot : frame.stack_slots) {
    if (slot.displacement == displacement) {
      return slot.stack_slot;
    }
  }
  throw std::runtime_error("test oracle could not resolve frame displacement");
}

TEST_CASE(backend_spill_transfer_executes_transfer_only_block) {
  const auto plan = canonical_transfer_plan({
      transfer(BackendOperationKind::register_move, physical(0U), physical(1U),
               10U),
      transfer(BackendOperationKind::stack_store, physical(1U), stack_cell(0U),
               11U),
      transfer(BackendOperationKind::stack_reload, stack_cell(1U), physical(0U),
               12U),
  });
  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {5, 99};

  const auto result =
      execute_backend_spill_transfer_block(plan, 0U, registers, frame);

  REQUIRE_EQ(result.after_entry_registers,
             (std::vector<std::int64_t>{11, 22, 984, 984}));
  REQUIRE_EQ(result.after_block_registers,
             (std::vector<std::int64_t>{99, 11, 984, 984}));
  REQUIRE_EQ(result.frame_slot_values,
             (std::vector<std::int64_t>{11, 99}));
  REQUIRE(!result.suspended_at_instruction.has_value());
  REQUIRE_EQ(result.steps.size(), std::size_t{3U});
  REQUIRE(result.steps[0].transferred_value.has_value());
  REQUIRE_EQ(*result.steps[0].transferred_value, std::int64_t{11});
  REQUIRE_EQ(*result.steps[1].transferred_value, std::int64_t{11});
  REQUIRE_EQ(*result.steps[2].transferred_value, std::int64_t{99});
  REQUIRE_EQ(result.steps[2].origin_index, std::size_t{12U});
  REQUIRE_EQ(registers, (std::vector<std::int64_t>{11, 22, 777, 1000}));
  REQUIRE_EQ(frame, (std::vector<std::int64_t>{5, 99}));
}

TEST_CASE(backend_spill_transfer_suspends_at_opaque_instruction_barrier) {
  const auto plan = canonical_transfer_plan({
      transfer(BackendOperationKind::stack_reload, stack_cell(0U), physical(0U),
               20U),
      operation(BackendOperationKind::instruction, {physical(0U)}, physical(1U),
                21U),
      transfer(BackendOperationKind::stack_store, physical(1U), stack_cell(1U),
               22U),
  });
  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {41, 99};

  const auto result =
      execute_backend_spill_transfer_block(plan, 0U, registers, frame);

  REQUIRE_EQ(result.suspended_at_instruction, std::optional<std::size_t>{1U});
  REQUIRE_EQ(result.after_block_registers,
             (std::vector<std::int64_t>{41, 22, 984, 984}));
  REQUIRE_EQ(result.frame_slot_values, frame);
  REQUIRE_EQ(result.steps.size(), std::size_t{2U});
  REQUIRE_EQ(result.steps[0].kind, BackendOperationKind::stack_reload);
  REQUIRE(result.steps[0].transferred_value.has_value());
  REQUIRE_EQ(*result.steps[0].transferred_value, std::int64_t{41});
  REQUIRE_EQ(result.steps[1].kind, BackendOperationKind::instruction);
  REQUIRE(!result.steps[1].transferred_value.has_value());
  REQUIRE_EQ(result.steps[1].origin_index, std::size_t{21U});
}

TEST_CASE(backend_spill_transfer_validates_canonical_body_provenance) {
  const auto clean = canonical_transfer_plan(
      {transfer(BackendOperationKind::stack_store, physical(0U), stack_cell(0U),
                0U)});
  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {5, 99};

  auto byte_tamper = clean;
  byte_tamper.bytes.front() ^= 0xffU;
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        byte_tamper, 0U, registers, frame),
                    std::invalid_argument);

  auto body_tamper = clean;
  auto& phase56 =
      body_tamper.source_plan.source_plan.source_plan.source_plan;
  auto& addressed =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  ++addressed.blocks[0].operations[0].origin_index;

  static_cast<void>(
      execute_backend_fixed_frame_bytecode(body_tamper, registers));
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        body_tamper, 0U, registers, frame),
                    std::logic_error);

  const std::vector<std::int64_t> short_registers = {11, 22, 777};
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        clean, 0U, short_registers, frame),
                    std::out_of_range);
  const std::vector<std::int64_t> short_frame = {5};
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        clean, 0U, registers, short_frame),
                    std::invalid_argument);
}

TEST_CASE(backend_spill_transfer_integrates_real_spilled_instruction_pipeline) {
  const auto plan = upstream_spilled_instruction_plan();
  const auto& phase56 = plan.source_plan.source_plan.source_plan.source_plan;
  const auto& addressed =
      *phase56.stack_directed_frame_plan.frame_base_plan.addressed_frame;
  REQUIRE_EQ(addressed.blocks.size(), std::size_t{1U});
  const auto& operations = addressed.blocks[0].operations;

  const auto barrier = std::find_if(
      operations.begin(), operations.end(), [](const auto& item) {
        return item.kind == BackendOperationKind::instruction;
      });
  REQUIRE(barrier != operations.end());
  const std::size_t barrier_index =
      static_cast<std::size_t>(barrier - operations.begin());
  REQUIRE(barrier_index > 0U);
  REQUIRE(std::any_of(operations.begin(), barrier, [](const auto& item) {
    return item.kind == BackendOperationKind::stack_reload;
  }));

  std::vector<std::int64_t> initial_registers(
      phase56.total_physical_registers, 0);
  for (std::size_t index = 0U; index < initial_registers.size(); ++index) {
    initial_registers[index] = static_cast<std::int64_t>(100U + index);
  }
  std::vector<std::int64_t> initial_frame(addressed.stack_slots.size(), 0);
  for (std::size_t index = 0U; index < initial_frame.size(); ++index) {
    initial_frame[index] = static_cast<std::int64_t>(700U + index);
  }

  std::vector<std::int64_t> expected_registers =
      execute_backend_fixed_frame_bytecode(plan, initial_registers)
          .after_entry_registers;
  std::vector<std::int64_t> expected_frame = initial_frame;
  for (std::size_t index = 0U; index < barrier_index; ++index) {
    const auto& item = operations[index];
    REQUIRE(item.inputs.size() == 1U);
    REQUIRE(item.output.has_value());
    if (item.kind == BackendOperationKind::stack_reload) {
      const std::size_t slot =
          slot_for_displacement(addressed, item.inputs.front().displacement);
      expected_registers[item.output->physical_register] = expected_frame[slot];
    } else if (item.kind == BackendOperationKind::stack_store) {
      const std::size_t slot =
          slot_for_displacement(addressed, item.output->displacement);
      expected_frame[slot] =
          expected_registers[item.inputs.front().physical_register];
    } else {
      REQUIRE_EQ(item.kind, BackendOperationKind::register_move);
      expected_registers[item.output->physical_register] =
          expected_registers[item.inputs.front().physical_register];
    }
  }

  const auto result = execute_backend_spill_transfer_block(
      plan, 0U, initial_registers, initial_frame);
  REQUIRE_EQ(result.suspended_at_instruction,
             std::optional<std::size_t>{barrier_index});
  REQUIRE_EQ(result.after_block_registers, expected_registers);
  REQUIRE_EQ(result.frame_slot_values, expected_frame);
  REQUIRE_EQ(result.steps.size(), barrier_index + 1U);
  REQUIRE_EQ(result.steps.back().kind, BackendOperationKind::instruction);
  REQUIRE(!result.steps.back().transferred_value.has_value());
}

TEST_CASE(backend_spill_transfer_matches_independent_cell_replay) {
  std::mt19937_64 rng(0x62A11C0FFEEULL);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000000,
                                                                 1000000);
  std::uniform_int_distribution<unsigned int> kind_distribution(0U, 2U);
  std::uniform_int_distribution<unsigned int> register_distribution(0U, 1U);
  std::uniform_int_distribution<unsigned int> slot_distribution(0U, 1U);

  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    std::vector<BackendOperation> operations;
    operations.reserve(80U);
    for (std::size_t index = 0U; index < 80U; ++index) {
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
      } else {
        operations.push_back(transfer(
            BackendOperationKind::stack_store,
            physical(static_cast<std::size_t>(register_distribution(rng))),
            stack_cell(static_cast<std::size_t>(slot_distribution(rng))),
            index));
      }
    }

    const auto plan = canonical_transfer_plan(operations);
    const std::vector<std::int64_t> initial_registers = {
        value_distribution(rng), value_distribution(rng),
        value_distribution(rng), value_distribution(rng)};
    const std::vector<std::int64_t> initial_frame = {
        value_distribution(rng), value_distribution(rng)};

    std::vector<std::int64_t> expected_registers = initial_registers;
    expected_registers[3] -= 16;
    expected_registers[2] = expected_registers[3];
    std::vector<std::int64_t> expected_frame = initial_frame;
    std::vector<std::int64_t> expected_values;
    expected_values.reserve(operations.size());

    for (const auto& item : operations) {
      std::int64_t value = 0;
      if (item.kind == BackendOperationKind::stack_reload) {
        const std::size_t slot = item.inputs.front().index;
        value = expected_frame[slot];
        expected_registers[item.output->index] = value;
      } else if (item.kind == BackendOperationKind::stack_store) {
        value = expected_registers[item.inputs.front().index];
        expected_frame[item.output->index] = value;
      } else {
        value = expected_registers[item.inputs.front().index];
        expected_registers[item.output->index] = value;
      }
      expected_values.push_back(value);
    }

    const auto actual = execute_backend_spill_transfer_block(
        plan, 0U, initial_registers, initial_frame);
    REQUIRE(!actual.suspended_at_instruction.has_value());
    REQUIRE_EQ(actual.after_block_registers, expected_registers);
    REQUIRE_EQ(actual.frame_slot_values, expected_frame);
    REQUIRE_EQ(actual.steps.size(), expected_values.size());
    for (std::size_t index = 0U; index < expected_values.size(); ++index) {
      REQUIRE_EQ(actual.steps[index].operation_index, index);
      REQUIRE(actual.steps[index].transferred_value.has_value());
      REQUIRE_EQ(*actual.steps[index].transferred_value,
                 expected_values[index]);
    }
  }
}

}  // namespace
