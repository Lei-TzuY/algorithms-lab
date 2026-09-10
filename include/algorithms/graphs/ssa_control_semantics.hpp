#pragma once

#include "algorithms/graphs/ssa_destruction.hpp"
#include "algorithms/graphs/ssa_scalar_semantics.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

// Input control semantics are attached per original CFG block. The conditional
// predicate is a source variable; construction resolves it to the exact SSA
// reaching definition at block exit before SSA destruction.
struct SsaControlTerminatorInput {
  SsaControlTerminatorKind kind{SsaControlTerminatorKind::opaque};
  std::optional<Variable> predicate;
  std::optional<Vertex> jump_successor;
  std::optional<Vertex> nonzero_successor;
  std::optional<Vertex> zero_successor;

  friend bool operator==(const SsaControlTerminatorInput&,
                         const SsaControlTerminatorInput&) = default;
};

// Construct semantic scalar SSA through the sealed Phase-66 constructor, bind
// branch predicates to their renamed SSA values, destroy SSA through the sealed
// Phase-47 lowering, and attach explicit lowered control descriptors.
//
// Reachable jump/branch targets must be explicit CFG edges. Unreachable blocks
// and legacy callers remain opaque. Critical phi-copy edges are mapped to the
// concrete split block that must execute before the logical successor; no target
// is inferred from adjacency order or block numbering.
[[nodiscard]] OutOfSsaProgram construct_control_semantic_out_of_ssa(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SemanticSsaInputInstruction>>& blocks,
    const std::vector<SsaControlTerminatorInput>& controls);

}  // namespace algorithms::graphs
