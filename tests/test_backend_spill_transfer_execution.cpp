#include "algorithms/graphs/backend_spill_transfer_execution.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

BaseRelativeBackendStorage physical(const std::size_t reg) {
  BaseRelativeBackendStorage storage;
  storage.kind = BaseRelativeBackendStorageKind::physical_register;
  storage.physical_register = reg;
  return storage;
}

BaseRelativeBackendStorage frame_cell(const std::int64_t displacement) {
  BaseRelativeBackendStorage storage;
  storage.kind = BaseRelativeBackendStorageKind::frame_base_displacement;
  storage.displacement = displacement;
  return storage;
}

BaseRelativeBackendOperation transfer(
    const BackendOperationKind kind, BaseRelativeBackendStorage input,
    BaseRelativeBackendStorage output, const std::size_t origin_index) {
  BaseRelativeBackendOperation operation;
  operation.kind = kind;
  operation.origin_index = origin_index;
  operation.inputs.push_back(std::move(input));
  operation.output = std::move(output);
  return operation;
}

BackendFixedFrameBytecodePlan canonical_transfer_plan(
    std::vector<BaseRelativeBackendOperation> operations,
    const bool reachable = true) {
  constexpr std::size_t frame_size = 16U;
  constexpr std::size_t anchor = 0U;

  StackPointerOwnedBackendFramePlan source;
  source.total_physical_registers = 4U;
  source.stack_pointer_physical_register = 3U;

  auto& directed = source.stack_directed_frame_plan;
  directed.stack_growth_direction =
      BackendStackGrowthDirection::toward_lower_addresses;
  directed.frame_base_plan.total_physical_registers = 3U;
  directed.frame_base_plan.frame_base_physical_register = 2U;

  ScratchAwareBaseRelativeBackendFrame frame;
  frame.frame_layout_config.stack_slot_size_bytes = 8U;
  frame.frame_layout_config.stack_slot_alignment_bytes = 8U;
  frame.frame_layout_config.frame_alignment_bytes = 16U;
  frame.addressing_config.frame_base_byte_anchor = anchor;
  frame.total_registers = 2U;
  frame.allocatable_registers = 2U;
  frame.frame_size_bytes = frame_size;
  frame.stack_slots = {
      BaseRelativeBackendFrameSlot{0U, 0U, 8U, 0},
      BaseRelativeBackendFrameSlot{1U, 8U, 8U, 8},
  };

  BaseRelativeBackendBlock block;
  block.reachable = reachable;
  block.original_block = Vertex{0U};
  block.operations = std::move(operations);
  frame.blocks.push_back(std::move(block));
  directed.frame_base_plan.addressed_frame = std::move(frame);

  StackDirectedBackendFrameEntrySetup setup;
  setup.stack_growth_direction =
      BackendStackGrowthDirection::toward_lower_addresses;
  setup.frame_base_physical_register = 2U;
  setup.frame_size_bytes = frame_size;
  setup.frame_base_byte_anchor = anchor;
  setup.stack_pointer_adjustment = -16;
  setup.frame_base_from_adjusted_stack_pointer = 0;
  setup.frame_base_from_entry_stack_pointer = -16;
  directed.entry_setup = setup;

  const BackendFixedFrameActionPlan phase57 = derive_fixed_frame_actions(source);
  const BackendFixedFrameActionLegalityPolicy policy{
      8U, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  const auto phase58 = legalize_fixed_frame_actions(phase57, policy);
  const auto phase59 = lower_fixed_frame_actions_to_symbolic_instructions(phase58);
  return encode_backend_fixed_frame_bytecode(phase59);
}

TEST_CASE(backend_spill_transfer_executes_register_reload_store_block) {
  const auto plan = canonical_transfer_plan({
      transfer(BackendOperationKind::register_move, physical(0U), physical(1U),
               10U),
      transfer(BackendOperationKind::stack_store, physical(1U), frame_cell(0),
               11U),
      transfer(BackendOperationKind::stack_reload, frame_cell(8), physical(0U),
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
  REQUIRE_EQ(result.steps.size(), std::size_t{3U});
  REQUIRE_EQ(result.steps[0].transferred_value, std::int64_t{11});
  REQUIRE_EQ(result.steps[1].transferred_value, std::int64_t{11});
  REQUIRE_EQ(result.steps[2].transferred_value, std::int64_t{99});
  REQUIRE_EQ(result.steps[2].origin_index, std::size_t{12U});
  REQUIRE_EQ(registers, (std::vector<std::int64_t>{11, 22, 777, 1000}));
  REQUIRE_EQ(frame, (std::vector<std::int64_t>{5, 99}));
}

TEST_CASE(backend_spill_transfer_rejects_opaque_or_malformed_block_before_transfer) {
  BaseRelativeBackendOperation opaque;
  opaque.kind = BackendOperationKind::instruction;
  opaque.inputs = {physical(0U)};
  opaque.output = physical(1U);

  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {5, 99};

  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        canonical_transfer_plan({opaque}), 0U, registers, frame),
                    std::invalid_argument);

  const auto bad_register = canonical_transfer_plan(
      {transfer(BackendOperationKind::register_move, physical(0U),
                physical(4U), 0U)});
  REQUIRE_THROWS_AS(
      execute_backend_spill_transfer_block(bad_register, 0U, registers, frame),
      std::out_of_range);

  const auto bad_slot = canonical_transfer_plan(
      {transfer(BackendOperationKind::stack_reload, frame_cell(9), physical(0U),
                0U)});
  REQUIRE_THROWS_AS(
      execute_backend_spill_transfer_block(bad_slot, 0U, registers, frame),
      std::out_of_range);

  const auto reserved_register = canonical_transfer_plan(
      {transfer(BackendOperationKind::register_move, physical(0U),
                physical(2U), 0U)});
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        reserved_register, 0U, registers, frame),
                    std::logic_error);

  const auto unreachable = canonical_transfer_plan({}, false);
  REQUIRE_THROWS_AS(
      execute_backend_spill_transfer_block(unreachable, 0U, registers, frame),
      std::invalid_argument);

  const std::vector<std::int64_t> short_frame = {5};
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        canonical_transfer_plan({}), 0U, registers, short_frame),
                    std::invalid_argument);

  REQUIRE_EQ(registers, (std::vector<std::int64_t>{11, 22, 777, 1000}));
  REQUIRE_EQ(frame, (std::vector<std::int64_t>{5, 99}));
}

