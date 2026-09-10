#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::graphs {

using Variable = std::size_t;

struct SsaInputInstruction {
  std::vector<Variable> uses;
  std::optional<Variable> definition;
  friend bool operator==(const SsaInputInstruction&,
                         const SsaInputInstruction&) = default;
};

struct SsaValue {
  Variable variable;
  std::size_t version;
  friend bool operator==(const SsaValue&, const SsaValue&) = default;
};

enum class SsaScalarOpcode : unsigned char {
  opaque,
  constant_i64,
  copy_i64,
  add_i64,
  subtract_i64,
  multiply_i64,
  equal_i64,
  less_than_i64,
};

struct SsaInstructionSemantics {
  SsaScalarOpcode opcode{SsaScalarOpcode::opaque};
  std::optional<std::int64_t> immediate;
  friend bool operator==(const SsaInstructionSemantics&,
                         const SsaInstructionSemantics&) = default;
};

struct SsaInstruction {
  std::vector<SsaValue> uses;
  std::optional<SsaValue> definition;
  SsaInstructionSemantics semantics{};
  friend bool operator==(const SsaInstruction&, const SsaInstruction&) = default;
};

struct SsaPhiIncoming {
  Vertex predecessor;
  SsaValue value;
  friend bool operator==(const SsaPhiIncoming&,
                         const SsaPhiIncoming&) = default;
};

struct SsaPhi {
  Variable variable;
  SsaValue result;
  std::vector<SsaPhiIncoming> incoming;
  friend bool operator==(const SsaPhi&, const SsaPhi&) = default;
};

struct SsaBlock {
  bool reachable;
  std::vector<SsaPhi> phis;
  std::vector<SsaInstruction> instructions;
  friend bool operator==(const SsaBlock&, const SsaBlock&) = default;
};

struct SsaProgram {
  Vertex start;
  std::size_t variable_count;
  std::vector<SsaValue> initial_values;
  std::vector<SsaBlock> blocks;
  friend bool operator==(const SsaProgram&, const SsaProgram&) = default;
};

// Construct deterministic IDF-based SSA over the start-reachable CFG.
//
// Every variable has an explicit version-0 entry value. The entry block must
// have no predecessor edge. Unreachable blocks must carry no input
// instructions; they remain explicitly outside the returned SSA domain.
[[nodiscard]] SsaProgram construct_ssa(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& blocks);

}  // namespace algorithms::graphs
