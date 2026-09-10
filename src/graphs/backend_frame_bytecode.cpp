#include "algorithms/graphs/backend_frame_bytecode.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <variant>

namespace algorithms::graphs {
namespace {

constexpr std::size_t kEncodedU64Bytes = 8U;
constexpr std::size_t kMinimumEncodedInstructionBytes = 17U;

std::uint64_t size_to_u64(const std::size_t value) {
  if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max())) {
      throw std::length_error(
          "fixed-frame bytecode size_t operand exceeds uint64 wire width");
    }
  }
  return static_cast<std::uint64_t>(value);
}

std::size_t u64_to_size(const std::uint64_t value) {
  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max())) {
      throw std::invalid_argument(
          "fixed-frame bytecode operand exceeds host size_t width");
    }
  }
  return static_cast<std::size_t>(value);
}

void append_u64_le(std::vector<std::uint8_t>& bytes,
                   const std::uint64_t value) {
  for (std::size_t index = 0U; index < kEncodedU64Bytes; ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

std::uint8_t read_byte(const std::span<const std::uint8_t> bytes,
                       std::size_t& offset) {
  if (offset >= bytes.size()) {
    throw std::invalid_argument("truncated fixed-frame bytecode");
  }
  return bytes[offset++];
}

std::uint64_t read_u64_le(const std::span<const std::uint8_t> bytes,
                          std::size_t& offset) {
  if (offset > bytes.size() || bytes.size() - offset < kEncodedU64Bytes) {
    throw std::invalid_argument("truncated fixed-frame uint64 operand");
  }

  std::uint64_t value = 0U;
  for (std::size_t index = 0U; index < kEncodedU64Bytes; ++index) {
    const auto shift = static_cast<unsigned int>(index * 8U);
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << shift;
  }
  offset += kEncodedU64Bytes;
  return value;
}

std::int64_t u64_bits_to_i64(const std::uint64_t bits) {
  const auto max_i64 =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (bits <= max_i64) {
    return static_cast<std::int64_t>(bits);
  }

  const std::uint64_t complement_magnitude =
      std::numeric_limits<std::uint64_t>::max() - bits;
  return -std::int64_t{1} -
         static_cast<std::int64_t>(complement_magnitude);
}

void encode_instruction(
    const BackendSymbolicFixedFrameInstruction& instruction,
    std::vector<std::uint8_t>& bytes) {
  if (const auto* decrement =
          std::get_if<BackendStackPointerDecrementImmediateInstruction>(
              &instruction)) {
    bytes.push_back(static_cast<std::uint8_t>(
        BackendFixedFrameBytecodeOpcode::stack_pointer_decrement_immediate));
    append_u64_le(bytes,
                  size_to_u64(decrement->stack_pointer_physical_register));
    append_u64_le(bytes, size_to_u64(decrement->immediate_byte_count));
    return;
  }

  if (const auto* increment =
          std::get_if<BackendStackPointerIncrementImmediateInstruction>(
              &instruction)) {
    bytes.push_back(static_cast<std::uint8_t>(
        BackendFixedFrameBytecodeOpcode::stack_pointer_increment_immediate));
    append_u64_le(bytes,
                  size_to_u64(increment->stack_pointer_physical_register));
    append_u64_le(bytes, size_to_u64(increment->immediate_byte_count));
    return;
  }

  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &instruction);
  if (frame_base == nullptr) {
    throw std::logic_error(
        "fixed-frame bytecode encoder received unsupported instruction");
  }
  bytes.push_back(static_cast<std::uint8_t>(
      BackendFixedFrameBytecodeOpcode::frame_base_from_stack_pointer_immediate));
  append_u64_le(bytes, size_to_u64(frame_base->frame_base_physical_register));
  append_u64_le(bytes, size_to_u64(frame_base->stack_pointer_physical_register));
  append_u64_le(bytes,
                static_cast<std::uint64_t>(frame_base->immediate_displacement));
}

BackendSymbolicFixedFrameInstruction decode_instruction(
    const std::span<const std::uint8_t> bytes, std::size_t& offset) {
  const std::uint8_t raw_opcode = read_byte(bytes, offset);
  const auto opcode = static_cast<BackendFixedFrameBytecodeOpcode>(raw_opcode);

  switch (opcode) {
    case BackendFixedFrameBytecodeOpcode::stack_pointer_decrement_immediate:
      return BackendStackPointerDecrementImmediateInstruction{
          u64_to_size(read_u64_le(bytes, offset)),
          u64_to_size(read_u64_le(bytes, offset))};
    case BackendFixedFrameBytecodeOpcode::stack_pointer_increment_immediate:
      return BackendStackPointerIncrementImmediateInstruction{
          u64_to_size(read_u64_le(bytes, offset)),
          u64_to_size(read_u64_le(bytes, offset))};
    case BackendFixedFrameBytecodeOpcode::frame_base_from_stack_pointer_immediate:
      return BackendFrameBaseFromStackPointerImmediateInstruction{
          u64_to_size(read_u64_le(bytes, offset)),
          u64_to_size(read_u64_le(bytes, offset)),
          u64_bits_to_i64(read_u64_le(bytes, offset))};
  }

  throw std::invalid_argument("unknown fixed-frame bytecode opcode");
}

void ensure_count_can_fit_remaining(const std::size_t count,
                                    const std::size_t remaining_bytes,
                                    const std::size_t required_tail_bytes) {
  if (remaining_bytes < required_tail_bytes) {
    throw std::invalid_argument("truncated fixed-frame bytecode sequence");
  }
  const std::size_t instruction_bytes = remaining_bytes - required_tail_bytes;
  if (count > instruction_bytes / kMinimumEncodedInstructionBytes) {
    throw std::invalid_argument(
        "fixed-frame bytecode instruction count exceeds remaining bytes");
  }
}

}  // namespace

