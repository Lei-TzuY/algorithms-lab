#pragma once
#include "algorithms/graphs/graph.hpp"
#include <cstddef>
#include <utility>
#include <vector>
namespace algorithms::graphs {
struct BlockCutIncidence {
  Vertex articulation_vertex;
  std::size_t block_index;
  friend bool operator==(const BlockCutIncidence&, const BlockCutIncidence&) = default;
};
struct VertexBiconnectedDecomposition {
  std::vector<std::vector<Vertex>> blocks;
  std::vector<Vertex> articulation_vertices;
  std::vector<BlockCutIncidence> block_cut_incidence;
};
// Structural vertex-biconnected decomposition of an undirected multigraph.
// Parallel copies collapse to one structural edge; self-loops do not affect
// vertex connectivity. Bridge edges form two-vertex blocks and structurally
// isolated vertices form singleton blocks.
[[nodiscard]] VertexBiconnectedDecomposition vertex_biconnected_decomposition(const Graph& graph);
}
