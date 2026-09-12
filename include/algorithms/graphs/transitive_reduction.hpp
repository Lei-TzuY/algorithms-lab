#pragma once

#include "algorithms/graphs/graph.hpp"

#include <vector>

namespace algorithms::graphs {

struct DirectedArc {
  Vertex from;
  Vertex to;

  friend bool operator==(const DirectedArc&, const DirectedArc&) = default;
};

struct TransitiveReductionResult {
  // Canonical structural arcs sorted lexicographically by (from, to).
  // Parallel input arcs collapse to one reachability relation; weights are
  // intentionally ignored because transitive reduction is structural.
  std::vector<DirectedArc> arcs;

  // Deterministic Kahn topological order for replay/validation.
  std::vector<Vertex> topological_order;
};

// Returns the unique transitive reduction of a directed acyclic graph's
// reachability relation. Undirected or cyclic input is rejected.
[[nodiscard]] TransitiveReductionResult transitive_reduction(const Graph& graph);

}  // namespace algorithms::graphs
