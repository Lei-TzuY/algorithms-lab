#pragma once

#include <algorithm>
#include <cstddef>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/gallai_edmonds.hpp"

namespace algorithms::graphs {

struct TutteBergeCertificate {
  GeneralMatchingResult maximum_matching;
  std::vector<Vertex> barrier_vertices;
  std::vector<std::vector<Vertex>> odd_components;
  std::vector<std::vector<Vertex>> even_components;
  std::size_t deficiency{};
  std::size_t unmatched_vertex_count{};
  std::size_t gallai_blossom_calls{};
};

namespace tutte_berge_detail {

[[nodiscard]] inline std::vector<std::vector<Vertex>> components_after_barrier(
    const Graph& graph, const std::vector<bool>& removed) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    if (removed[u]) continue;
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to != u && !removed[edge.to]) {
        adjacent[u][edge.to] = true;
        adjacent[edge.to][u] = true;
      }
    }
  }

  std::vector<bool> seen(n, false);
  std::vector<std::vector<Vertex>> components;
  for (Vertex start = 0; start < n; ++start) {
    if (removed[start] || seen[start]) continue;
    std::queue<Vertex> queue;
    std::vector<Vertex> component;
    seen[start] = true;
    queue.push(start);
    while (!queue.empty()) {
      const Vertex u = queue.front();
      queue.pop();
      component.push_back(u);
      for (Vertex v = 0; v < n; ++v) {
        if (!removed[v] && adjacent[u][v] && !seen[v]) {
          seen[v] = true;
          queue.push(v);
        }
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  return components;
}

}  // namespace tutte_berge_detail

[[nodiscard]] inline TutteBergeCertificate tutte_berge_certificate(
    const Graph& graph) {
  GallaiEdmondsResult decomposition = gallai_edmonds_decomposition(graph);
  const std::size_t n = graph.vertex_count();

  std::vector<bool> removed(n, false);
  for (const Vertex vertex : decomposition.a_vertices) {
    removed[vertex] = true;
  }

  std::vector<std::vector<Vertex>> odd_components;
  std::vector<std::vector<Vertex>> even_components;
  for (auto component :
       tutte_berge_detail::components_after_barrier(graph, removed)) {
    if (component.size() % 2U == 1U) {
      odd_components.push_back(std::move(component));
    } else {
      even_components.push_back(std::move(component));
    }
  }

  if (odd_components.size() < decomposition.a_vertices.size()) {
    throw std::logic_error("Gallai-Edmonds barrier produced negative deficiency");
  }
  if (decomposition.maximum_matching.cardinality > n / 2U) {
    throw std::logic_error("maximum matching cardinality exceeds vertex bound");
  }

  const std::size_t deficiency =
      odd_components.size() - decomposition.a_vertices.size();
  const std::size_t unmatched =
      n - 2U * decomposition.maximum_matching.cardinality;
  if (deficiency != unmatched) {
    throw std::logic_error("Tutte-Berge equality violated by decomposition");
  }

  return TutteBergeCertificate{
      std::move(decomposition.maximum_matching),
      std::move(decomposition.a_vertices),
      std::move(odd_components),
      std::move(even_components),
      deficiency,
      unmatched,
      decomposition.blossom_calls};
}

}  // namespace algorithms::graphs
