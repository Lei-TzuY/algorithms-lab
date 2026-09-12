#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/max_flow.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct VertexConnectivityResult {
  std::size_t connectivity{};
  std::vector<Vertex> separator;
  std::optional<std::pair<Vertex, Vertex>> separated_pair;
  bool input_connected{};
  bool complete_graph{};
};

namespace vertex_connectivity_detail {

using StructuralAdjacency = std::vector<std::vector<unsigned char>>;

[[nodiscard]] inline StructuralAdjacency structural_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  StructuralAdjacency adjacency(n, std::vector<unsigned char>(n, 0U));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to != from) {
        adjacency[from][edge.to] = 1U;
      }
    }
  }
  return adjacency;
}

[[nodiscard]] inline std::vector<bool> reachable_without(
    const StructuralAdjacency& adjacency, const std::vector<bool>& removed,
    Vertex start) {
  std::vector<bool> seen(adjacency.size(), false);
  if (removed[start]) {
    return seen;
  }
  std::queue<Vertex> queue;
  seen[start] = true;
  queue.push(start);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (Vertex next = 0; next < adjacency.size(); ++next) {
      if (adjacency[vertex][next] != 0U && !removed[next] && !seen[next]) {
        seen[next] = true;
        queue.push(next);
      }
    }
  }
  return seen;
}

[[nodiscard]] inline std::optional<std::pair<Vertex, Vertex>>
first_disconnected_pair(const StructuralAdjacency& adjacency,
                        const std::vector<bool>& removed) {
  const std::size_t n = adjacency.size();
  Vertex first = n;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (!removed[vertex]) {
      first = vertex;
      break;
    }
  }
  if (first == n) {
    return std::nullopt;
  }
  const auto seen = reachable_without(adjacency, removed, first);
  for (Vertex vertex = first + 1U; vertex < n; ++vertex) {
    if (!removed[vertex] && !seen[vertex]) {
      return std::pair<Vertex, Vertex>{first, vertex};
    }
  }
  return std::nullopt;
}

struct LocalVertexCut {
  std::size_t value{};
  std::vector<Vertex> separator;
};

[[nodiscard]] inline LocalVertexCut minimum_nonadjacent_pair_cut(
    const StructuralAdjacency& adjacency, Vertex source, Vertex sink) {
  const std::size_t n = adjacency.size();
  if (n > std::numeric_limits<std::size_t>::max() / 2U) {
    throw std::length_error("vertex-split network size is not representable");
  }
  if (n >= static_cast<std::size_t>(std::numeric_limits<Capacity>::max())) {
    throw std::length_error("vertex-connectivity capacity domain exceeded");
  }

  const Capacity infinite_capacity = static_cast<Capacity>(n) + 1;
  std::vector<CapacityEdge> edges;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    const Capacity capacity =
        (vertex == source || vertex == sink) ? infinite_capacity : 1;
    edges.push_back(CapacityEdge{2U * vertex, 2U * vertex + 1U, capacity});
  }
  for (Vertex left = 0; left < n; ++left) {
    for (Vertex right = left + 1U; right < n; ++right) {
      if (adjacency[left][right] == 0U) {
        continue;
      }
      edges.push_back(CapacityEdge{2U * left + 1U, 2U * right,
                                   infinite_capacity});
      edges.push_back(CapacityEdge{2U * right + 1U, 2U * left,
                                   infinite_capacity});
    }
  }

  const auto flow = dinic_max_flow(2U * n, edges, 2U * source + 1U, 2U * sink);
  if (flow.value < 0 || flow.value >= infinite_capacity) {
    throw std::logic_error("invalid local vertex-cut capacity");
  }

  std::vector<Vertex> separator;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (vertex == source || vertex == sink) {
      continue;
    }
    const Vertex in = 2U * vertex;
    const Vertex out = in + 1U;
    if (flow.source_side_min_cut[in] && !flow.source_side_min_cut[out]) {
      separator.push_back(vertex);
    }
  }
  if (separator.size() != static_cast<std::size_t>(flow.value)) {
    throw std::logic_error("vertex-cut witness size does not match max flow");
  }

  std::vector<bool> removed(n, false);
  for (const Vertex vertex : separator) {
    removed[vertex] = true;
  }
  const auto seen = reachable_without(adjacency, removed, source);
  if (seen[sink]) {
    throw std::logic_error("vertex-cut witness does not separate its pair");
  }
  return LocalVertexCut{separator.size(), std::move(separator)};
}

}  // namespace vertex_connectivity_detail

// Exact global vertex connectivity of an undirected structural graph.
// Self-loops and edge weights are ignored; parallel copies collapse to one
// structural adjacency relation. For a complete graph K_n (n >= 2), the
// conventional value n-1 is returned with a separator that leaves one vertex.
// For a disconnected graph the value is zero and separated_pair witnesses two
// vertices already disconnected by the empty separator.
[[nodiscard]] inline VertexConnectivityResult exact_vertex_connectivity(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("vertex connectivity requires an undirected graph");
  }

  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    return VertexConnectivityResult{};
  }
  if (n == 1U) {
    return VertexConnectivityResult{0U, {}, std::nullopt, true, true};
  }

  const auto adjacency = vertex_connectivity_detail::structural_adjacency(graph);
  const std::vector<bool> none_removed(n, false);
  if (const auto pair = vertex_connectivity_detail::first_disconnected_pair(
          adjacency, none_removed);
      pair.has_value()) {
    return VertexConnectivityResult{0U, {}, pair, false, false};
  }

  bool complete = true;
  for (Vertex left = 0; left < n; ++left) {
    for (Vertex right = left + 1U; right < n; ++right) {
      if (adjacency[left][right] == 0U) {
        complete = false;
      }
    }
  }
  if (complete) {
    std::vector<Vertex> separator;
    separator.reserve(n - 1U);
    for (Vertex vertex = 0; vertex + 1U < n; ++vertex) {
      separator.push_back(vertex);
    }
    return VertexConnectivityResult{n - 1U, std::move(separator), std::nullopt,
                                    true, true};
  }

  std::size_t best = n;
  std::vector<Vertex> best_separator;
  std::optional<std::pair<Vertex, Vertex>> best_pair;
  for (Vertex source = 0; source < n; ++source) {
    for (Vertex sink = source + 1U; sink < n; ++sink) {
      if (adjacency[source][sink] != 0U) {
        continue;
      }
      auto local = vertex_connectivity_detail::minimum_nonadjacent_pair_cut(
          adjacency, source, sink);
      if (local.value < best) {
        best = local.value;
        best_separator = std::move(local.separator);
        best_pair = std::pair<Vertex, Vertex>{source, sink};
      }
    }
  }
  if (!best_pair.has_value()) {
    throw std::logic_error("non-complete graph has no non-adjacent vertex pair");
  }
  return VertexConnectivityResult{best, std::move(best_separator), best_pair,
                                  true, false};
}

}  // namespace algorithms::graphs
