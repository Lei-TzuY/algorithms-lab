#pragma once

#include "algorithms/graphs/ssa_construction.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct SemanticSsaInputInstruction {
  std::vector<Variable> uses;
  std::optional<Variable> definition;
  SsaInstructionSemantics semantics{};

  friend bool operator==(const SemanticSsaInputInstruction&,
                         const SemanticSsaInputInstruction&) = default;
};

// Construct SSA through the sealed dataflow constructor, then attach validated
// scalar semantics to the renamed instructions. The legacy construct_ssa() API
// remains unchanged and therefore produces opaque instructions by default.
[[nodiscard]] SsaProgram construct_semantic_ssa(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SemanticSsaInputInstruction>>& blocks);

// Validate the executable-shape contract carried by one canonical SSA
// instruction. Opaque instructions retain the legacy arbitrary use/definition
// shape but may not carry an immediate. Known scalar opcodes have fixed arity,
// require a definition, and accept an immediate only for constant_i64.
void validate_ssa_instruction_semantics(const SsaInstruction& instruction);

[[nodiscard]] bool ssa_instruction_has_executable_scalar_semantics(
    const SsaInstruction& instruction);

// Evaluate one known scalar opcode from explicit operand values. The operand
// span must match instruction.uses exactly. Arithmetic is checked in int64_t;
// unrepresentable add/subtract/multiply results throw std::overflow_error.
// Opaque instructions are deliberately not evaluated here.
[[nodiscard]] std::int64_t evaluate_ssa_scalar_instruction(
    const SsaInstruction& instruction,
    std::span<const std::int64_t> operand_values);

}  // namespace algorithms::graphs
