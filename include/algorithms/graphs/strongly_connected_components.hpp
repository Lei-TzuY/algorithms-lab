#pragma once

#include <cstddef>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct StronglyConnectedComponents {
  // component_of[v] is an algorithm-assigned component ID in
  // [0, components.size()).
  std::vector<std::size_t> component_of;
  std::vector<std::vector<Vertex>> components;
};

// Directed graphs only.
//
// Tarjan invariant: lowlink[v] is the smallest DFS discovery index reachable
// from v while staying within the active DFS stack. When lowlink[v] equals
// index[v], v is the root of exactly one maximal SCC.
// Time: O(V + E), auxiliary space: O(V) excluding recursion stack.
[[nodiscard]] StronglyConnectedComponents tarjan_strongly_connected_components(
    const Graph& graph);

// Kosaraju proof obligation: decreasing finish time from the first DFS orders
// SCCs so that a DFS in the transposed graph cannot escape into an unassigned
// SCC. Each second-pass DFS therefore discovers exactly one SCC.
// Time: O(V + E), auxiliary space: O(V + E) including transpose storage.
[[nodiscard]] StronglyConnectedComponents
kosaraju_strongly_connected_components(const Graph& graph);

// Contracts each SCC to one vertex and removes duplicate inter-component edges.
// The result must be a DAG. Edge weights are intentionally discarded because
// condensation captures reachability structure, not weighted path semantics.
// Time: O(E log E) due deterministic duplicate removal; space: O(V + E).
[[nodiscard]] Graph condensation_graph(
    const Graph& graph, const StronglyConnectedComponents& decomposition);

}  // namespace algorithms::graphs
