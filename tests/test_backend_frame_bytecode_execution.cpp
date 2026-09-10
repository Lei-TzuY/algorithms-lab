#include "algorithms/graphs/backend_frame_bytecode_execution.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace algorithms::graphs;

std::int64_t test_size_to_i64(const std::size_t value) {
  if (value > static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("test magnitude is not int64 representable");
  }
  return static_cast<std::int64_t>(value);
}

std::int64_t test_checked_add(const std::int64_t left,
                              const std::int64_t right) {
  if (right > 0 &&
      left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error("test addition overflow");
  }
  if (right < 0 &&
      left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error("test addition underflow");
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
        setup.frame_base_from_adjusted_stack_pointer = test_size_to_i64(anchor);
        setup.frame_base_from_entry_stack_pointer =
            anchor == 0U ? std::numeric_limits<std::int64_t>::min()
                         : static_cast<std::int64_t>(
                               std::numeric_limits<std::int64_t>::min() +
                               test_size_to_i64(anchor));
        directed.entry_setup = setup;
        return derive_fixed_frame_actions(source);
      }
    }

    const std::int64_t signed_frame_size = test_size_to_i64(frame_size);
    const std::int64_t signed_anchor = test_size_to_i64(anchor);
    setup.stack_pointer_adjustment = -signed_frame_size;
    setup.frame_base_from_adjusted_stack_pointer = signed_anchor;
    setup.frame_base_from_entry_stack_pointer =
        signed_anchor - signed_frame_size;
  } else {
    const std::int64_t signed_frame_size = test_size_to_i64(frame_size);
    const std::int64_t signed_anchor = test_size_to_i64(anchor);
    setup.stack_pointer_adjustment = signed_frame_size;
    setup.frame_base_from_adjusted_stack_pointer =
        signed_anchor - signed_frame_size;
    setup.frame_base_from_entry_stack_pointer = signed_anchor;
  }

  directed.entry_setup = setup;
  return derive_fixed_frame_actions(source);
}

BackendSymbolicFixedFrameInstructionPlan canonical_phase59(
    const std::size_t frame_size, const std::size_t anchor,
    const BackendStackGrowthDirection direction,
    const std::size_t max_stack_immediate) {
  const auto phase57 = manual_phase57_plan(frame_size, anchor, direction);
  const BackendFixedFrameActionLegalityPolicy policy{
      max_stack_immediate, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  return lower_fixed_frame_actions_to_symbolic_instructions(
      legalize_fixed_frame_actions(phase57, policy));
}

struct RawCursor {
  std::span<const std::uint8_t> bytes;
  std::size_t offset{0U};

  std::uint8_t byte() {
    REQUIRE(offset < bytes.size());
    return bytes[offset++];
  }

  std::uint64_t u64() {
    REQUIRE(offset <= bytes.size());
    REQUIRE(bytes.size() - offset >= 8U);
    std::uint64_t value = 0U;
    for (unsigned int shift = 0U; shift < 64U; shift += 8U) {
      value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    }
    return value;
  }
};

std::size_t raw_size(const std::uint64_t value) {
  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
    REQUIRE(value <= static_cast<std::uint64_t>(
                         std::numeric_limits<std::size_t>::max()));
  }
  return static_cast<std::size_t>(value);
}

std::int64_t raw_i64(const std::uint64_t bits) {
  const auto max_i64 =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (bits <= max_i64) {
    return static_cast<std::int64_t>(bits);
  }
  return -std::int64_t{1} - static_cast<std::int64_t>(
                                 std::numeric_limits<std::uint64_t>::max() -
                                 bits);
}

