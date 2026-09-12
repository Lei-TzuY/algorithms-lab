#pragma once

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct DirectedEdgeRef {
  Vertex from{};
  std::size_t adjacency_index{};
  friend bool operator==(const DirectedEdgeRef&, const DirectedEdgeRef&) = default;
};

struct LooplessPath {
  Weight total_weight{};
  std::vector<Vertex> vertices;
  std::vector<DirectedEdgeRef> edges;
  friend bool operator==(const LooplessPath&, const LooplessPath&) = default;
};

namespace yen_detail {

inline Weight checked_add(Weight a, Weight b) {
  constexpr Weight max = std::numeric_limits<Weight>::max();
  constexpr Weight min = std::numeric_limits<Weight>::min();
  if (b > 0 && a > max - b) throw std::overflow_error("k-shortest path cost overflow");
  if (b < 0 && a < min - b) throw std::overflow_error("k-shortest path cost underflow");
  return a + b;
}

inline bool edge_ref_less(const DirectedEdgeRef& a, const DirectedEdgeRef& b) noexcept {
  return a.from < b.from || (a.from == b.from && a.adjacency_index < b.adjacency_index);
}

inline bool edge_sequence_less(const std::vector<DirectedEdgeRef>& a,
                               const std::vector<DirectedEdgeRef>& b) noexcept {
  const std::size_t common = a.size() < b.size() ? a.size() : b.size();
  for (std::size_t i = 0; i < common; ++i) {
    if (a[i] == b[i]) continue;
    return edge_ref_less(a[i], b[i]);
  }
  return a.size() < b.size();
}

struct QueueEntry { Weight distance{}; Vertex vertex{}; };
struct QueueCompare {
  bool operator()(const QueueEntry& a, const QueueEntry& b) const noexcept {
    return a.distance < b.distance || (a.distance == b.distance && a.vertex < b.vertex);
  }
};

inline bool edge_is_blocked(const DirectedEdgeRef& ref,
                            const std::vector<DirectedEdgeRef>& blocked) {
  for (const auto& candidate : blocked) if (candidate == ref) return true;
  return false;
}

inline std::optional<LooplessPath> shortest_path_with_exclusions(
    const Graph& graph, Vertex source, Vertex target,
    const std::vector<bool>& blocked_vertices,
    const std::vector<DirectedEdgeRef>& blocked_edges) {
  if (blocked_vertices[source] || blocked_vertices[target]) return std::nullopt;
  if (source == target) return LooplessPath{Weight{0}, {source}, {}};

  std::vector<std::optional<Weight>> distance(graph.vertex_count());
  std::vector<std::optional<DirectedEdgeRef>> parent(graph.vertex_count());
  std::vector<bool> settled(graph.vertex_count(), false);
  algorithms::data_structures::BinaryHeap<QueueEntry, QueueCompare> queue;
  distance[source] = Weight{0};
  queue.push(QueueEntry{Weight{0}, source});

  while (!queue.empty()) {
    const QueueEntry current = queue.pop();
    if (!distance[current.vertex].has_value() ||
        current.distance != *distance[current.vertex] || settled[current.vertex]) continue;
    settled[current.vertex] = true;
    if (current.vertex == target) break;
    const auto& neighbors = graph.neighbors(current.vertex);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const Edge& edge = neighbors[index];
      if (blocked_vertices[edge.to] || settled[edge.to]) continue;
      const DirectedEdgeRef ref{current.vertex, index};
      if (edge_is_blocked(ref, blocked_edges)) continue;
      const Weight candidate = checked_add(current.distance, edge.weight);
      if (!distance[edge.to].has_value() || candidate < *distance[edge.to]) {
        distance[edge.to] = candidate;
        parent[edge.to] = ref;
        queue.push(QueueEntry{candidate, edge.to});
      }
    }
  }

  if (!distance[target].has_value()) return std::nullopt;
  std::vector<Vertex> reversed_vertices{target};
  std::vector<DirectedEdgeRef> reversed_edges;
  Vertex current = target;
  while (current != source) {
    if (!parent[current].has_value()) throw std::logic_error("k-shortest parent chain broken");
    const DirectedEdgeRef ref = *parent[current];
    if (ref.from >= graph.vertex_count() || ref.adjacency_index >= graph.neighbors(ref.from).size())
      throw std::logic_error("k-shortest parent edge invalid");
    reversed_edges.push_back(ref);
    current = ref.from;
    reversed_vertices.push_back(current);
    if (reversed_vertices.size() > graph.vertex_count())
      throw std::logic_error("k-shortest parent chain is cyclic");
  }
  std::vector<Vertex> vertices(reversed_vertices.rbegin(), reversed_vertices.rend());
  std::vector<DirectedEdgeRef> edges(reversed_edges.rbegin(), reversed_edges.rend());
  return LooplessPath{*distance[target], std::move(vertices), std::move(edges)};
}

