#pragma once

#include "algorithms/graphs/biconnected_components.hpp"
#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

enum class OpenEarStatus {
  success,
  not_two_vertex_connected,
};

struct OpenEar {
  // The initial ear is a simple cycle and therefore repeats its first vertex at
  // the end. Every later ear is an open path whose endpoints were already
  // present and whose internal vertices are new.
  std::vector<Vertex> vertices;

  friend bool operator==(const OpenEar&, const OpenEar&) = default;
};

struct OpenEarDecompositionResult {
  OpenEarStatus status = OpenEarStatus::not_two_vertex_connected;
  std::vector<OpenEar> ears;
  std::optional<Vertex> blocking_articulation;
};

namespace open_ear_detail {

struct StructuralEdge {
  Vertex first;
  Vertex second;
};

struct AdjacentEdge {
  Vertex to;
  std::size_t edge_id;
};

struct Attachment {
  Vertex old_vertex;
  Vertex component_vertex;
  std::size_t edge_id;
};

inline std::vector<StructuralEdge> collect_simple_edges(const Graph& graph) {
  std::vector<StructuralEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        throw std::invalid_argument(
            "open ear decomposition requires a simple graph");
      }
      if (from < edge.to) {
        edges.push_back({from, edge.to});
      }
    }
  }
  std::sort(edges.begin(), edges.end(),
            [](const StructuralEdge& left, const StructuralEdge& right) {
              return std::pair{left.first, left.second} <
                     std::pair{right.first, right.second};
            });
  for (std::size_t index = 1; index < edges.size(); ++index) {
    if (edges[index - 1].first == edges[index].first &&
        edges[index - 1].second == edges[index].second) {
      throw std::invalid_argument(
          "open ear decomposition requires a simple graph");
    }
  }
  return edges;
}

inline std::vector<std::vector<AdjacentEdge>> build_adjacency(
    std::size_t vertex_count, const std::vector<StructuralEdge>& edges) {
  std::vector<std::vector<AdjacentEdge>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const StructuralEdge& edge = edges[edge_id];
    adjacency[edge.first].push_back({edge.second, edge_id});
    adjacency[edge.second].push_back({edge.first, edge_id});
  }
  for (auto& neighbors : adjacency) {
    std::sort(neighbors.begin(), neighbors.end(),
              [](const AdjacentEdge& left, const AdjacentEdge& right) {
                if (left.to != right.to) {
                  return left.to < right.to;
                }
                return left.edge_id < right.edge_id;
              });
  }
  return adjacency;
}

inline bool connected(const std::vector<std::vector<AdjacentEdge>>& adjacency) {
  if (adjacency.empty()) {
    return false;
  }
  std::vector<bool> seen(adjacency.size(), false);
  std::queue<Vertex> queue;
  seen[0] = true;
  queue.push(0);
  std::size_t reached = 0;
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    ++reached;
    for (const AdjacentEdge& edge : adjacency[vertex]) {
      if (!seen[edge.to]) {
        seen[edge.to] = true;
        queue.push(edge.to);
      }
    }
  }
  return reached == adjacency.size();
}

inline std::vector<Vertex> reconstruct_vertex_path(
    Vertex source, Vertex target, const std::vector<Vertex>& parent) {
  std::vector<Vertex> reverse_path;
  for (Vertex vertex = target;; vertex = parent[vertex]) {
    reverse_path.push_back(vertex);
    if (vertex == source) {
      break;
    }
  }
  std::reverse(reverse_path.begin(), reverse_path.end());
  return reverse_path;
}

inline std::vector<std::size_t> reconstruct_edge_path(
    Vertex source, Vertex target, const std::vector<Vertex>& parent,
    const std::vector<std::size_t>& parent_edge) {
  std::vector<std::size_t> reverse_edges;
  for (Vertex vertex = target; vertex != source; vertex = parent[vertex]) {
    reverse_edges.push_back(parent_edge[vertex]);
  }
  std::reverse(reverse_edges.begin(), reverse_edges.end());
  return reverse_edges;
}

