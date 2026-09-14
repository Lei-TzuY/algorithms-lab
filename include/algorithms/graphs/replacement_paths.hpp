#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct ReplacementLogicalEdgeWitness {
  Vertex first = 0;
  Vertex second = 0;
  std::size_t canonical_adjacency_index = 0;
  Weight weight = 0;

  friend bool operator==(const ReplacementLogicalEdgeWitness&,
                         const ReplacementLogicalEdgeWitness&) = default;
};

struct ReplacementFailureResult {
  ReplacementLogicalEdgeWitness failed_edge;
  std::optional<Weight> distance;
  std::optional<ReplacementLogicalEdgeWitness> detour_edge;
  std::optional<Vertex> detour_from;
  std::optional<Vertex> detour_to;

  friend bool operator==(const ReplacementFailureResult&,
                         const ReplacementFailureResult&) = default;
};

struct EdgeReplacementPathsResult {
  Weight shortest_distance = 0;
  std::vector<Vertex> shortest_path_vertices;
  std::vector<ReplacementLogicalEdgeWitness> shortest_path_edges;
  std::vector<ReplacementFailureResult> replacements;

  friend bool operator==(const EdgeReplacementPathsResult&,
                         const EdgeReplacementPathsResult&) = default;
};

namespace replacement_paths_detail {

constexpr std::uint64_t kOverflowDistance =
    static_cast<std::uint64_t>(std::numeric_limits<Weight>::max()) + 1ULL;
constexpr std::uint64_t kUnreachableDistance =
    std::numeric_limits<std::uint64_t>::max();
constexpr std::size_t kNoLabel = std::numeric_limits<std::size_t>::max();

struct LogicalEdge {
  Vertex first;
  Vertex second;
  Weight weight;
  ReplacementLogicalEdgeWitness witness;
};

struct LogicalGraph {
  std::vector<LogicalEdge> edges;
  std::vector<std::vector<std::size_t>> incident;
};

[[nodiscard]] inline LogicalGraph make_logical_graph(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "edge replacement paths require an undirected graph");
  }

  LogicalGraph logical;
  logical.incident.resize(graph.vertex_count());
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& edges = graph.neighbors(from);
    for (std::size_t adjacency_index = 0; adjacency_index < edges.size();
         ++adjacency_index) {
      const Edge& edge = edges[adjacency_index];
      if (edge.weight <= 0) {
        throw std::invalid_argument(
            "edge replacement paths require strictly positive edge weights");
      }
      if (edge.to < from) {
        continue;
      }
      const std::size_t id = logical.edges.size();
      logical.edges.push_back(LogicalEdge{
          from, edge.to, edge.weight,
          ReplacementLogicalEdgeWitness{from, edge.to, adjacency_index,
                                        edge.weight}});
      logical.incident[from].push_back(id);
      if (edge.to != from) {
        logical.incident[edge.to].push_back(id);
      }
    }
  }
  return logical;
}

[[nodiscard]] inline Vertex other_endpoint(const LogicalEdge& edge,
                                           Vertex vertex) {
  if (edge.first == vertex) {
    return edge.second;
  }
  if (edge.second == vertex) {
    return edge.first;
  }
  throw std::logic_error("logical edge does not contain requested endpoint");
}

[[nodiscard]] inline std::uint64_t capped_add(std::uint64_t first,
                                              Weight second) {
  if (first == kUnreachableDistance) {
    return kUnreachableDistance;
  }
  if (first >= kOverflowDistance) {
    return kOverflowDistance;
  }
  const auto addend = static_cast<std::uint64_t>(second);
  if (addend > kOverflowDistance - first) {
    return kOverflowDistance;
  }
  return first + addend;
}

[[nodiscard]] inline std::uint64_t capped_add(std::uint64_t first,
                                              std::uint64_t second) {
  if (first == kUnreachableDistance || second == kUnreachableDistance) {
    return kUnreachableDistance;
  }
  if (first >= kOverflowDistance || second >= kOverflowDistance ||
      second > kOverflowDistance - first) {
    return kOverflowDistance;
  }
  return first + second;
}

struct DijkstraEntry {
  std::uint64_t distance;
  Vertex vertex;
};

struct DijkstraEntryCompare {
  bool operator()(const DijkstraEntry& first,
                  const DijkstraEntry& second) const noexcept {
    if (first.distance != second.distance) {
      return first.distance < second.distance;
    }
    return first.vertex < second.vertex;
  }
};