TEST_CASE(backend_spill_transfer_requires_canonical_phase61_plan) {
  auto plan = canonical_transfer_plan(
      {transfer(BackendOperationKind::stack_store, physical(0U), frame_cell(0),
                0U)});
  const std::vector<std::int64_t> registers = {11, 22, 777, 1000};
  const std::vector<std::int64_t> frame = {5, 99};

  REQUIRE(!plan.bytes.empty());
  plan.bytes.front() ^= 0xffU;
  REQUIRE_THROWS_AS(
      execute_backend_spill_transfer_block(plan, 0U, registers, frame),
      std::invalid_argument);

  auto provenance_tamper = canonical_transfer_plan({});
  ++std::get<BackendStackPointerDecrementImmediateInstruction>(
        provenance_tamper.source_plan.entry_instructions.front())
        .immediate_byte_count;
  REQUIRE_THROWS_AS(execute_backend_spill_transfer_block(
                        provenance_tamper, 0U, registers, frame),
                    std::logic_error);
}

TEST_CASE(backend_spill_transfer_matches_independent_cell_replay) {
  std::mt19937_64 rng(0x62A11C0FFEEULL);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000000,
                                                                 1000000);
  std::uniform_int_distribution<unsigned int> kind_distribution(0U, 2U);
  std::uniform_int_distribution<unsigned int> register_distribution(0U, 1U);
  std::uniform_int_distribution<unsigned int> slot_distribution(0U, 1U);

  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    std::vector<BaseRelativeBackendOperation> operations;
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
        const std::int64_t displacement =
            slot_distribution(rng) == 0U ? 0 : 8;
        operations.push_back(transfer(
            BackendOperationKind::stack_reload, frame_cell(displacement),
            physical(static_cast<std::size_t>(register_distribution(rng))),
            index));
      } else {
        const std::int64_t displacement =
            slot_distribution(rng) == 0U ? 0 : 8;
        operations.push_back(transfer(
            BackendOperationKind::stack_store,
            physical(static_cast<std::size_t>(register_distribution(rng))),
            frame_cell(displacement), index));
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

    for (const auto& operation : operations) {
      std::int64_t value = 0;
      if (operation.kind == BackendOperationKind::stack_reload) {
        const auto displacement = operation.inputs.front().displacement;
        const std::size_t slot = displacement == 0 ? 0U : 1U;
        value = expected_frame[slot];
        expected_registers[operation.output->physical_register] = value;
      } else if (operation.kind == BackendOperationKind::stack_store) {
        value = expected_registers[operation.inputs.front().physical_register];
        const std::size_t slot = operation.output->displacement == 0 ? 0U : 1U;
        expected_frame[slot] = value;
      } else {
        value = expected_registers[operation.inputs.front().physical_register];
        expected_registers[operation.output->physical_register] = value;
      }
      expected_values.push_back(value);
    }

    const auto actual = execute_backend_spill_transfer_block(
        plan, 0U, initial_registers, initial_frame);
    REQUIRE_EQ(actual.after_block_registers, expected_registers);
    REQUIRE_EQ(actual.frame_slot_values, expected_frame);
    REQUIRE_EQ(actual.steps.size(), expected_values.size());
    for (std::size_t index = 0U; index < expected_values.size(); ++index) {
      REQUIRE_EQ(actual.steps[index].operation_index, index);
      REQUIRE_EQ(actual.steps[index].transferred_value, expected_values[index]);
    }
    REQUIRE_EQ(initial_registers.size(), std::size_t{4U});
    REQUIRE_EQ(initial_frame.size(), std::size_t{2U});
  }
}

}  // namespace