inline std::pair<std::vector<Vertex>, std::vector<std::size_t>>
path_excluding_edge(
    Vertex source, Vertex target, std::size_t excluded_edge,
    const std::vector<std::vector<AdjacentEdge>>& adjacency) {
  const Vertex none_vertex = std::numeric_limits<Vertex>::max();
  const std::size_t none_edge = std::numeric_limits<std::size_t>::max();
  std::vector<Vertex> parent(adjacency.size(), none_vertex);
  std::vector<std::size_t> parent_edge(adjacency.size(), none_edge);
  std::queue<Vertex> queue;
  parent[source] = source;
  queue.push(source);
  while (!queue.empty() && parent[target] == none_vertex) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const AdjacentEdge& edge : adjacency[vertex]) {
      if (edge.edge_id == excluded_edge ||
          parent[edge.to] != none_vertex) {
        continue;
      }
      parent[edge.to] = vertex;
      parent_edge[edge.to] = edge.edge_id;
      queue.push(edge.to);
    }
  }
  if (parent[target] == none_vertex) {
    throw std::logic_error(
        "two-connected graph edge did not lie on a cycle");
  }
  return {reconstruct_vertex_path(source, target, parent),
          reconstruct_edge_path(source, target, parent, parent_edge)};
}

inline std::vector<Vertex> outside_component(
    Vertex seed, const std::vector<bool>& incorporated,
    const std::vector<std::vector<AdjacentEdge>>& adjacency) {
  std::vector<bool> seen(adjacency.size(), false);
  std::vector<Vertex> component;
  std::queue<Vertex> queue;
  seen[seed] = true;
  queue.push(seed);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    component.push_back(vertex);
    for (const AdjacentEdge& edge : adjacency[vertex]) {
      if (!incorporated[edge.to] && !seen[edge.to]) {
        seen[edge.to] = true;
        queue.push(edge.to);
      }
    }
  }
  std::sort(component.begin(), component.end());
  return component;
}

inline std::pair<std::vector<Vertex>, std::vector<std::size_t>> component_path(
    Vertex source, Vertex target, const std::vector<bool>& in_component,
    const std::vector<std::vector<AdjacentEdge>>& adjacency) {
  if (source == target) {
    return {{source}, {}};
  }
  const Vertex none_vertex = std::numeric_limits<Vertex>::max();
  const std::size_t none_edge = std::numeric_limits<std::size_t>::max();
  std::vector<Vertex> parent(adjacency.size(), none_vertex);
  std::vector<std::size_t> parent_edge(adjacency.size(), none_edge);
  std::queue<Vertex> queue;
  parent[source] = source;
  queue.push(source);
  while (!queue.empty() && parent[target] == none_vertex) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const AdjacentEdge& edge : adjacency[vertex]) {
      if (!in_component[edge.to] || parent[edge.to] != none_vertex) {
        continue;
      }
      parent[edge.to] = vertex;
      parent_edge[edge.to] = edge.edge_id;
      queue.push(edge.to);
    }
  }
  if (parent[target] == none_vertex) {
    throw std::logic_error(
        "outside component was not internally connected");
  }
  return {reconstruct_vertex_path(source, target, parent),
          reconstruct_edge_path(source, target, parent, parent_edge)};
}

}  // namespace open_ear_detail

