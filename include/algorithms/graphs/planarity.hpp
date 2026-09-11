#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct PlanarRotationEmbedding {
  // Cyclic order of the unique non-self-loop neighbors around every vertex in
  // the simplified underlying graph. Parallel copies and self-loops are
  // topologically neutral for planarity and therefore do not appear here.
  std::vector<std::vector<Vertex>> clockwise_neighbors;
  std::vector<std::size_t> component_face_counts;
  std::size_t rotation_systems_tested{};
};

// Exact educational baseline for undirected multigraph planarity.
//
// The solver simplifies self-loops/parallel copies, enumerates deterministic
// orientable rotation systems component-by-component, and returns the first
// genus-zero embedding. Directed input is rejected.
//
// This is intentionally not a linear-time planarity implementation. If d(v)
// is the degree in the simplified graph, the search may examine up to
// product_v max(1, (d(v)-1)!) rotation systems. Each complete assignment is
// checked in O(E * Delta) time by replaying face cycles.
[[nodiscard]] std::optional<PlanarRotationEmbedding>
exact_planar_rotation_embedding(const Graph& graph);

}  // namespace algorithms::graphs
