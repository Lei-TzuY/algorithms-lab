#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <functional>
#include <queue>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace detail {

inline std::vector<std::set<Vertex>> validate_prufer_tree(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("Pruefer encoding requires an undirected tree");
  }

  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count == 0U) {
    throw std::invalid_argument("Pruefer encoding requires at least one vertex");
  }

  std::vector<std::set<Vertex>> adjacency(vertex_count);
  std::size_t logical_edge_count = 0U;
  for (Vertex from = 0U; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex to = edge.to;
      if (from == to) {
        throw std::invalid_argument("Pruefer encoding rejects self-loops");
      }
      if (from < to) {
        if (!adjacency[from].insert(to).second) {
          throw std::invalid_argument("Pruefer encoding rejects parallel edges");
        }
        adjacency[to].insert(from);
        ++logical_edge_count;
      }
    }
  }

  if (logical_edge_count != vertex_count - 1U) {
    throw std::invalid_argument("Pruefer encoding requires exactly V-1 edges");
  }

  std::vector<bool> seen(vertex_count, false);
  std::queue<Vertex> pending;
  seen[0] = true;
  pending.push(0U);
  std::size_t reached = 0U;
  while (!pending.empty()) {
    const Vertex vertex = pending.front();
    pending.pop();
    ++reached;
    for (const Vertex next : adjacency[vertex]) {
      if (!seen[next]) {
        seen[next] = true;
        pending.push(next);
      }
    }
  }
  if (reached != vertex_count) {
    throw std::invalid_argument("Pruefer encoding requires a connected tree");
  }
  return adjacency;
}

}  // namespace detail

// Return the classical canonical Pruefer sequence obtained by repeatedly
// deleting the smallest labeled leaf. Vertex ids are the labels.
//
// Preconditions: graph is a non-empty simple undirected tree. Edge weights are
// intentionally ignored. The direct ordered-set / heap baseline is O(V log V)
// time and O(V) auxiliary state.
[[nodiscard]] inline std::vector<Vertex> prufer_encode(const Graph& graph) {
  std::vector<std::set<Vertex>> adjacency = detail::validate_prufer_tree(graph);
  const std::size_t vertex_count = graph.vertex_count();
  if (vertex_count <= 2U) {
    return {};
  }

  std::priority_queue<Vertex, std::vector<Vertex>, std::greater<Vertex>> leaves;
  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (adjacency[vertex].size() == 1U) {
      leaves.push(vertex);
    }
  }

  std::vector<Vertex> code;
  code.reserve(vertex_count - 2U);
  for (std::size_t step = 0U; step < vertex_count - 2U; ++step) {
    if (leaves.empty()) {
      throw std::logic_error("Pruefer leaf invariant failed");
    }
    const Vertex leaf = leaves.top();
    leaves.pop();
    if (adjacency[leaf].size() != 1U) {
      throw std::logic_error("Pruefer leaf degree invariant failed");
    }

    const Vertex neighbor = *adjacency[leaf].begin();
    code.push_back(neighbor);
    adjacency[leaf].clear();
    const std::size_t erased = adjacency[neighbor].erase(leaf);
    if (erased != 1U) {
      throw std::logic_error("Pruefer symmetric adjacency invariant failed");
    }
    if (adjacency[neighbor].size() == 1U) {
      leaves.push(neighbor);
    }
  }
  return code;
}

// Decode a Pruefer sequence on dense labels [0, vertex_count). For n >= 2,
// code.size() must equal n-2. A one-vertex tree uses the empty sequence; n=0
// is deliberately outside the classical labeled-tree contract.
//
// The returned graph is an unweighted undirected tree. Direct complexity is
// O(V log V) time and O(V) auxiliary state.
[[nodiscard]] inline Graph prufer_decode(
    std::size_t vertex_count, std::span<const Vertex> code) {
  if (vertex_count == 0U) {
    throw std::invalid_argument("Pruefer decoding requires at least one vertex");
  }
  const std::size_t expected_size = vertex_count > 2U ? vertex_count - 2U : 0U;
  if (code.size() != expected_size) {
    throw std::invalid_argument("Pruefer code has the wrong length");
  }

  Graph graph(vertex_count, false);
  if (vertex_count == 1U) {
    return graph;
  }

  std::vector<std::size_t> degree(vertex_count, 1U);
  for (const Vertex vertex : code) {
    if (vertex >= vertex_count) {
      throw std::out_of_range("Pruefer code vertex out of range");
    }
    ++degree[vertex];
  }

  std::priority_queue<Vertex, std::vector<Vertex>, std::greater<Vertex>> leaves;
  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (degree[vertex] == 1U) {
      leaves.push(vertex);
    }
  }

  for (const Vertex neighbor : code) {
    if (leaves.empty()) {
      throw std::logic_error("Pruefer decode leaf invariant failed");
    }
    const Vertex leaf = leaves.top();
    leaves.pop();
    graph.add_edge(leaf, neighbor);
    degree[leaf] = 0U;
    --degree[neighbor];
    if (degree[neighbor] == 1U) {
      leaves.push(neighbor);
    }
  }

  if (leaves.size() != 2U) {
    throw std::logic_error("Pruefer decode final-leaf invariant failed");
  }
  const Vertex first = leaves.top();
  leaves.pop();
  const Vertex second = leaves.top();
  leaves.pop();
  graph.add_edge(first, second);
  return graph;
}

}  // namespace algorithms::graphs
