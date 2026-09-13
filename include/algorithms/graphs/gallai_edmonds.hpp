#pragma once

#include <algorithm>
#include <cstddef>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/general_matching.hpp"
#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct GallaiEdmondsResult {
  GeneralMatchingResult maximum_matching;
  std::vector<Vertex> d_vertices;
  std::vector<Vertex> a_vertices;
  std::vector<Vertex> c_vertices;
  std::vector<std::vector<Vertex>> d_components;
  std::vector<std::vector<Vertex>> c_components;
  std::size_t blossom_calls;
};

namespace gallai_edmonds_detail {

[[nodiscard]] inline std::vector<std::vector<bool>> simple_adjacency(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("Gallai-Edmonds decomposition requires undirected input");
  }
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to != u) {
        adjacent[u][edge.to] = true;
        adjacent[edge.to][u] = true;
      }
    }
  }
  return adjacent;
}

[[nodiscard]] inline Graph without_vertex(
    const std::vector<std::vector<bool>>& adjacent, Vertex removed) {
  const std::size_t n = adjacent.size();
  Graph reduced(n - 1U, false);
  std::vector<Vertex> map(n, n);
  Vertex next = 0;
  for (Vertex v = 0; v < n; ++v) {
    if (v != removed) {
      map[v] = next++;
    }
  }
  for (Vertex u = 0; u < n; ++u) {
    if (u == removed) continue;
    for (Vertex v = u + 1U; v < n; ++v) {
      if (v != removed && adjacent[u][v]) {
        reduced.add_edge(map[u], map[v]);
      }
    }
  }
  return reduced;
}

[[nodiscard]] inline std::vector<std::vector<Vertex>> induced_components(
    const std::vector<std::vector<bool>>& adjacent,
    const std::vector<bool>& included) {
  const std::size_t n = included.size();
  std::vector<bool> seen(n, false);
  std::vector<std::vector<Vertex>> components;
  for (Vertex start = 0; start < n; ++start) {
    if (!included[start] || seen[start]) continue;
    std::vector<Vertex> component;
    std::queue<Vertex> queue;
    seen[start] = true;
    queue.push(start);
    while (!queue.empty()) {
      const Vertex u = queue.front();
      queue.pop();
      component.push_back(u);
      for (Vertex v = 0; v < n; ++v) {
        if (included[v] && adjacent[u][v] && !seen[v]) {
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

}  // namespace gallai_edmonds_detail

[[nodiscard]] inline GallaiEdmondsResult gallai_edmonds_decomposition(
    const Graph& graph) {
  const auto adjacent = gallai_edmonds_detail::simple_adjacency(graph);
  const std::size_t n = graph.vertex_count();
  GeneralMatchingResult maximum = edmonds_blossom_maximum_matching(graph);
  std::size_t blossom_calls = 1;

  std::vector<bool> in_d(n, false);
  for (Vertex removed = 0; removed < n; ++removed) {
    const Graph reduced = gallai_edmonds_detail::without_vertex(adjacent, removed);
    const auto reduced_matching = edmonds_blossom_maximum_matching(reduced);
    ++blossom_calls;
    in_d[removed] = reduced_matching.cardinality == maximum.cardinality;
  }

  std::vector<bool> in_a(n, false);
  for (Vertex d = 0; d < n; ++d) {
    if (!in_d[d]) continue;
    for (Vertex v = 0; v < n; ++v) {
      if (!in_d[v] && adjacent[d][v]) {
        in_a[v] = true;
      }
    }
  }

  std::vector<bool> in_c(n, false);
  std::vector<Vertex> d_vertices;
  std::vector<Vertex> a_vertices;
  std::vector<Vertex> c_vertices;
  for (Vertex v = 0; v < n; ++v) {
    if (in_d[v]) {
      d_vertices.push_back(v);
    } else if (in_a[v]) {
      a_vertices.push_back(v);
    } else {
      in_c[v] = true;
      c_vertices.push_back(v);
    }
  }

  return GallaiEdmondsResult{
      std::move(maximum),
      std::move(d_vertices),
      std::move(a_vertices),
      std::move(c_vertices),
      gallai_edmonds_detail::induced_components(adjacent, in_d),
      gallai_edmonds_detail::induced_components(adjacent, in_c),
      blossom_calls};
}

}  // namespace algorithms::graphs