void raw_execute_instruction(RawCursor& cursor,
                             std::vector<std::int64_t>& registers) {
  const std::uint8_t opcode = cursor.byte();
  if (opcode == 0x01U || opcode == 0x02U) {
    const std::size_t reg = raw_size(cursor.u64());
    const std::size_t magnitude = raw_size(cursor.u64());
    REQUIRE(reg < registers.size());
    const std::int64_t signed_magnitude = test_size_to_i64(magnitude);
    registers[reg] = test_checked_add(
        registers[reg], opcode == 0x01U ? -signed_magnitude : signed_magnitude);
    return;
  }

  REQUIRE_EQ(opcode, std::uint8_t{0x03U});
  const std::size_t frame_base = raw_size(cursor.u64());
  const std::size_t stack_pointer = raw_size(cursor.u64());
  const std::int64_t displacement = raw_i64(cursor.u64());
  REQUIRE(frame_base < registers.size());
  REQUIRE(stack_pointer < registers.size());
  registers[frame_base] =
      test_checked_add(registers[stack_pointer], displacement);
}

BackendFixedFrameBytecodeExecutionSnapshots independent_raw_execution(
    const std::vector<std::uint8_t>& bytes,
    const std::span<const std::int64_t> initial_registers) {
  RawCursor cursor{bytes};
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x41U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x4cU});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x46U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x42U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x01U});

  std::vector<std::int64_t> registers(initial_registers.begin(),
                                      initial_registers.end());
  const std::size_t entry_count = raw_size(cursor.u64());
  for (std::size_t index = 0U; index < entry_count; ++index) {
    raw_execute_instruction(cursor, registers);
  }

  BackendFixedFrameBytecodeExecutionSnapshots result;
  result.after_entry_registers = registers;

  const std::size_t exit_count = raw_size(cursor.u64());
  for (std::size_t index = 0U; index < exit_count; ++index) {
    raw_execute_instruction(cursor, registers);
  }
  REQUIRE_EQ(cursor.offset, bytes.size());
  result.after_exit_registers = std::move(registers);
  return result;
}

TEST_CASE(backend_frame_bytecode_execution_replays_entry_and_exit) {
  const auto lower = encode_backend_fixed_frame_bytecode(canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses, 4U));
  const std::vector<std::int64_t> initial = {77, 1000};
  const auto lower_result = execute_backend_fixed_frame_bytecode(lower, initial);
  REQUIRE_EQ(lower_result.after_entry_registers,
             (std::vector<std::int64_t>{993, 990}));
  REQUIRE_EQ(lower_result.after_exit_registers,
             (std::vector<std::int64_t>{993, 1000}));
  REQUIRE_EQ(initial, (std::vector<std::int64_t>{77, 1000}));

  const auto upper = encode_backend_fixed_frame_bytecode(canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_higher_addresses, 4U));
  const auto upper_result = execute_backend_fixed_frame_bytecode(upper, initial);
  REQUIRE_EQ(upper_result.after_entry_registers,
             (std::vector<std::int64_t>{1003, 1010}));
  REQUIRE_EQ(upper_result.after_exit_registers,
             (std::vector<std::int64_t>{1003, 1000}));
  REQUIRE_EQ(initial, (std::vector<std::int64_t>{77, 1000}));
}

TEST_CASE(backend_frame_bytecode_execution_rejects_before_touching_caller_state) {
  const auto canonical = encode_backend_fixed_frame_bytecode(canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses, 4U));
  const std::vector<std::int64_t> initial = {17, 1000};

  auto malformed = canonical;
  malformed.bytes[0] ^= 0xffU;
  REQUIRE_THROWS_AS(execute_backend_fixed_frame_bytecode(malformed, initial),
                    std::invalid_argument);
  REQUIRE_EQ(initial, (std::vector<std::int64_t>{17, 1000}));

  auto well_formed_tamper = canonical;
  REQUIRE(well_formed_tamper.bytes.size() > 22U);
  well_formed_tamper.bytes[22] ^= 0x01U;
  REQUIRE_THROWS_AS(
      execute_backend_fixed_frame_bytecode(well_formed_tamper, initial),
      std::logic_error);
  REQUIRE_EQ(initial, (std::vector<std::int64_t>{17, 1000}));

  auto source_tamper = canonical;
  ++std::get<BackendStackPointerDecrementImmediateInstruction>(
        source_tamper.source_plan.entry_instructions.front())
        .immediate_byte_count;
  REQUIRE_THROWS_AS(execute_backend_fixed_frame_bytecode(source_tamper, initial),
                    std::logic_error);
  REQUIRE_EQ(initial, (std::vector<std::int64_t>{17, 1000}));

  const std::vector<std::int64_t> too_small = {17};
  REQUIRE_THROWS_AS(execute_backend_fixed_frame_bytecode(canonical, too_small),
                    std::out_of_range);
  REQUIRE_EQ(too_small, (std::vector<std::int64_t>{17}));
}

