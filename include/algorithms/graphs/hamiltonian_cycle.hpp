#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct HamiltonianCycleEdge {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};

  friend bool operator==(const HamiltonianCycleEdge&,
                         const HamiltonianCycleEdge&) = default;
};

struct HamiltonianCycleResult {
  Weight total_weight{};
  std::vector<Vertex> cycle_vertices;
  std::vector<HamiltonianCycleEdge> edges;
  std::size_t reachable_dp_states{};
};

// Exact minimum-weight directed Hamiltonian cycle rooted at vertex 0.
//
// Empty and singleton directed graphs have the canonical zero-edge, zero-cost
// trivial cycle. For n >= 2, every vertex appears exactly once before returning
// to vertex 0. Parallel arcs are supported; for one ordered vertex pair the
// cheapest arc is sufficient, with the smallest adjacency index breaking equal
// weight ties. Self-loops are ignored.
//
// The direct Held-Karp baseline is intentionally capped at 16 vertices. It uses
// O(2^V V^2 + E) time and O(2^V V) state. DP costs are accumulated in a private
// wider exact signed representation, so an overflowing non-optimal tour does not
// reject a representable optimum. If the exact optimum itself is outside int64,
// std::overflow_error is thrown.
[[nodiscard]] std::optional<HamiltonianCycleResult>
minimum_directed_hamiltonian_cycle(const Graph& graph);

}  // namespace algorithms::graphs
