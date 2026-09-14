#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <vector>

namespace algorithms::graphs {

struct SeparationPairWitness {
  Vertex first{};
  Vertex second{};
  std::vector<std::vector<Vertex>> remaining_components;

  friend bool operator==(const SeparationPairWitness&,
                         const SeparationPairWitness&) = default;
};

enum class BiconnectedBlockKind {
  singleton,
  bridge,
  triangle,
  triconnected,
  split_pair_decomposable,
};

struct BiconnectedBlockTriconnectivity {
  std::vector<Vertex> vertices;
  BiconnectedBlockKind kind{BiconnectedBlockKind::singleton};
  std::vector<SeparationPairWitness> separation_pairs;

  friend bool operator==(const BiconnectedBlockTriconnectivity&,
                         const BiconnectedBlockTriconnectivity&) = default;
};

struct TriconnectivityAnalysis {
  std::vector<Vertex> articulation_vertices;
  std::vector<BiconnectedBlockTriconnectivity> blocks;

  friend bool operator==(const TriconnectivityAnalysis&,
                         const TriconnectivityAnalysis&) = default;
};

// Structural separation-pair analysis for an undirected multigraph.
//
// The sealed block-cut decomposition first splits the graph at articulation
// vertices. Within each vertex-biconnected block, self-loops are ignored and
// parallel copies collapse to one structural adjacency. For every block with at
// least four vertices, every unordered vertex pair whose deletion disconnects
// the remaining block is returned together with the deterministic connected
// components that witness the split.
//
// A block is classified as triconnected exactly when it has at least four
// vertices and no separation pair. Three-vertex blocks are reported as
// triangles/small atoms rather than being called 3-connected. This API is an
// exact separation-pair/triconnectivity analysis; it is deliberately not a
// canonical SPQR-tree construction.
[[nodiscard]] TriconnectivityAnalysis analyze_triconnectivity(
    const Graph& graph);

}  // namespace algorithms::graphs