// Construct an open ear decomposition of a simple, undirected,
// two-vertex-connected graph. Edge weights are ignored.
//
// Directed input, self-loops, and parallel edges are rejected because the
// returned witness is the classical simple-graph open-ear decomposition.
// Graphs with fewer than three vertices, disconnected graphs, or graphs with an
// articulation vertex return not_two_vertex_connected.
//
// Direct implementation complexity is O(V(V + E)) time and O(V + E) working
// storage. The vertex-biconnected gate reuses the repository's sealed
// decomposition; ear construction itself is independent and deterministic.
[[nodiscard]] inline OpenEarDecompositionResult open_ear_decomposition(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "open ear decomposition requires undirected input");
  }

  const std::vector<open_ear_detail::StructuralEdge> edges =
      open_ear_detail::collect_simple_edges(graph);
  const std::size_t vertex_count = graph.vertex_count();
  const auto adjacency =
      open_ear_detail::build_adjacency(vertex_count, edges);

  OpenEarDecompositionResult result;
  if (vertex_count < 3 || !open_ear_detail::connected(adjacency)) {
    return result;
  }

  const VertexBiconnectedDecomposition biconnected =
      vertex_biconnected_decomposition(graph);
  if (!biconnected.articulation_vertices.empty()) {
    result.blocking_articulation = biconnected.articulation_vertices.front();
    return result;
  }

  if (edges.empty()) {
    return result;
  }

  std::vector<bool> used_edge(edges.size(), false);
  std::vector<bool> incorporated(vertex_count, false);
  std::size_t incorporated_count = 0;

  const open_ear_detail::StructuralEdge first_edge = edges.front();
  auto [cycle_path, cycle_path_edges] = open_ear_detail::path_excluding_edge(
      first_edge.first, first_edge.second, 0, adjacency);
  cycle_path.push_back(first_edge.first);
  result.ears.push_back({cycle_path});
  used_edge[0] = true;
  for (std::size_t edge_id : cycle_path_edges) {
    used_edge[edge_id] = true;
  }
  for (std::size_t index = 0; index + 1 < cycle_path.size(); ++index) {
    const Vertex vertex = cycle_path[index];
    if (!incorporated[vertex]) {
      incorporated[vertex] = true;
      ++incorporated_count;
    }
  }

  while (incorporated_count < vertex_count) {
    Vertex seed = 0;
    while (seed < vertex_count && incorporated[seed]) {
      ++seed;
    }
    if (seed == vertex_count) {
      break;
    }

    const std::vector<Vertex> component =
        open_ear_detail::outside_component(seed, incorporated, adjacency);
    std::vector<bool> in_component(vertex_count, false);
    for (Vertex vertex : component) {
      in_component[vertex] = true;
    }

    std::vector<open_ear_detail::Attachment> attachments;
    for (Vertex vertex : component) {
      for (const open_ear_detail::AdjacentEdge& edge : adjacency[vertex]) {
        if (incorporated[edge.to]) {
          attachments.push_back({edge.to, vertex, edge.edge_id});
        }
      }
    }
    std::sort(
        attachments.begin(), attachments.end(),
        [](const open_ear_detail::Attachment& left,
           const open_ear_detail::Attachment& right) {
          if (left.old_vertex != right.old_vertex) {
            return left.old_vertex < right.old_vertex;
          }
          if (left.component_vertex != right.component_vertex) {
            return left.component_vertex < right.component_vertex;
          }
          return left.edge_id < right.edge_id;
        });

    std::optional<std::pair<open_ear_detail::Attachment,
                            open_ear_detail::Attachment>>
        selected;
    for (std::size_t first = 0;
         first < attachments.size() && !selected.has_value(); ++first) {
      for (std::size_t second = first + 1; second < attachments.size();
           ++second) {
        if (attachments[first].old_vertex != attachments[second].old_vertex) {
          selected = std::pair{attachments[first], attachments[second]};
          break;
        }
      }
    }
    if (!selected.has_value()) {
      throw std::logic_error(
          "two-connected outside component lacked two attachments");
    }

    const open_ear_detail::Attachment left = selected->first;
    const open_ear_detail::Attachment right = selected->second;
    auto [inside_vertices, inside_edges] = open_ear_detail::component_path(
        left.component_vertex, right.component_vertex, in_component, adjacency);

    std::vector<Vertex> ear_vertices;
    ear_vertices.reserve(inside_vertices.size() + 2);
    ear_vertices.push_back(left.old_vertex);
    ear_vertices.insert(ear_vertices.end(), inside_vertices.begin(),
                        inside_vertices.end());
    ear_vertices.push_back(right.old_vertex);
    result.ears.push_back({std::move(ear_vertices)});

    used_edge[left.edge_id] = true;
    used_edge[right.edge_id] = true;
    for (std::size_t edge_id : inside_edges) {
      used_edge[edge_id] = true;
    }
    for (Vertex vertex : inside_vertices) {
      if (!incorporated[vertex]) {
        incorporated[vertex] = true;
        ++incorporated_count;
      }
    }
  }

  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    if (!used_edge[edge_id]) {
      result.ears.push_back({{edges[edge_id].first, edges[edge_id].second}});
    }
  }

  result.status = OpenEarStatus::success;
  return result;
}

}  // namespace algorithms::graphs