struct DijkstraTree {
  std::vector<std::uint64_t> distance;
  std::vector<std::optional<std::size_t>> parent_edge;
};

[[nodiscard]] inline DijkstraTree capped_dijkstra(const LogicalGraph& graph,
                                                   Vertex source) {
  DijkstraTree result;
  result.distance.assign(graph.incident.size(), kUnreachableDistance);
  result.parent_edge.resize(graph.incident.size());
  result.distance[source] = 0;

  data_structures::BinaryHeap<DijkstraEntry, DijkstraEntryCompare> queue;
  queue.push(DijkstraEntry{0, source});
  while (!queue.empty()) {
    const DijkstraEntry current = queue.pop();
    if (current.distance != result.distance[current.vertex]) {
      continue;
    }
    for (const std::size_t edge_id : graph.incident[current.vertex]) {
      const LogicalEdge& edge = graph.edges[edge_id];
      const Vertex next = other_endpoint(edge, current.vertex);
      const std::uint64_t candidate = capped_add(current.distance, edge.weight);
      if (candidate < result.distance[next]) {
        result.distance[next] = candidate;
        result.parent_edge[next] = edge_id;
        queue.push(DijkstraEntry{candidate, next});
      }
    }
  }
  return result;
}

struct DetourCandidate {
  std::uint64_t distance;
  std::size_t end;
  std::size_t edge_id;
  Vertex from;
  Vertex to;
};

struct DetourCandidateCompare {
  bool operator()(const DetourCandidate& first,
                  const DetourCandidate& second) const noexcept {
    if (first.distance != second.distance) {
      return first.distance < second.distance;
    }
    if (first.edge_id != second.edge_id) {
      return first.edge_id < second.edge_id;
    }
    if (first.from != second.from) {
      return first.from < second.from;
    }
    if (first.to != second.to) {
      return first.to < second.to;
    }
    return first.end < second.end;
  }
};

}  // namespace replacement_paths_detail

