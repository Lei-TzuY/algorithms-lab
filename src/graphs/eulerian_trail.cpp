#include "algorithms/graphs/eulerian_trail.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace algorithms::graphs {
namespace {

struct LogicalEdge {
  Vertex from;
  Vertex to;
};

struct TrailAdjacencyEntry {
  std::size_t edge_id;
  Vertex to;
};

struct NormalizedEulerianGraph {
  std::vector<LogicalEdge> edges;
  std::vector<std::vector<TrailAdjacencyEntry>> adjacency;
  std::vector<std::vector<Vertex>> weak_adjacency;
  std::vector<std::size_t> indegree;
  std::vector<std::size_t> outdegree;
  std::vector<std::size_t> undirected_degree;
  std::vector<bool> active;
};

NormalizedEulerianGraph normalize_edges(const Graph& graph) {
  NormalizedEulerianGraph normalized;
  const std::size_t vertex_count = graph.vertex_count();
  normalized.adjacency.resize(vertex_count);
  normalized.weak_adjacency.resize(vertex_count);
  normalized.indegree.assign(vertex_count, 0);
  normalized.outdegree.assign(vertex_count, 0);
  normalized.undirected_degree.assign(vertex_count, 0);
  normalized.active.assign(vertex_count, false);

  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (!graph.directed() && from > edge.to) {
        continue;
      }

      const std::size_t edge_id = normalized.edges.size();
      normalized.edges.push_back(LogicalEdge{from, edge.to});
      normalized.active[from] = true;
      normalized.active[edge.to] = true;

      normalized.adjacency[from].push_back(
          TrailAdjacencyEntry{edge_id, edge.to});

      if (graph.directed()) {
        ++normalized.outdegree[from];
        ++normalized.indegree[edge.to];
      } else {
        if (from == edge.to) {
          normalized.undirected_degree[from] += 2;
        } else {
          ++normalized.undirected_degree[from];
          ++normalized.undirected_degree[edge.to];
          normalized.adjacency[edge.to].push_back(
              TrailAdjacencyEntry{edge_id, from});
        }
      }

      if (from != edge.to) {
        normalized.weak_adjacency[from].push_back(edge.to);
        normalized.weak_adjacency[edge.to].push_back(from);
      }
    }
  }

  return normalized;
}

bool active_vertices_are_weakly_connected(
    const NormalizedEulerianGraph& graph) {
  std::optional<Vertex> start;
  for (Vertex vertex = 0; vertex < graph.active.size(); ++vertex) {
    if (graph.active[vertex]) {
      start = vertex;
      break;
    }
  }
  if (!start.has_value()) {
    return true;
  }

  std::vector<bool> visited(graph.active.size(), false);
  std::deque<Vertex> queue;
  visited[*start] = true;
  queue.push_back(*start);

  while (!queue.empty()) {
    const Vertex current = queue.front();
    queue.pop_front();
    for (const Vertex next : graph.weak_adjacency[current]) {
      if (!visited[next]) {
        visited[next] = true;
        queue.push_back(next);
      }
    }
  }

  for (Vertex vertex = 0; vertex < graph.active.size(); ++vertex) {
    if (graph.active[vertex] && !visited[vertex]) {
      return false;
    }
  }
  return true;
}

std::optional<Vertex> directed_start(const NormalizedEulerianGraph& graph) {
  std::optional<Vertex> positive_imbalance;
  std::optional<Vertex> negative_imbalance;

  for (Vertex vertex = 0; vertex < graph.indegree.size(); ++vertex) {
    const std::size_t in = graph.indegree[vertex];
    const std::size_t out = graph.outdegree[vertex];
    if (out > in) {
      if (out - in != 1 || positive_imbalance.has_value()) {
        return std::nullopt;
      }
      positive_imbalance = vertex;
    } else if (in > out) {
      if (in - out != 1 || negative_imbalance.has_value()) {
        return std::nullopt;
      }
      negative_imbalance = vertex;
    }
  }

  if (positive_imbalance.has_value() != negative_imbalance.has_value()) {
    return std::nullopt;
  }
  if (positive_imbalance.has_value()) {
    return positive_imbalance;
  }

  for (Vertex vertex = 0; vertex < graph.outdegree.size(); ++vertex) {
    if (graph.outdegree[vertex] != 0) {
      return vertex;
    }
  }
  return Vertex{0};
}

std::optional<Vertex> undirected_start(const NormalizedEulerianGraph& graph) {
  std::vector<Vertex> odd_vertices;
  for (Vertex vertex = 0; vertex < graph.undirected_degree.size(); ++vertex) {
    if (graph.undirected_degree[vertex] % 2 != 0) {
      odd_vertices.push_back(vertex);
    }
  }
  if (!(odd_vertices.empty() || odd_vertices.size() == 2)) {
    return std::nullopt;
  }
  if (odd_vertices.size() == 2) {
    return odd_vertices.front();
  }
  for (Vertex vertex = 0; vertex < graph.undirected_degree.size(); ++vertex) {
    if (graph.undirected_degree[vertex] != 0) {
      return vertex;
    }
  }
  return Vertex{0};
}

}  // namespace

std::optional<EulerianTrailResult> eulerian_trail(const Graph& graph) {
  if (graph.vertex_count() == 0) {
    return EulerianTrailResult{{}, 0, true};
  }

  const NormalizedEulerianGraph normalized = normalize_edges(graph);
  if (normalized.edges.empty()) {
    return EulerianTrailResult{{0}, 0, true};
  }
  if (!active_vertices_are_weakly_connected(normalized)) {
    return std::nullopt;
  }

  const std::optional<Vertex> start =
      graph.directed() ? directed_start(normalized) : undirected_start(normalized);
  if (!start.has_value()) {
    return std::nullopt;
  }

  std::vector<std::size_t> cursor(graph.vertex_count(), 0);
  std::vector<bool> used(normalized.edges.size(), false);
  std::vector<Vertex> stack{*start};
  std::vector<Vertex> reversed_vertices;
  reversed_vertices.reserve(normalized.edges.size() + 1);
  std::size_t used_edges = 0;

  while (!stack.empty()) {
    const Vertex current = stack.back();
    const auto& incident = normalized.adjacency[current];
    while (cursor[current] < incident.size() &&
           used[incident[cursor[current]].edge_id]) {
      ++cursor[current];
    }

    if (cursor[current] == incident.size()) {
      reversed_vertices.push_back(current);
      stack.pop_back();
      continue;
    }

    const TrailAdjacencyEntry entry = incident[cursor[current]];
    ++cursor[current];
    if (used[entry.edge_id]) {
      continue;
    }
    used[entry.edge_id] = true;
    ++used_edges;
    stack.push_back(entry.to);
  }

  if (used_edges != normalized.edges.size() ||
      reversed_vertices.size() != normalized.edges.size() + 1) {
    return std::nullopt;
  }

  std::reverse(reversed_vertices.begin(), reversed_vertices.end());
  const bool is_circuit =
      reversed_vertices.front() == reversed_vertices.back();
  return EulerianTrailResult{std::move(reversed_vertices),
                             normalized.edges.size(), is_circuit};
}

}  // namespace algorithms::graphs
