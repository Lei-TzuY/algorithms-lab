#include "algorithms/graphs/backend_frame_bytecode.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>

namespace {
using namespace algorithms::graphs;

std::int64_t size_to_i64(const std::size_t value) {
  if (value > static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("test size is not int64 representable");
  }
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_add(const std::int64_t left, const std::int64_t right) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error("test replay positive overflow");
  }
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error("test replay negative overflow");
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

void append_u64_spec(std::vector<std::uint8_t>& bytes,
                     const std::uint64_t value) {
  for (unsigned int shift = 0U; shift < 64U; shift += 8U) {
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

void append_stack_instruction_spec(std::vector<std::uint8_t>& bytes,
                                   const std::uint8_t opcode,
                                   const std::uint64_t reg,
                                   const std::uint64_t magnitude) {
  bytes.push_back(opcode);
  append_u64_spec(bytes, reg);
  append_u64_spec(bytes, magnitude);
}

struct RegisterState {
  std::vector<std::int64_t> registers;

  friend bool operator==(const RegisterState&, const RegisterState&) = default;
};

std::int64_t& register_value(RegisterState& state, const std::size_t id) {
  if (id >= state.registers.size()) {
    state.registers.resize(id + 1U, 0);
  }
  return state.registers[id];
}

void apply_stack_delta(RegisterState& state, const std::size_t reg,
                       const bool decrement, const std::size_t magnitude) {
  const std::int64_t signed_magnitude = size_to_i64(magnitude);
  auto& value = register_value(state, reg);
  value = checked_add(value, decrement ? -signed_magnitude : signed_magnitude);
}

void replay_symbolic_instruction(
    const BackendSymbolicFixedFrameInstruction& instruction,
    RegisterState& state) {
  if (const auto* decrement =
          std::get_if<BackendStackPointerDecrementImmediateInstruction>(
              &instruction)) {
    apply_stack_delta(state, decrement->stack_pointer_physical_register, true,
                      decrement->immediate_byte_count);
    return;
  }
  if (const auto* increment =
          std::get_if<BackendStackPointerIncrementImmediateInstruction>(
              &instruction)) {
    apply_stack_delta(state, increment->stack_pointer_physical_register, false,
                      increment->immediate_byte_count);
    return;
  }
  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &instruction);
  REQUIRE(frame_base != nullptr);
  const std::int64_t stack_pointer =
      register_value(state, frame_base->stack_pointer_physical_register);
  register_value(state, frame_base->frame_base_physical_register) = checked_add(
      stack_pointer, frame_base->immediate_displacement);
}

void replay_symbolic_sequence(
    const std::vector<BackendSymbolicFixedFrameInstruction>& instructions,
    RegisterState& state) {
  for (const auto& instruction : instructions) {
    replay_symbolic_instruction(instruction, state);
  }
}

struct IndependentCursor {
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

std::size_t independent_size(const std::uint64_t value) {
  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
    REQUIRE(value <= static_cast<std::uint64_t>(
                         std::numeric_limits<std::size_t>::max()));
  }
  return static_cast<std::size_t>(value);
}

std::int64_t independent_i64(const std::uint64_t bits) {
  const auto max_i64 =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (bits <= max_i64) {
    return static_cast<std::int64_t>(bits);
  }
  return -std::int64_t{1} - static_cast<std::int64_t>(
                                 std::numeric_limits<std::uint64_t>::max() -
                                 bits);
}

void replay_wire_instruction(IndependentCursor& cursor, RegisterState& state) {
  const std::uint8_t opcode = cursor.byte();
  if (opcode == 0x01U || opcode == 0x02U) {
    const std::size_t reg = independent_size(cursor.u64());
    const std::size_t magnitude = independent_size(cursor.u64());
    apply_stack_delta(state, reg, opcode == 0x01U, magnitude);
    return;
  }
  REQUIRE_EQ(opcode, std::uint8_t{0x03U});
  const std::size_t frame_base = independent_size(cursor.u64());
  const std::size_t stack_pointer = independent_size(cursor.u64());
  const std::int64_t displacement = independent_i64(cursor.u64());
  register_value(state, frame_base) =
      checked_add(register_value(state, stack_pointer), displacement);
}

struct ReplaySnapshots {
  RegisterState after_entry;
  RegisterState after_exit;
};

ReplaySnapshots independently_replay_bytecode(
    const std::vector<std::uint8_t>& bytes, RegisterState initial) {
  IndependentCursor cursor{bytes};
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x41U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x4cU});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x46U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x42U});
  REQUIRE_EQ(cursor.byte(), std::uint8_t{0x01U});

  const std::size_t entry_count = independent_size(cursor.u64());
  for (std::size_t index = 0U; index < entry_count; ++index) {
    replay_wire_instruction(cursor, initial);
  }
  ReplaySnapshots result;
  result.after_entry = initial;

  const std::size_t exit_count = independent_size(cursor.u64());
  for (std::size_t index = 0U; index < exit_count; ++index) {
    replay_wire_instruction(cursor, initial);
  }
  REQUIRE_EQ(cursor.offset, bytes.size());
  result.after_exit = initial;
  return result;
}