// Computes edge-failure replacement distances for one deterministic shortest
// source-target path in an undirected graph with strictly positive weights.
//
// Production does not rerun Dijkstra once per failed path edge.  A shortest-path
// tree rooted at source is cut by the chosen source-target tree path.  Every
// non-path logical edge joining tree components i<j is a detour candidate for
// exactly the path-edge interval [i,j).  Its reduced replacement cost is
// d(source,u)+w(u,v)+d(v,target).  A sweep-line heap computes the minimum active
// candidate for every path edge.
//
// Returns nullopt if target is unreachable.  Throws overflow_error if the base
// shortest distance or any finite replacement distance is not representable in
// Weight.  Parallel copies remain distinct through canonical adjacency-index
// witnesses.  Self-loops cannot enter a shortest path or cross a tree cut.
[[nodiscard]] inline std::optional<EdgeReplacementPathsResult>
undirected_edge_replacement_paths(const Graph& graph, Vertex source,
                                  Vertex target) {
  graph.validate_vertex(source);
  graph.validate_vertex(target);
  const auto logical = replacement_paths_detail::make_logical_graph(graph);
  const auto from_source =
      replacement_paths_detail::capped_dijkstra(logical, source);

  const std::uint64_t baseline = from_source.distance[target];
  if (baseline == replacement_paths_detail::kUnreachableDistance) {
    return std::nullopt;
  }
  if (baseline >= replacement_paths_detail::kOverflowDistance) {
    throw std::overflow_error("shortest source-target distance exceeds int64_t");
  }

  EdgeReplacementPathsResult result;
  result.shortest_distance = static_cast<Weight>(baseline);
  if (source == target) {
    result.shortest_path_vertices.push_back(source);
    return result;
  }

  std::vector<Vertex> reversed_vertices;
  std::vector<std::size_t> reversed_edges;
  Vertex current = target;
  reversed_vertices.push_back(current);
  while (current != source) {
    if (!from_source.parent_edge[current].has_value()) {
      throw std::logic_error("reachable target is missing a shortest-tree parent");
    }
    const std::size_t edge_id = *from_source.parent_edge[current];
    reversed_edges.push_back(edge_id);
    current = replacement_paths_detail::other_endpoint(logical.edges[edge_id],
                                                       current);
    reversed_vertices.push_back(current);
  }
  result.shortest_path_vertices.assign(reversed_vertices.rbegin(),
                                       reversed_vertices.rend());
  std::vector<std::size_t> path_edge_ids(reversed_edges.rbegin(),
                                         reversed_edges.rend());
  result.shortest_path_edges.reserve(path_edge_ids.size());
  for (const std::size_t edge_id : path_edge_ids) {
    result.shortest_path_edges.push_back(logical.edges[edge_id].witness);
  }

  const std::size_t path_edge_count = path_edge_ids.size();
  std::vector<bool> is_path_edge(logical.edges.size(), false);
  for (const std::size_t edge_id : path_edge_ids) {
    is_path_edge[edge_id] = true;
  }

  std::vector<std::vector<std::pair<Vertex, std::size_t>>> tree_children(
      graph.vertex_count());
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (vertex == source || !from_source.parent_edge[vertex].has_value()) {
      continue;
    }
    const std::size_t edge_id = *from_source.parent_edge[vertex];
    const Vertex parent = replacement_paths_detail::other_endpoint(
        logical.edges[edge_id], vertex);
    tree_children[parent].push_back({vertex, edge_id});
  }

  std::vector<std::size_t> component(graph.vertex_count(),
                                     replacement_paths_detail::kNoLabel);
  for (std::size_t index = 0; index < result.shortest_path_vertices.size();
       ++index) {
    const Vertex root = result.shortest_path_vertices[index];
    std::vector<Vertex> stack{root};
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      if (component[vertex] != replacement_paths_detail::kNoLabel) {
        continue;
      }
      component[vertex] = index;
      for (const auto& [child, edge_id] : tree_children[vertex]) {
        if (!is_path_edge[edge_id]) {
          stack.push_back(child);
        }
      }
    }
  }

  const auto to_target =
      replacement_paths_detail::capped_dijkstra(logical, target);
  std::vector<std::vector<replacement_paths_detail::DetourCandidate>> starts(
      path_edge_count);
  for (std::size_t edge_id = 0; edge_id < logical.edges.size(); ++edge_id) {
    if (is_path_edge[edge_id]) {
      continue;
    }
    const auto& edge = logical.edges[edge_id];
    const std::size_t first_component = component[edge.first];
    const std::size_t second_component = component[edge.second];
    if (first_component == replacement_paths_detail::kNoLabel ||
        second_component == replacement_paths_detail::kNoLabel ||
        first_component == second_component) {
      continue;
    }

    Vertex from = edge.first;
    Vertex to = edge.second;
    std::size_t begin = first_component;
    std::size_t end = second_component;
    if (begin > end) {
      std::swap(begin, end);
      std::swap(from, to);
    }

    std::uint64_t distance = replacement_paths_detail::capped_add(
        from_source.distance[from], edge.weight);
    distance = replacement_paths_detail::capped_add(
        distance, to_target.distance[to]);
    if (distance == replacement_paths_detail::kUnreachableDistance) {
      continue;
    }
    starts[begin].push_back(
        replacement_paths_detail::DetourCandidate{distance, end, edge_id,
                                                   from, to});
  }

  data_structures::BinaryHeap<replacement_paths_detail::DetourCandidate,
                              replacement_paths_detail::DetourCandidateCompare>
      active;
  result.replacements.reserve(path_edge_count);
  for (std::size_t index = 0; index < path_edge_count; ++index) {
    for (const auto& candidate : starts[index]) {
      active.push(candidate);
    }
    while (!active.empty() && active.top().end <= index) {
      static_cast<void>(active.pop());
    }

    ReplacementFailureResult replacement;
    replacement.failed_edge = result.shortest_path_edges[index];
    if (!active.empty()) {
      const auto& best = active.top();
      if (best.distance >= replacement_paths_detail::kOverflowDistance) {
        throw std::overflow_error("finite replacement distance exceeds int64_t");
      }
      if (best.distance < baseline) {
        throw std::logic_error("replacement distance is below base shortest path");
      }
      replacement.distance = static_cast<Weight>(best.distance);
      replacement.detour_edge = logical.edges[best.edge_id].witness;
      replacement.detour_from = best.from;
      replacement.detour_to = best.to;
    }
    result.replacements.push_back(replacement);
  }

  return result;
}

}  // namespace algorithms::graphs
