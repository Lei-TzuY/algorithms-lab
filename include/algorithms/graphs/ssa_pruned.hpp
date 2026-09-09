#pragma once

#include "algorithms/graphs/ssa_construction.hpp"

#include <cstddef>
#include <vector>

namespace algorithms::graphs {

struct SsaLiveness {
  Vertex start;
  std::size_t variable_count;
  std::vector<unsigned char> reachable;
  std::vector<std::vector<unsigned char>> live_in;
  std::vector<std::vector<unsigned char>> live_out;

  friend bool operator==(const SsaLiveness&, const SsaLiveness&) = default;
};

// Analyze block-entry/block-exit liveness for the Phase-45 scalar event IR.
// Uses in an instruction are observed before that instruction's optional
// definition, matching SSA renaming semantics. Unreachable blocks remain false.
[[nodiscard]] SsaLiveness analyze_ssa_liveness(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& blocks);

// Construct deterministic liveness-pruned SSA. Phi candidates are considered
// through dominance frontiers, but a phi is materialized only when its variable
// is live-in at that block. Only a materialized phi becomes a new definition
// site for further frontier propagation.
[[nodiscard]] SsaProgram construct_pruned_ssa(
    const Graph& graph, Vertex start, std::size_t variable_count,
    const std::vector<std::vector<SsaInputInstruction>>& blocks);

}  // namespace algorithms::graphs