TEST_CASE(backend_frame_bytecode_execution_checks_full_width_stack_arithmetic) {
  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t huge =
        static_cast<std::size_t>(std::uint64_t{1U} << 63U);
    const auto plan = encode_backend_fixed_frame_bytecode(canonical_phase59(
        huge, 0U, BackendStackGrowthDirection::toward_lower_addresses, huge));

    const std::vector<std::int64_t> zero_stack = {11, 0};
    const auto zero_result =
        execute_backend_fixed_frame_bytecode(plan, zero_stack);
    REQUIRE_EQ(zero_result.after_entry_registers[0],
               std::numeric_limits<std::int64_t>::min());
    REQUIRE_EQ(zero_result.after_entry_registers[1],
               std::numeric_limits<std::int64_t>::min());
    REQUIRE_EQ(zero_result.after_exit_registers[1], std::int64_t{0});
    REQUIRE_EQ(zero_stack, (std::vector<std::int64_t>{11, 0}));

    const std::vector<std::int64_t> max_stack = {
        0, std::numeric_limits<std::int64_t>::max()};
    const auto max_result = execute_backend_fixed_frame_bytecode(plan, max_stack);
    REQUIRE_EQ(max_result.after_entry_registers[0], std::int64_t{-1});
    REQUIRE_EQ(max_result.after_entry_registers[1], std::int64_t{-1});
    REQUIRE_EQ(max_result.after_exit_registers[1],
               std::numeric_limits<std::int64_t>::max());

    const std::vector<std::int64_t> underflow = {
        0, std::numeric_limits<std::int64_t>::min()};
    REQUIRE_THROWS_AS(execute_backend_fixed_frame_bytecode(plan, underflow),
                      std::overflow_error);
    REQUIRE_EQ(underflow[1], std::numeric_limits<std::int64_t>::min());
  }
}

TEST_CASE(backend_frame_bytecode_execution_matches_independent_raw_replay) {
  std::mt19937_64 rng(0x61B17EC0DEULL);
  std::uniform_int_distribution<std::size_t> frame_distribution(0U, 4096U);
  std::uniform_int_distribution<std::size_t> immediate_distribution(1U, 512U);
  std::uniform_int_distribution<std::int64_t> register_distribution(-1000000,
                                                                    1000000);
  const std::array<BackendStackGrowthDirection, 2U> directions = {
      BackendStackGrowthDirection::toward_lower_addresses,
      BackendStackGrowthDirection::toward_higher_addresses};

  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const std::size_t frame_size = frame_distribution(rng);
    std::uniform_int_distribution<std::size_t> anchor_distribution(0U,
                                                                  frame_size);
    const std::size_t anchor = anchor_distribution(rng);
    const std::size_t max_immediate = immediate_distribution(rng);

    for (const auto direction : directions) {
      const auto plan = encode_backend_fixed_frame_bytecode(canonical_phase59(
          frame_size, anchor, direction, max_immediate));
      const std::vector<std::int64_t> initial = {
          register_distribution(rng), register_distribution(rng)};

      const auto expected = independent_raw_execution(plan.bytes, initial);
      const auto actual = execute_backend_fixed_frame_bytecode(plan, initial);
      REQUIRE_EQ(actual, expected);
      REQUIRE(initial[0] >= -1000000 && initial[0] <= 1000000);
      REQUIRE(initial[1] >= -1000000 && initial[1] <= 1000000);
    }
  }
}

}  // namespace
