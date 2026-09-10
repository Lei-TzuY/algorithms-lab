#include "algorithms/graphs/ssa_scalar_semantics.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void validate_semantic_shape(const std::size_t use_count,
                             const bool has_definition,
                             const SsaInstructionSemantics& semantics) {
  const bool has_immediate = semantics.immediate.has_value();
  switch (semantics.opcode) {
    case SsaScalarOpcode::opaque:
      if (has_immediate) {
        throw std::invalid_argument("opaque SSA instruction cannot carry immediate");
      }
      return;
    case SsaScalarOpcode::constant_i64:
      if (use_count != 0U || !has_definition || !has_immediate) {
        throw std::invalid_argument(
            "constant_i64 requires zero uses, one definition, and an immediate");
      }
      return;
    case SsaScalarOpcode::copy_i64:
      if (use_count != 1U || !has_definition || has_immediate) {
        throw std::invalid_argument(
            "copy_i64 requires one use, one definition, and no immediate");
      }
      return;
    case SsaScalarOpcode::add_i64:
    case SsaScalarOpcode::subtract_i64:
    case SsaScalarOpcode::multiply_i64:
    case SsaScalarOpcode::equal_i64:
    case SsaScalarOpcode::less_than_i64:
      if (use_count != 2U || !has_definition || has_immediate) {
        throw std::invalid_argument(
            "binary scalar opcode requires two uses, one definition, and no immediate");
      }
      return;
  }
  throw std::invalid_argument("unknown SSA scalar opcode");
}

[[nodiscard]] std::int64_t checked_add(const std::int64_t left,
                                       const std::int64_t right) {
  constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  if ((right > 0 && left > maximum - right) ||
      (right < 0 && left < minimum - right)) {
    throw std::overflow_error("SSA scalar addition overflow");
  }
  return left + right;
}

[[nodiscard]] std::int64_t checked_subtract(const std::int64_t left,
                                            const std::int64_t right) {
  constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  if ((right > 0 && left < minimum + right) ||
      (right < 0 && left > maximum + right)) {
    throw std::overflow_error("SSA scalar subtraction overflow");
  }
  return left - right;
}

[[nodiscard]] std::int64_t checked_multiply(const std::int64_t left,
                                            const std::int64_t right) {
  constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  if (left == 0 || right == 0) {
    return 0;
  }
  if ((left == -1 && right == minimum) ||
      (right == -1 && left == minimum)) {
    throw std::overflow_error("SSA scalar multiplication overflow");
  }
  if (left > 0) {
    if ((right > 0 && left > maximum / right) ||
        (right < 0 && right < minimum / left)) {
      throw std::overflow_error("SSA scalar multiplication overflow");
    }
  } else {
    if ((right > 0 && left < minimum / right) ||
        (right < 0 && left < maximum / right)) {
      throw std::overflow_error("SSA scalar multiplication overflow");
    }
  }
  return left * right;
}

}  // namespace

SsaProgram construct_semantic_ssa(
    const Graph& graph, const Vertex start, const std::size_t variable_count,
    const std::vector<std::vector<SemanticSsaInputInstruction>>& blocks) {
  std::vector<std::vector<SsaInputInstruction>> dataflow(blocks.size());
  for (std::size_t block = 0U; block < blocks.size(); ++block) {
    dataflow[block].reserve(blocks[block].size());
    for (const SemanticSsaInputInstruction& instruction : blocks[block]) {
      validate_semantic_shape(instruction.uses.size(),
                              instruction.definition.has_value(),
                              instruction.semantics);
      dataflow[block].push_back(
          SsaInputInstruction{instruction.uses, instruction.definition});
    }
  }

  SsaProgram program = construct_ssa(graph, start, variable_count, dataflow);
  if (program.blocks.size() != blocks.size()) {
    throw std::logic_error("semantic SSA block count changed during construction");
  }
  for (std::size_t block = 0U; block < blocks.size(); ++block) {
    if (program.blocks[block].instructions.size() != blocks[block].size()) {
      throw std::logic_error(
          "semantic SSA instruction count changed during construction");
    }
    for (std::size_t index = 0U; index < blocks[block].size(); ++index) {
      program.blocks[block].instructions[index].semantics =
          blocks[block][index].semantics;
    }
  }
  return program;
}

void validate_ssa_instruction_semantics(const SsaInstruction& instruction) {
  validate_semantic_shape(instruction.uses.size(),
                          instruction.definition.has_value(),
                          instruction.semantics);
}

bool ssa_instruction_has_executable_scalar_semantics(
    const SsaInstruction& instruction) {
  validate_ssa_instruction_semantics(instruction);
  return instruction.semantics.opcode != SsaScalarOpcode::opaque;
}

std::int64_t evaluate_ssa_scalar_instruction(
    const SsaInstruction& instruction,
    const std::span<const std::int64_t> operand_values) {
  validate_ssa_instruction_semantics(instruction);
  if (operand_values.size() != instruction.uses.size()) {
    throw std::invalid_argument("SSA scalar operand count does not match uses");
  }

  switch (instruction.semantics.opcode) {
    case SsaScalarOpcode::opaque:
      throw std::invalid_argument("opaque SSA instruction is not executable");
    case SsaScalarOpcode::constant_i64:
      return *instruction.semantics.immediate;
    case SsaScalarOpcode::copy_i64:
      return operand_values[0U];
    case SsaScalarOpcode::add_i64:
      return checked_add(operand_values[0U], operand_values[1U]);
    case SsaScalarOpcode::subtract_i64:
      return checked_subtract(operand_values[0U], operand_values[1U]);
    case SsaScalarOpcode::multiply_i64:
      return checked_multiply(operand_values[0U], operand_values[1U]);
    case SsaScalarOpcode::equal_i64:
      return operand_values[0U] == operand_values[1U] ? 1 : 0;
    case SsaScalarOpcode::less_than_i64:
      return operand_values[0U] < operand_values[1U] ? 1 : 0;
  }
  throw std::invalid_argument("unknown SSA scalar opcode");
}

}  // namespace algorithms::graphs
