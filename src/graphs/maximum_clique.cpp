#include "algorithms/graphs/maximum_clique.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

using AdjacencyMatrix = std::vector<std::vector<std::uint8_t>>;

[[nodiscard]] std::vector<Vertex> intersect_neighbors(
    const std::vector<Vertex>& vertices, Vertex pivot,
    const AdjacencyMatrix& adjacent) {
  std::vector<Vertex> result;
  result.reserve(vertices.size());
  for (const Vertex vertex : vertices) {
    if (adjacent[pivot][vertex] != 0U) result.push_back(vertex);
  }
  return result;
}

[[nodiscard]] Vertex choose_pivot(const std::vector<Vertex>& candidates,
                                  const std::vector<Vertex>& excluded,
                                  const AdjacencyMatrix& adjacent) {
  Vertex best = 0;
  bool have_best = false;
  std::size_t best_neighbors = 0;

  const auto consider = [&](Vertex vertex, Vertex& best_vertex,
                            bool& has_best, std::size_t& neighbor_count) {
    std::size_t count = 0;
    for (const Vertex candidate : candidates) {
      if (adjacent[vertex][candidate] != 0U) ++count;
    }
    if (!has_best || count > neighbor_count ||
        (count == neighbor_count && vertex < best_vertex)) {
      best_vertex = vertex;
      has_best = true;
      neighbor_count = count;
    }
  };

  for (const Vertex vertex : candidates) {
    consider(vertex, best, have_best, best_neighbors);
  }
  for (const Vertex vertex : excluded) {
    consider(vertex, best, have_best, best_neighbors);
  }
  return best;
}

void update_best(const std::vector<Vertex>& current, MaximumCliqueResult& result) {
  std::vector<Vertex> sorted = current;
  std::sort(sorted.begin(), sorted.end());
  if (sorted.size() > result.vertices.size() ||
      (sorted.size() == result.vertices.size() && sorted < result.vertices)) {
    result.vertices = std::move(sorted);
  }
}

void bron_kerbosch(std::vector<Vertex>& current, std::vector<Vertex> candidates,
                   std::vector<Vertex> excluded,
                   const AdjacencyMatrix& adjacent, MaximumCliqueResult& result) {
  ++result.recursive_calls;
  if (candidates.empty() && excluded.empty()) {
    ++result.maximal_cliques_examined;
    update_best(current, result);
    return;
  }

  const Vertex pivot = choose_pivot(candidates, excluded, adjacent);
  std::vector<Vertex> branch_vertices;
  branch_vertices.reserve(candidates.size());
  for (const Vertex vertex : candidates) {
    if (adjacent[pivot][vertex] == 0U) branch_vertices.push_back(vertex);
  }

  for (const Vertex vertex : branch_vertices) {
    current.push_back(vertex);
    const auto next_candidates = intersect_neighbors(candidates, vertex, adjacent);
    const auto next_excluded = intersect_neighbors(excluded, vertex, adjacent);
    bron_kerbosch(current, next_candidates, next_excluded, adjacent, result);
    current.pop_back();

    const auto candidate_position =
        std::lower_bound(candidates.begin(), candidates.end(), vertex);
    if (candidate_position != candidates.end() && *candidate_position == vertex) {
      candidates.erase(candidate_position);
    }
    excluded.insert(std::lower_bound(excluded.begin(), excluded.end(), vertex), vertex);
  }
}

}  // namespace

MaximumCliqueResult maximum_clique_bron_kerbosch(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("maximum clique requires an undirected graph");
  }

  const std::size_t vertex_count = graph.vertex_count();
  AdjacencyMatrix adjacent(
      vertex_count, std::vector<std::uint8_t>(vertex_count, std::uint8_t{0}));
  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (from == edge.to) continue;
      adjacent[from][edge.to] = 1U;
      adjacent[edge.to][from] = 1U;
    }
  }

  std::vector<Vertex> candidates(vertex_count);
  std::iota(candidates.begin(), candidates.end(), Vertex{0});
  std::vector<Vertex> current;
  std::vector<Vertex> excluded;
  MaximumCliqueResult result;
  bron_kerbosch(current, std::move(candidates), std::move(excluded), adjacent, result);
  return result;
}

}  // namespace algorithms::graphs
