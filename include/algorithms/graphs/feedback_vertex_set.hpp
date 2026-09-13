#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

inline constexpr std::size_t kMaxExactFeedbackVertexSetVertices = 20;

struct DirectedFeedbackVertexSetResult {
  std::vector<Vertex> removed_vertices;
  std::vector<Vertex> forced_self_loop_vertices;
  std::vector<Vertex> topological_order;
  std::size_t search_states{};

  friend bool operator==(const DirectedFeedbackVertexSetResult&,
                         const DirectedFeedbackVertexSetResult&) = default;
};

namespace feedback_vertex_set_detail {

using Mask = std::uint64_t;

[[nodiscard]] inline bool removed(Mask mask, Vertex vertex) {
  return (mask & (Mask{1} << vertex)) != 0U;
}

[[nodiscard]] inline std::vector<Vertex> mask_vertices(Mask mask,
                                                        std::size_t n) {
  std::vector<Vertex> vertices;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (removed(mask, vertex)) {
      vertices.push_back(vertex);
    }
  }
  return vertices;
}

[[nodiscard]] inline std::vector<Vertex> find_directed_cycle(
    const Graph& graph, Mask removed_mask) {
  const std::size_t n = graph.vertex_count();
  std::vector<unsigned char> color(n, 0U);
  std::vector<Vertex> parent(n, n);
  std::vector<Vertex> cycle;

  const auto dfs = [&](auto&& self, Vertex vertex) -> bool {
    color[vertex] = 1U;
    for (const Edge& edge : graph.neighbors(vertex)) {
      const Vertex next = edge.to;
      if (removed(removed_mask, next)) {
        continue;
      }
      if (color[next] == 0U) {
        parent[next] = vertex;
        if (self(self, next)) {
          return true;
        }
      } else if (color[next] == 1U) {
        cycle.push_back(next);
        Vertex current = vertex;
        while (current != next) {
          cycle.push_back(current);
          if (current >= n || parent[current] >= n) {
            throw std::logic_error("feedback-vertex-set DFS parent invariant broken");
          }
          current = parent[current];
        }
        std::sort(cycle.begin(), cycle.end());
        cycle.erase(std::unique(cycle.begin(), cycle.end()), cycle.end());
        return true;
      }
    }
    color[vertex] = 2U;
    return false;
  };

  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (!removed(removed_mask, vertex) && color[vertex] == 0U &&
        dfs(dfs, vertex)) {
      return cycle;
    }
  }
  return {};
}

[[nodiscard]] inline std::vector<Vertex> topological_order(
    const Graph& graph, Mask removed_mask) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::size_t> indegree(n, 0U);
  std::size_t kept = 0U;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (removed(removed_mask, vertex)) {
      continue;
    }
    ++kept;
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (!removed(removed_mask, edge.to)) {
        if (indegree[edge.to] == std::numeric_limits<std::size_t>::max()) {
          throw std::overflow_error("feedback-vertex-set indegree overflow");
        }
        ++indegree[edge.to];
      }
    }
  }

  std::priority_queue<Vertex, std::vector<Vertex>, std::greater<Vertex>> ready;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (!removed(removed_mask, vertex) && indegree[vertex] == 0U) {
      ready.push(vertex);
    }
  }

  std::vector<Vertex> order;
  order.reserve(kept);
  while (!ready.empty()) {
    const Vertex vertex = ready.top();
    ready.pop();
    order.push_back(vertex);
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (removed(removed_mask, edge.to)) {
        continue;
      }
      if (indegree[edge.to] == 0U) {
        throw std::logic_error("feedback-vertex-set indegree invariant broken");
      }
      --indegree[edge.to];
      if (indegree[edge.to] == 0U) {
        ready.push(edge.to);
      }
    }
  }
  if (order.size() != kept) {
    throw std::logic_error("feedback-vertex-set topological certificate is cyclic");
  }
  return order;
}

[[nodiscard]] inline bool lexicographically_better(Mask candidate, Mask best,
                                                   std::size_t n) {
  return mask_vertices(candidate, n) < mask_vertices(best, n);
}

}  // namespace feedback_vertex_set_detail

// Exact bounded minimum directed feedback vertex set.
//
// Preconditions / semantics:
// - graph must be directed;
// - at most kMaxExactFeedbackVertexSetVertices vertices are accepted;
// - self-loops force their incident vertex into every feasible solution;
// - parallel arcs are allowed and edge weights are ignored;
// - ties among optimum vertex sets use lexicographically smallest sorted ids.
//
// The algorithm repeatedly extracts one directed cycle from the residual graph
// and branches on deleting each distinct vertex of that cycle. Every feedback
// vertex set must hit that cycle, so the branching is exact. Removed-subset
// memoization bounds the number of explored states by 2^V. The intentionally
// direct bounded implementation makes no polynomial-time or parameterized-FPT
// claim; a conservative bound is O(2^V * (E + V log V)) time and O(2^V + V) auxiliary state.
[[nodiscard]] inline DirectedFeedbackVertexSetResult
minimum_directed_feedback_vertex_set(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "minimum_directed_feedback_vertex_set requires a directed graph");
  }
  const std::size_t n = graph.vertex_count();
  if (n > kMaxExactFeedbackVertexSetVertices) {
    throw std::length_error(
        "minimum_directed_feedback_vertex_set exceeds bounded exact vertex limit");
  }

  using namespace feedback_vertex_set_detail;
  Mask forced_mask = 0U;
  std::vector<Vertex> forced;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (edge.to == vertex) {
        forced_mask |= (Mask{1} << vertex);
        forced.push_back(vertex);
        break;
      }
    }
  }

  Mask best_mask = (n == 0U) ? 0U : ((Mask{1} << n) - 1U);
  std::size_t best_size = n;
  bool have_best = false;
  const std::size_t state_count = std::size_t{1} << n;
  std::vector<unsigned char> visited(state_count, 0U);
  std::size_t explored = 0U;

  const auto search = [&](auto&& self, Mask mask) -> void {
    const std::size_t index = static_cast<std::size_t>(mask);
    if (visited[index] != 0U) {
      return;
    }
    visited[index] = 1U;
    ++explored;

    const std::size_t used = static_cast<std::size_t>(std::popcount(mask));
    if (have_best && used > best_size) {
      return;
    }

    const std::vector<Vertex> cycle = find_directed_cycle(graph, mask);
    if (cycle.empty()) {
      if (!have_best || used < best_size ||
          (used == best_size && lexicographically_better(mask, best_mask, n))) {
        have_best = true;
        best_size = used;
        best_mask = mask;
      }
      return;
    }
    if (have_best && used >= best_size) {
      return;
    }
    for (const Vertex vertex : cycle) {
      self(self, mask | (Mask{1} << vertex));
    }
  };

  search(search, forced_mask);
  if (!have_best) {
    throw std::logic_error("feedback-vertex-set exact search found no solution");
  }

  return DirectedFeedbackVertexSetResult{
      mask_vertices(best_mask, n), std::move(forced),
      topological_order(graph, best_mask), explored};
}

}  // namespace algorithms::graphs
