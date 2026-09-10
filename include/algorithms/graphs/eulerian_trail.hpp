#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct EulerianTrailResult {
  std::vector<Vertex> vertices;
  std::size_t edge_count = 0;
  bool is_circuit = false;

  friend bool operator==(const EulerianTrailResult&,
                         const EulerianTrailResult&) = default;
};

// Returns one deterministic Eulerian trail that consumes every logical edge
// exactly once, or nullopt when no such trail exists. Directed and undirected
// multigraphs, parallel edges, and self-loops are supported. Edge weights are
// intentionally ignored because Eulerian feasibility is structural.
//
// Canonical trivial semantics:
// - empty graph: empty vertex sequence, circuit=true;
// - non-empty edgeless graph: {0}, circuit=true.
[[nodiscard]] std::optional<EulerianTrailResult> eulerian_trail(
    const Graph& graph);

}  // namespace algorithms::graphs
