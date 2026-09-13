#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

enum class StrongOrientationStatus {
  success,
  disconnected,
  bridge,
};

struct StrongOrientationEdge {
  std::size_t logical_edge_index;
  Vertex undirected_first;
  Vertex undirected_second;
  Weight weight;
  Vertex from;
  Vertex to;

  friend bool operator==(const StrongOrientationEdge&,
                         const StrongOrientationEdge&) = default;
};

struct StrongOrientationBridgeWitness {
  std::size_t logical_edge_index;
  Vertex first;
  Vertex second;
  Weight weight;

  friend bool operator==(const StrongOrientationBridgeWitness&,
                         const StrongOrientationBridgeWitness&) = default;
};

struct StrongOrientationResult {
  StrongOrientationStatus status{StrongOrientationStatus::success};
  std::vector<StrongOrientationEdge> edges;
  std::optional<Graph> directed_graph;
  std::optional<StrongOrientationBridgeWitness> blocking_bridge;

  [[nodiscard]] bool orientable() const noexcept {
    return status == StrongOrientationStatus::success;
  }
};

namespace strong_orientation_detail {

struct LogicalEdge {
  Vertex first;
  Vertex second;
  Weight weight;
};

[[nodiscard]] inline std::vector<LogicalEdge> collect_logical_edges(
    const Graph& graph) {
  std::vector<LogicalEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to < from) {
        continue;
      }
      edges.push_back(LogicalEdge{from, edge.to, edge.weight});
    }
  }
  return edges;
}

}  // namespace strong_orientation_detail

// Robbins strong-orientation construction for the repository's undirected
// multigraph model.
//
// Contract:
// - directed input is rejected with invalid_argument;
// - empty and singleton graphs are orientable;
// - disconnected graphs return status=disconnected;
// - connected graphs with a bridge return status=bridge plus one deterministic
//   blocking bridge witness;
// - otherwise every logical edge is oriented exactly once and directed_graph is
//   strongly connected. Edge weights are preserved but do not affect topology.
//
// Deterministic logical-edge order scans vertices increasingly and keeps each
// self-loop plus each non-loop adjacency copy only at its smaller endpoint.
// Parallel edges therefore remain distinct logical edges.
//
// DFS proof obligation: tree edges are oriented parent->child and every non-tree
// edge is oriented from a descendant toward an already discovered ancestor. If
// no tree edge is a bridge, every DFS subtree has a directed route back to an
// ancestor; the root reaches all vertices through tree edges, and every vertex
// reaches the root through back-edge escapes. Robbins' theorem then gives a
// strongly connected orientation exactly for connected bridgeless graphs.
//
// Time: O(V + E). Auxiliary storage: O(V + E). The DFS is iterative, so this
// implementation does not consume recursion-stack space on deep graphs.
[[nodiscard]] inline StrongOrientationResult strong_orientation(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "strong_orientation requires an undirected graph");
  }

  using strong_orientation_detail::LogicalEdge;
  const std::size_t vertex_count = graph.vertex_count();
  const std::vector<LogicalEdge> logical_edges =
      strong_orientation_detail::collect_logical_edges(graph);

  std::vector<std::optional<std::pair<Vertex, Vertex>>> orientation(
      logical_edges.size());
  std::vector<std::vector<std::size_t>> incident(vertex_count);
  for (std::size_t edge_index = 0; edge_index < logical_edges.size();
       ++edge_index) {
    const LogicalEdge& edge = logical_edges[edge_index];
    if (edge.first == edge.second) {
      orientation[edge_index] = std::pair<Vertex, Vertex>{edge.first,
                                                          edge.second};
      continue;
    }
    incident[edge.first].push_back(edge_index);
    incident[edge.second].push_back(edge_index);
  }

  if (vertex_count == 0U) {
    return StrongOrientationResult{StrongOrientationStatus::success,
                                   {},
                                   Graph(0U, true),
                                   std::nullopt};
  }

  constexpr std::size_t kUnset = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> discovery(vertex_count, kUnset);
  std::vector<std::size_t> low(vertex_count, kUnset);
  std::vector<std::size_t> parent_edge(vertex_count, kUnset);
  std::vector<Vertex> parent(vertex_count, 0U);
  std::vector<std::size_t> next_incident(vertex_count, 0U);
  std::vector<Vertex> stack;
  std::vector<std::size_t> bridges;
  stack.reserve(vertex_count);
  bridges.reserve(vertex_count);

  std::size_t timer = 0U;
  discovery[0] = timer;
  low[0] = timer;
  ++timer;
  stack.push_back(0U);

  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    if (next_incident[vertex] < incident[vertex].size()) {
      const std::size_t edge_index =
          incident[vertex][next_incident[vertex]++];
      if (edge_index == parent_edge[vertex]) {
        continue;
      }

      const LogicalEdge& edge = logical_edges[edge_index];
      const Vertex other = edge.first == vertex ? edge.second : edge.first;
      if (discovery[other] == kUnset) {
        orientation[edge_index] =
            std::pair<Vertex, Vertex>{vertex, other};
        parent_edge[other] = edge_index;
        parent[other] = vertex;
        discovery[other] = timer;
        low[other] = timer;
        ++timer;
        stack.push_back(other);
        continue;
      }

      if (!orientation[edge_index].has_value()) {
        orientation[edge_index] =
            std::pair<Vertex, Vertex>{vertex, other};
      }
      low[vertex] = std::min(low[vertex], discovery[other]);
      continue;
    }

    stack.pop_back();
    if (parent_edge[vertex] == kUnset) {
      continue;
    }

    const Vertex ancestor = parent[vertex];
    low[ancestor] = std::min(low[ancestor], low[vertex]);
    if (low[vertex] > discovery[ancestor]) {
      bridges.push_back(parent_edge[vertex]);
    }
  }

  if (timer != vertex_count) {
    return StrongOrientationResult{StrongOrientationStatus::disconnected,
                                   {},
                                   std::nullopt,
                                   std::nullopt};
  }

  if (!bridges.empty()) {
    const std::size_t edge_index =
        *std::min_element(bridges.begin(), bridges.end());
    const LogicalEdge& edge = logical_edges[edge_index];
    return StrongOrientationResult{
        StrongOrientationStatus::bridge,
        {},
        std::nullopt,
        StrongOrientationBridgeWitness{edge_index, edge.first, edge.second,
                                       edge.weight}};
  }

  Graph directed(vertex_count, true);
  std::vector<StrongOrientationEdge> result_edges;
  result_edges.reserve(logical_edges.size());
  for (std::size_t edge_index = 0; edge_index < logical_edges.size();
       ++edge_index) {
    if (!orientation[edge_index].has_value()) {
      throw std::logic_error("strong-orientation edge was not oriented");
    }
    const LogicalEdge& edge = logical_edges[edge_index];
    const auto [from, to] = *orientation[edge_index];
    directed.add_edge(from, to, edge.weight);
    result_edges.push_back(StrongOrientationEdge{
        edge_index, edge.first, edge.second, edge.weight, from, to});
  }

  return StrongOrientationResult{StrongOrientationStatus::success,
                                 std::move(result_edges),
                                 std::move(directed),
                                 std::nullopt};
}

}  // namespace algorithms::graphs
