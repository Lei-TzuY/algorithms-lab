#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::graphs {

struct CycleBasisEdgeRef {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;

  friend bool operator==(const CycleBasisEdgeRef&,
                         const CycleBasisEdgeRef&) = default;
};

struct CycleBasisCycle {
  std::vector<std::size_t> logical_edge_ids;
  std::uint64_t weight;

  friend bool operator==(const CycleBasisCycle&,
                         const CycleBasisCycle&) = default;
};

struct MinimumCycleBasisResult {
  std::vector<CycleBasisEdgeRef> logical_edges;
  std::vector<CycleBasisCycle> cycles;
  std::size_t cycle_space_dimension;
  std::uint64_t total_weight;

  friend bool operator==(const MinimumCycleBasisResult&,
                         const MinimumCycleBasisResult&) = default;
};

// Exact minimum-weight cycle basis for an undirected positive-weight multigraph.
//
// Parallel copies and self-loops are distinct logical edges. All logical edge
// weights must be strictly positive; directed input and non-positive weights are
// rejected. The returned cycles are GF(2) edge vectors represented by sorted
// logical-edge ids, and every logical edge id resolves through logical_edges to
// one original adjacency entry.
//
// Production generates Horton's O(VE) shortest-path-tree candidate cycles and
// greedily selects a minimum-weight independent subset over GF(2). Required
// arithmetic that cannot be represented in uint64_t fails closed; overflowing
// non-selected candidates do not by themselves reject a representable optimum.
[[nodiscard]] MinimumCycleBasisResult minimum_weight_cycle_basis(
    const Graph& graph);

}  // namespace algorithms::graphs
