#pragma once

#include <cstddef>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct WeightedEdge {
  Vertex from;
  Vertex to;
  Weight weight;

  friend bool operator==(const WeightedEdge&, const WeightedEdge&) = default;
};

struct MinimumSpanningForest {
  std::vector<WeightedEdge> edges;
  Weight total_weight{0};
  std::size_t component_count{0};
};

// Both algorithms require an undirected graph and return a minimum spanning
// forest when the graph is disconnected. Self-loops never enter the forest;
// parallel edges and negative weights are supported.
//
// Kruskal invariant: before each accepted edge, the chosen edges are acyclic
// and can be extended to a minimum spanning forest. The lightest edge crossing
// two current DSU components is safe by the cut property.
// Time: O(E log E), space: O(V + E).
[[nodiscard]] MinimumSpanningForest kruskal_minimum_spanning_forest(
    const Graph& graph);

// Prim invariant: for each connected component, the visited vertices are
// connected by the chosen tree edges; the next accepted edge is the lightest
// edge crossing from visited to unvisited vertices, so the cut property makes
// it safe.
// This implementation uses the lab's first-principles binary heap and may keep
// stale crossing edges in the heap.
// Time: O(E log E), space: O(V + E).
[[nodiscard]] MinimumSpanningForest prim_minimum_spanning_forest(
    const Graph& graph);

}  // namespace algorithms::graphs