TEST_CASE(backend_frame_bytecode_has_one_known_canonical_encoding) {
  const auto phase59 = canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses, 4U);
  const auto encoded = encode_backend_fixed_frame_bytecode(phase59);
  REQUIRE_EQ(encoded.source_plan, phase59);

  std::vector<std::uint8_t> expected = {0x41U, 0x4cU, 0x46U, 0x42U, 0x01U};
  append_u64_spec(expected, 4U);
  append_stack_instruction_spec(expected, 0x01U, 1U, 4U);
  append_stack_instruction_spec(expected, 0x01U, 1U, 4U);
  append_stack_instruction_spec(expected, 0x01U, 1U, 2U);
  expected.push_back(0x03U);
  append_u64_spec(expected, 0U);
  append_u64_spec(expected, 1U);
  append_u64_spec(expected, 3U);
  append_u64_spec(expected, 3U);
  append_stack_instruction_spec(expected, 0x02U, 1U, 2U);
  append_stack_instruction_spec(expected, 0x02U, 1U, 4U);
  append_stack_instruction_spec(expected, 0x02U, 1U, 4U);
  REQUIRE_EQ(encoded.bytes, expected);

  const auto decoded = decode_backend_fixed_frame_bytecode(encoded.bytes);
  REQUIRE_EQ(decoded.entry_instructions, phase59.entry_instructions);
  REQUIRE_EQ(decoded.exit_instructions, phase59.exit_instructions);
  REQUIRE_EQ(encode_backend_fixed_frame_bytecode(phase59), encoded);

  const auto upward = canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_higher_addresses, 4U);
  const auto upward_encoded = encode_backend_fixed_frame_bytecode(upward);
  const auto upward_decoded =
      decode_backend_fixed_frame_bytecode(upward_encoded.bytes);
  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &upward_decoded.entry_instructions.back());
  REQUIRE(frame_base != nullptr);
  REQUIRE_EQ(frame_base->immediate_displacement, std::int64_t{-7});
}

TEST_CASE(backend_frame_bytecode_rejects_tampered_plan_and_malformed_streams) {
  const auto phase59 = canonical_phase59(
      10U, 3U, BackendStackGrowthDirection::toward_lower_addresses, 4U);
  const auto encoded = encode_backend_fixed_frame_bytecode(phase59);

  auto tampered = phase59;
  ++std::get<BackendStackPointerDecrementImmediateInstruction>(
        tampered.entry_instructions.front())
        .immediate_byte_count;
  REQUIRE_THROWS_AS(encode_backend_fixed_frame_bytecode(tampered),
                    std::logic_error);

  auto bad = encoded.bytes;
  bad[0] ^= 0xffU;
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  bad = encoded.bytes;
  bad[4] = 0xffU;
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  bad = encoded.bytes;
  bad[13] = 0xffU;
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  bad = encoded.bytes;
  bad.pop_back();
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  bad = encoded.bytes;
  bad.push_back(0U);
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  bad = encoded.bytes;
  for (std::size_t index = 5U; index < 13U; ++index) {
    bad[index] = 0xffU;
  }
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(bad),
                    std::invalid_argument);

  const std::vector<std::uint8_t> too_short = {0x41U, 0x4cU, 0x46U};
  REQUIRE_THROWS_AS(decode_backend_fixed_frame_bytecode(too_short),
                    std::invalid_argument);
}

