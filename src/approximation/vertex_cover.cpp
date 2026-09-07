#include "algorithms/approximation/vertex_cover.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace algorithms::approximation {
namespace {

using graphs::Vertex;

}  // namespace

VertexCoverApproximationResult approximate_minimum_vertex_cover(
    const graphs::Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("vertex cover approximation requires undirected input");
  }

  const std::size_t n = graph.vertex_count();
  std::vector<bool> forced(n, false);

  // A self-loop (v,v) can only be covered by v, so every such vertex belongs
  // to every feasible cover. Mark them before building the residual matching.
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to == u) {
        forced[u] = true;
        break;
      }
    }
  }

  VertexCoverApproximationResult result;
  for (Vertex v = 0; v < n; ++v) {
    if (forced[v]) {
      result.forced_self_loop_vertices.push_back(v);
    }
  }

  std::vector<bool> matched(n, false);

  // Deterministic greedy maximal matching on edges not already covered by a
  // forced self-loop vertex. Each undirected edge is considered from its
  // smaller endpoint only; parallel copies do not alter feasibility.
  for (Vertex u = 0; u < n; ++u) {
    if (forced[u] || matched[u]) {
      continue;
    }
    for (const auto& edge : graph.neighbors(u)) {
      const Vertex v = edge.to;
      if (v <= u || forced[v] || matched[v]) {
        continue;
      }
      matched[u] = true;
      matched[v] = true;
      result.maximal_matching_edges.emplace_back(u, v);
      break;
    }
  }

  std::vector<bool> in_cover = forced;
  for (const auto& [u, v] : result.maximal_matching_edges) {
    in_cover[u] = true;
    in_cover[v] = true;
  }

  for (Vertex v = 0; v < n; ++v) {
    if (in_cover[v]) {
      result.vertices.push_back(v);
    }
  }
  return result;
}

}  // namespace algorithms::approximation