BackendFixedFrameBytecodePlan encode_backend_fixed_frame_bytecode(
    const BackendSymbolicFixedFrameInstructionPlan& source_plan) {
  const BackendSymbolicFixedFrameInstructionPlan canonical =
      lower_fixed_frame_actions_to_symbolic_instructions(source_plan.source_plan);
  if (canonical != source_plan) {
    throw std::logic_error(
        "fixed-frame bytecode encoding requires a canonical Phase-59 plan");
  }

  BackendFixedFrameBytecodePlan result;
  result.source_plan = source_plan;
  auto& bytes = result.bytes;
  bytes.push_back(kBackendFixedFrameBytecodeMagic0);
  bytes.push_back(kBackendFixedFrameBytecodeMagic1);
  bytes.push_back(kBackendFixedFrameBytecodeMagic2);
  bytes.push_back(kBackendFixedFrameBytecodeMagic3);
  bytes.push_back(kBackendFixedFrameBytecodeVersion);

  append_u64_le(bytes, size_to_u64(source_plan.entry_instructions.size()));
  for (const auto& instruction : source_plan.entry_instructions) {
    encode_instruction(instruction, bytes);
  }
  append_u64_le(bytes, size_to_u64(source_plan.exit_instructions.size()));
  for (const auto& instruction : source_plan.exit_instructions) {
    encode_instruction(instruction, bytes);
  }
  return result;
}

DecodedBackendFixedFrameBytecode decode_backend_fixed_frame_bytecode(
    const std::span<const std::uint8_t> bytes) {
  std::size_t offset = 0U;
  if (read_byte(bytes, offset) != kBackendFixedFrameBytecodeMagic0 ||
      read_byte(bytes, offset) != kBackendFixedFrameBytecodeMagic1 ||
      read_byte(bytes, offset) != kBackendFixedFrameBytecodeMagic2 ||
      read_byte(bytes, offset) != kBackendFixedFrameBytecodeMagic3) {
    throw std::invalid_argument("invalid fixed-frame bytecode magic");
  }
  if (read_byte(bytes, offset) != kBackendFixedFrameBytecodeVersion) {
    throw std::invalid_argument("unsupported fixed-frame bytecode version");
  }

  DecodedBackendFixedFrameBytecode result;
  const std::size_t entry_count = u64_to_size(read_u64_le(bytes, offset));
  ensure_count_can_fit_remaining(entry_count, bytes.size() - offset,
                                 kEncodedU64Bytes);
  result.entry_instructions.reserve(entry_count);
  for (std::size_t index = 0U; index < entry_count; ++index) {
    result.entry_instructions.push_back(decode_instruction(bytes, offset));
  }

  const std::size_t exit_count = u64_to_size(read_u64_le(bytes, offset));
  ensure_count_can_fit_remaining(exit_count, bytes.size() - offset, 0U);
  result.exit_instructions.reserve(exit_count);
  for (std::size_t index = 0U; index < exit_count; ++index) {
    result.exit_instructions.push_back(decode_instruction(bytes, offset));
  }

  if (offset != bytes.size()) {
    throw std::invalid_argument("trailing bytes after fixed-frame bytecode plan");
  }
  return result;
}

}  // namespace algorithms::graphs