TEST_CASE(backend_frame_bytecode_preserves_fixed_width_integer_boundaries) {
  if constexpr (std::numeric_limits<std::size_t>::digits >= 64) {
    const std::size_t huge =
        static_cast<std::size_t>(std::uint64_t{1U} << 63U);
    const auto phase59 = canonical_phase59(
        huge, 0U, BackendStackGrowthDirection::toward_lower_addresses, huge);
    const auto encoded = encode_backend_fixed_frame_bytecode(phase59);
    const auto decoded = decode_backend_fixed_frame_bytecode(encoded.bytes);
    REQUIRE_EQ(decoded.entry_instructions, phase59.entry_instructions);
    REQUIRE_EQ(decoded.exit_instructions, phase59.exit_instructions);
    const auto* decrement =
        std::get_if<BackendStackPointerDecrementImmediateInstruction>(
            &decoded.entry_instructions.front());
    REQUIRE(decrement != nullptr);
    REQUIRE_EQ(decrement->immediate_byte_count, huge);
  }

  std::vector<std::uint8_t> bytes = {0x41U, 0x4cU, 0x46U, 0x42U, 0x01U};
  append_u64_spec(bytes, 1U);
  bytes.push_back(0x03U);
  append_u64_spec(bytes, 0U);
  append_u64_spec(bytes, 1U);
  append_u64_spec(bytes, std::uint64_t{1U} << 63U);
  append_u64_spec(bytes, 0U);
  const auto decoded = decode_backend_fixed_frame_bytecode(bytes);
  REQUIRE_EQ(decoded.entry_instructions.size(), std::size_t{1U});
  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &decoded.entry_instructions.front());
  REQUIRE(frame_base != nullptr);
  REQUIRE_EQ(frame_base->immediate_displacement,
             std::numeric_limits<std::int64_t>::min());
  REQUIRE(decoded.exit_instructions.empty());
}

TEST_CASE(backend_frame_bytecode_randomized_decode_and_independent_replay) {
  std::mt19937_64 random(0x60B17E5ULL);
  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const std::size_t frame_size = static_cast<std::size_t>(random() % 1001U);
    const std::size_t anchor =
        static_cast<std::size_t>(random() % (frame_size + 1U));
    const std::size_t maximum_immediate =
        1U + static_cast<std::size_t>(random() % 31U);

    for (const BackendStackGrowthDirection direction : {
             BackendStackGrowthDirection::toward_lower_addresses,
             BackendStackGrowthDirection::toward_higher_addresses}) {
      const auto phase59 =
          canonical_phase59(frame_size, anchor, direction, maximum_immediate);
      const auto encoded = encode_backend_fixed_frame_bytecode(phase59);
      const auto decoded = decode_backend_fixed_frame_bytecode(encoded.bytes);
      REQUIRE_EQ(decoded.entry_instructions, phase59.entry_instructions);
      REQUIRE_EQ(decoded.exit_instructions, phase59.exit_instructions);
      REQUIRE_EQ(encode_backend_fixed_frame_bytecode(phase59), encoded);

      RegisterState symbolic;
      symbolic.registers.resize(2U, 0);
      symbolic.registers[1] = 1000000;
      RegisterState initial = symbolic;
      replay_symbolic_sequence(phase59.entry_instructions, symbolic);
      const RegisterState symbolic_after_entry = symbolic;
      replay_symbolic_sequence(phase59.exit_instructions, symbolic);

      const ReplaySnapshots wire =
          independently_replay_bytecode(encoded.bytes, initial);
      REQUIRE_EQ(wire.after_entry, symbolic_after_entry);
      REQUIRE_EQ(wire.after_exit, symbolic);
      REQUIRE_EQ(wire.after_exit.registers[1], std::int64_t{1000000});
    }
  }
}

}  // namespace
