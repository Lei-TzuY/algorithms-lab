#include "algorithms/graphs/backend_frame_bytecode_execution.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

namespace algorithms::graphs {
namespace {

constexpr std::uint64_t kInt64NegativeMagnitudeLimit =
    std::uint64_t{1U} << 63U;

std::uint64_t size_to_u64(const std::size_t value) {
  if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
    if (value > static_cast<std::size_t>(
                    std::numeric_limits<std::uint64_t>::max())) {
      throw std::overflow_error(
          "fixed-frame execution magnitude exceeds uint64 wire domain");
    }
  }
  return static_cast<std::uint64_t>(value);
}

std::uint64_t negative_magnitude(const std::int64_t value) {
  if (value >= 0) {
    return 0U;
  }
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

std::int64_t signed_from_negative_magnitude(const std::uint64_t magnitude) {
  if (magnitude > kInt64NegativeMagnitudeLimit) {
    throw std::overflow_error("fixed-frame execution signed underflow");
  }
  if (magnitude == kInt64NegativeMagnitudeLimit) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return -static_cast<std::int64_t>(magnitude);
}

std::int64_t checked_increment_by_magnitude(const std::int64_t value,
                                            const std::uint64_t magnitude) {
  if (value >= 0) {
    const auto room = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max() - value);
    if (magnitude > room) {
      throw std::overflow_error("fixed-frame stack increment overflow");
    }
    return value + static_cast<std::int64_t>(magnitude);
  }

  const std::uint64_t absolute_value = negative_magnitude(value);
  if (magnitude < absolute_value) {
    return signed_from_negative_magnitude(absolute_value - magnitude);
  }

  const std::uint64_t nonnegative = magnitude - absolute_value;
  if (nonnegative > static_cast<std::uint64_t>(
                        std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("fixed-frame stack increment overflow");
  }
  return static_cast<std::int64_t>(nonnegative);
}

std::int64_t checked_decrement_by_magnitude(const std::int64_t value,
                                            const std::uint64_t magnitude) {
  if (value <= 0) {
    const std::uint64_t absolute_value = negative_magnitude(value);
    const std::uint64_t room =
        kInt64NegativeMagnitudeLimit - absolute_value;
    if (magnitude > room) {
      throw std::overflow_error("fixed-frame stack decrement underflow");
    }
    return signed_from_negative_magnitude(absolute_value + magnitude);
  }

  const auto positive = static_cast<std::uint64_t>(value);
  if (magnitude <= positive) {
    return static_cast<std::int64_t>(positive - magnitude);
  }
  return signed_from_negative_magnitude(magnitude - positive);
}

std::int64_t checked_add(const std::int64_t left,
                         const std::int64_t right) {
  if (right > 0 &&
      left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error("fixed-frame register addition overflow");
  }
  if (right < 0 &&
      left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error("fixed-frame register addition underflow");
  }
  return left + right;
}

void validate_register(const std::size_t register_id,
                       const std::size_t register_count) {
  if (register_id >= register_count) {
    throw std::out_of_range(
        "fixed-frame bytecode references register outside finite file");
  }
}

void validate_instruction_registers(
    const BackendSymbolicFixedFrameInstruction& instruction,
    const std::size_t register_count) {
  if (const auto* decrement =
          std::get_if<BackendStackPointerDecrementImmediateInstruction>(
              &instruction)) {
    validate_register(decrement->stack_pointer_physical_register,
                      register_count);
    return;
  }
  if (const auto* increment =
          std::get_if<BackendStackPointerIncrementImmediateInstruction>(
              &instruction)) {
    validate_register(increment->stack_pointer_physical_register,
                      register_count);
    return;
  }
  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &instruction);
  if (frame_base == nullptr) {
    throw std::logic_error(
        "fixed-frame execution received unsupported decoded instruction");
  }
  validate_register(frame_base->frame_base_physical_register, register_count);
  validate_register(frame_base->stack_pointer_physical_register, register_count);
}

void validate_sequence_registers(
    const std::vector<BackendSymbolicFixedFrameInstruction>& instructions,
    const std::size_t register_count) {
  for (const auto& instruction : instructions) {
    validate_instruction_registers(instruction, register_count);
  }
}

void execute_instruction(
    const BackendSymbolicFixedFrameInstruction& instruction,
    std::vector<std::int64_t>& registers) {
  if (const auto* decrement =
          std::get_if<BackendStackPointerDecrementImmediateInstruction>(
              &instruction)) {
    auto& stack_pointer =
        registers[decrement->stack_pointer_physical_register];
    stack_pointer = checked_decrement_by_magnitude(
        stack_pointer, size_to_u64(decrement->immediate_byte_count));
    return;
  }
  if (const auto* increment =
          std::get_if<BackendStackPointerIncrementImmediateInstruction>(
              &instruction)) {
    auto& stack_pointer =
        registers[increment->stack_pointer_physical_register];
    stack_pointer = checked_increment_by_magnitude(
        stack_pointer, size_to_u64(increment->immediate_byte_count));
    return;
  }
  const auto* frame_base =
      std::get_if<BackendFrameBaseFromStackPointerImmediateInstruction>(
          &instruction);
  if (frame_base == nullptr) {
    throw std::logic_error(
        "fixed-frame execution received unsupported decoded instruction");
  }
  registers[frame_base->frame_base_physical_register] = checked_add(
      registers[frame_base->stack_pointer_physical_register],
      frame_base->immediate_displacement);
}

void execute_sequence(
    const std::vector<BackendSymbolicFixedFrameInstruction>& instructions,
    std::vector<std::int64_t>& registers) {
  for (const auto& instruction : instructions) {
    execute_instruction(instruction, registers);
  }
}

}  // namespace

BackendFixedFrameBytecodeExecutionSnapshots execute_backend_fixed_frame_bytecode(
    const BackendFixedFrameBytecodePlan& plan,
    const std::span<const std::int64_t> initial_registers) {
  const DecodedBackendFixedFrameBytecode decoded =
      decode_backend_fixed_frame_bytecode(plan.bytes);

  const BackendFixedFrameBytecodePlan canonical =
      encode_backend_fixed_frame_bytecode(plan.source_plan);
  if (canonical != plan) {
    throw std::logic_error(
        "fixed-frame execution requires canonical Phase-60 bytecode");
  }
  if (decoded.entry_instructions != plan.source_plan.entry_instructions ||
      decoded.exit_instructions != plan.source_plan.exit_instructions) {
    throw std::logic_error(
        "fixed-frame decoded instructions disagree with canonical provenance");
  }

  validate_sequence_registers(decoded.entry_instructions,
                              initial_registers.size());
  validate_sequence_registers(decoded.exit_instructions,
                              initial_registers.size());

  std::vector<std::int64_t> working(initial_registers.begin(),
                                    initial_registers.end());
  execute_sequence(decoded.entry_instructions, working);

  BackendFixedFrameBytecodeExecutionSnapshots result;
  result.after_entry_registers = working;

  execute_sequence(decoded.exit_instructions, working);
  result.after_exit_registers = std::move(working);
  return result;
}

}  // namespace algorithms::graphs