inline bool same_prefix(const LooplessPath& path,
                        const std::vector<DirectedEdgeRef>& prefix) {
  if (path.edges.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i) if (!(path.edges[i] == prefix[i])) return false;
  return true;
}

inline bool contains_path(const std::vector<LooplessPath>& paths, const LooplessPath& needle) {
  for (const auto& path : paths) if (path.edges == needle.edges) return true;
  return false;
}

struct Candidate { LooplessPath path; };
struct CandidateCompare {
  bool operator()(const Candidate& a, const Candidate& b) const noexcept {
    if (a.path.total_weight != b.path.total_weight) return a.path.total_weight < b.path.total_weight;
    return edge_sequence_less(a.path.edges, b.path.edges);
  }
};

}  // namespace yen_detail

// Direct Yen-style enumeration of the k shortest loopless source-target paths in
// a directed non-negative weighted multigraph. Parallel adjacency entries are
// distinct edges; self-loops cannot appear in a loopless witness. Any negative
// edge anywhere in the graph is rejected, matching the repository Dijkstra
// contract. The returned paths have nondecreasing total weight; ties use a
// deterministic implementation order but are not claimed as a canonical global
// lexicographic order.
[[nodiscard]] inline std::vector<LooplessPath> yen_k_shortest_loopless_paths(
    const Graph& graph, Vertex source, Vertex target, std::size_t k) {
  using namespace yen_detail;
  graph.validate_vertex(source);
  graph.validate_vertex(target);
  if (!graph.directed()) throw std::invalid_argument("Yen k-shortest paths requires a directed graph");
  for (Vertex from = 0; from < graph.vertex_count(); ++from)
    for (const Edge& edge : graph.neighbors(from))
      if (edge.weight < 0) throw std::invalid_argument("Yen k-shortest paths requires non-negative weights");
  if (k == 0U) return {};

  const std::vector<bool> no_blocked_vertices(graph.vertex_count(), false);
  const std::vector<DirectedEdgeRef> no_blocked_edges;
  auto first = shortest_path_with_exclusions(graph, source, target, no_blocked_vertices, no_blocked_edges);
  if (!first.has_value()) return {};
  std::vector<LooplessPath> accepted;
  accepted.push_back(std::move(*first));
  if (source == target || k == 1U) return accepted;

  algorithms::data_structures::BinaryHeap<Candidate, CandidateCompare> candidates;
  std::vector<LooplessPath> queued;

  while (accepted.size() < k) {
    const LooplessPath& previous = accepted.back();
    Weight root_cost = 0;
    std::vector<DirectedEdgeRef> root_edges;
    root_edges.reserve(previous.edges.size());

    for (std::size_t spur_index = 0; spur_index < previous.edges.size(); ++spur_index) {
      const Vertex spur_vertex = previous.vertices[spur_index];
      std::vector<bool> blocked_vertices(graph.vertex_count(), false);
      for (std::size_t i = 0; i < spur_index; ++i) blocked_vertices[previous.vertices[i]] = true;

      std::vector<DirectedEdgeRef> blocked_edges;
      for (const LooplessPath& path : accepted) {
        if (path.edges.size() > spur_index && same_prefix(path, root_edges))
          blocked_edges.push_back(path.edges[spur_index]);
      }

      auto spur = shortest_path_with_exclusions(graph, spur_vertex, target,
                                                 blocked_vertices, blocked_edges);
      if (spur.has_value()) {
        LooplessPath combined;
        combined.total_weight = checked_add(root_cost, spur->total_weight);
        combined.vertices.reserve(spur_index + spur->vertices.size());
        combined.vertices.insert(combined.vertices.end(), previous.vertices.begin(),
                                 previous.vertices.begin() + static_cast<std::ptrdiff_t>(spur_index));
        combined.vertices.insert(combined.vertices.end(), spur->vertices.begin(), spur->vertices.end());
        combined.edges.reserve(root_edges.size() + spur->edges.size());
        combined.edges.insert(combined.edges.end(), root_edges.begin(), root_edges.end());
        combined.edges.insert(combined.edges.end(), spur->edges.begin(), spur->edges.end());
        if (!contains_path(accepted, combined) && !contains_path(queued, combined)) {
          candidates.push(Candidate{combined});
          queued.push_back(std::move(combined));
        }
      }

      const DirectedEdgeRef root_edge = previous.edges[spur_index];
      const Edge& edge = graph.neighbors(root_edge.from)[root_edge.adjacency_index];
      root_cost = checked_add(root_cost, edge.weight);
      root_edges.push_back(root_edge);
    }

    if (candidates.empty()) break;
    Candidate best = candidates.pop();
    for (auto it = queued.begin(); it != queued.end(); ++it) {
      if (it->edges == best.path.edges) { queued.erase(it); break; }
    }
    accepted.push_back(std::move(best.path));
  }

  return accepted;
}

}  // namespace algorithms::graphs
