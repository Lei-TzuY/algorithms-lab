#pragma once

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct SuurballeEdgeWitness {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};

  friend bool operator==(const SuurballeEdgeWitness&,
                         const SuurballeEdgeWitness&) = default;
};

struct SuurballePath {
  Weight total_cost{};
  std::vector<Vertex> vertices;
  std::vector<SuurballeEdgeWitness> edges;

  friend bool operator==(const SuurballePath&, const SuurballePath&) = default;
};

struct SuurballeResult {
  Weight total_cost{};
  std::array<SuurballePath, 2> paths;

  friend bool operator==(const SuurballeResult&, const SuurballeResult&) = default;
};

namespace detail {

struct SuurballeWideUnsigned {
  std::uint64_t high{};
  std::uint64_t low{};

  friend bool operator==(const SuurballeWideUnsigned&,
                         const SuurballeWideUnsigned&) = default;
};

inline bool suurballe_wide_less(const SuurballeWideUnsigned& first,
                                const SuurballeWideUnsigned& second) noexcept {
  return first.high < second.high ||
         (first.high == second.high && first.low < second.low);
}

inline SuurballeWideUnsigned suurballe_wide_add(
    SuurballeWideUnsigned first, SuurballeWideUnsigned second) {
  const std::uint64_t old_low = first.low;
  first.low += second.low;
  const std::uint64_t carry = first.low < old_low ? 1U : 0U;
  const std::uint64_t old_high = first.high;
  first.high += second.high;
  if (first.high < old_high) {
    throw std::overflow_error("Suurballe internal distance exceeds 128 bits");
  }
  const std::uint64_t before_carry = first.high;
  first.high += carry;
  if (first.high < before_carry) {
    throw std::overflow_error("Suurballe internal distance exceeds 128 bits");
  }
  return first;
}

inline SuurballeWideUnsigned suurballe_wide_add_u64(
    SuurballeWideUnsigned first, std::uint64_t second) {
  return suurballe_wide_add(first, SuurballeWideUnsigned{0, second});
}

inline SuurballeWideUnsigned suurballe_wide_subtract(
    SuurballeWideUnsigned first, SuurballeWideUnsigned second) {
  if (suurballe_wide_less(first, second)) {
    throw std::logic_error("Suurballe internal negative reduced cost");
  }
  const std::uint64_t borrow = first.low < second.low ? 1U : 0U;
  SuurballeWideUnsigned result;
  result.low = first.low - second.low;
  result.high = first.high - second.high - borrow;
  return result;
}

inline Weight suurballe_narrow_nonnegative(SuurballeWideUnsigned value) {
  const auto max_public = static_cast<std::uint64_t>(
      std::numeric_limits<Weight>::max());
  if (value.high != 0 || value.low > max_public) {
    throw std::overflow_error("Suurballe optimum is outside int64_t range");
  }
  return static_cast<Weight>(value.low);
}

struct SuurballeOriginalEdge {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};
};

struct SuurballeQueueEntry {
  SuurballeWideUnsigned distance{};
  Vertex vertex{};
};

struct SuurballeQueueCompare {
  bool operator()(const SuurballeQueueEntry& first,
                  const SuurballeQueueEntry& second) const noexcept {
    if (suurballe_wide_less(first.distance, second.distance)) {
      return true;
    }
    if (suurballe_wide_less(second.distance, first.distance)) {
      return false;
    }
    return first.vertex < second.vertex;
  }
};

struct SuurballeResidualArc {
  Vertex to{};
  SuurballeWideUnsigned cost{};
  std::size_t original_edge_id{};
  bool reverses_first_path{};
};

struct SuurballeResidualPredecessor {
  Vertex from{};
  std::size_t adjacency_index{};
};

inline std::vector<std::size_t> suurballe_reconstruct_original_path(
    Vertex source, Vertex target,
    const std::vector<std::optional<std::size_t>>& predecessor,
    const std::vector<SuurballeOriginalEdge>& edges) {
  std::vector<std::size_t> reversed;
  Vertex current = target;
  while (current != source) {
    if (!predecessor[current].has_value()) {
      throw std::logic_error("Suurballe missing original predecessor");
    }
    const std::size_t edge_id = *predecessor[current];
    if (edge_id >= edges.size() || edges[edge_id].to != current) {
      throw std::logic_error("Suurballe malformed original predecessor");
    }
    reversed.push_back(edge_id);
    current = edges[edge_id].from;
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

inline std::vector<SuurballeResidualPredecessor>
 suurballe_reconstruct_residual_path(
    Vertex source, Vertex target,
    const std::vector<std::optional<SuurballeResidualPredecessor>>& predecessor,
    const std::vector<std::vector<SuurballeResidualArc>>& residual) {
  std::vector<SuurballeResidualPredecessor> reversed;
  Vertex current = target;
  while (current != source) {
    if (!predecessor[current].has_value()) {
      throw std::logic_error("Suurballe missing residual predecessor");
    }
    const auto ref = *predecessor[current];
    if (ref.from >= residual.size() ||
        ref.adjacency_index >= residual[ref.from].size() ||
        residual[ref.from][ref.adjacency_index].to != current) {
      throw std::logic_error("Suurballe malformed residual predecessor");
    }
    reversed.push_back(ref);
    current = ref.from;
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

inline SuurballePath suurballe_extract_flow_path(
    Vertex source, Vertex target,
    const std::vector<SuurballeOriginalEdge>& edges,
    const std::vector<std::vector<std::size_t>>& outgoing_edge_ids,
    std::vector<bool>& remaining) {
  const std::size_t vertex_count = outgoing_edge_ids.size();
  std::vector<bool> visited(vertex_count, false);
  std::vector<std::optional<std::size_t>> predecessor(vertex_count);
  std::queue<Vertex> queue;
  visited[source] = true;
  queue.push(source);

  while (!queue.empty() && !visited[target]) {
    const Vertex from = queue.front();
    queue.pop();
    for (const std::size_t edge_id : outgoing_edge_ids[from]) {
      if (!remaining[edge_id]) {
        continue;
      }
      const Vertex to = edges[edge_id].to;
      if (visited[to]) {
        continue;
      }
      visited[to] = true;
      predecessor[to] = edge_id;
      queue.push(to);
      if (to == target) {
        break;
      }
    }
  }

  if (!visited[target]) {
    throw std::logic_error("Suurballe flow decomposition lost an s-t path");
  }

  std::vector<std::size_t> path_ids;
  Vertex current = target;
  while (current != source) {
    const std::size_t edge_id = *predecessor[current];
    path_ids.push_back(edge_id);
    current = edges[edge_id].from;
  }
  std::reverse(path_ids.begin(), path_ids.end());

  SuurballePath path;
  path.vertices.push_back(source);
  SuurballeWideUnsigned cost{};
  for (const std::size_t edge_id : path_ids) {
    if (!remaining[edge_id]) {
      throw std::logic_error("Suurballe flow edge reused during decomposition");
    }
    remaining[edge_id] = false;
    const auto& edge = edges[edge_id];
    path.edges.push_back(SuurballeEdgeWitness{
        edge.from, edge.adjacency_index, edge.to, edge.weight});
    path.vertices.push_back(edge.to);
    cost = suurballe_wide_add_u64(
        cost, static_cast<std::uint64_t>(edge.weight));
  }
  path.total_cost = suurballe_narrow_nonnegative(cost);
  return path;
}

}  // namespace detail

// Returns the minimum-total-cost pair of edge-disjoint loopless source-target
// paths in a directed graph with globally non-negative edge weights.
//
// The implementation is the direct Suurballe construction:
//   1) one shortest path establishes vertex potentials,
//   2) its exact adjacency-edge copies are reversed at zero reduced cost,
//   3) a second shortest path is found in the reduced residual graph,
//   4) opposite uses cancel and the resulting integral two-unit flow is
//      decomposed into two original-edge-disjoint paths.
//
// Parallel adjacency entries are distinct edges. Self-loops are accepted but
// cannot appear in a returned loopless path. Undirected graphs and source==target
// are rejected because this slice deliberately models directed edge identity.
//
// Two Dijkstra passes use the repository BinaryHeap with stale entries, giving
// O((V+E) + E log(E+1)) time and O(V+E) auxiliary/result scale in the direct
// representation, excluding returned path storage.
inline std::optional<SuurballeResult>
suurballe_two_edge_disjoint_shortest_paths(const Graph& graph, Vertex source,
                                            Vertex target) {
  if (!graph.directed()) {
    throw std::invalid_argument("Suurballe requires a directed graph");
  }
  graph.validate_vertex(source);
  graph.validate_vertex(target);
  if (source == target) {
    throw std::invalid_argument("Suurballe requires distinct source and target");
  }

  const std::size_t vertex_count = graph.vertex_count();
  std::vector<detail::SuurballeOriginalEdge> edges;
  std::vector<std::vector<std::size_t>> outgoing_edge_ids(vertex_count);
  for (Vertex from = 0; from < vertex_count; ++from) {
    const auto& adjacency = graph.neighbors(from);
    outgoing_edge_ids[from].reserve(adjacency.size());
    for (std::size_t index = 0; index < adjacency.size(); ++index) {
      const auto& edge = adjacency[index];
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "Suurballe requires globally non-negative edge weights");
      }
      const std::size_t edge_id = edges.size();
      edges.push_back(detail::SuurballeOriginalEdge{
          from, index, edge.to, edge.weight});
      outgoing_edge_ids[from].push_back(edge_id);
    }
  }

  using detail::SuurballeQueueCompare;
  using detail::SuurballeQueueEntry;
  using detail::SuurballeWideUnsigned;
  algorithms::data_structures::BinaryHeap<SuurballeQueueEntry,
                                           SuurballeQueueCompare>
      first_queue;
  std::vector<std::optional<SuurballeWideUnsigned>> first_distance(vertex_count);
  std::vector<std::optional<std::size_t>> first_predecessor(vertex_count);
  first_distance[source] = SuurballeWideUnsigned{};
  first_queue.push(SuurballeQueueEntry{SuurballeWideUnsigned{}, source});

  while (!first_queue.empty()) {
    const SuurballeQueueEntry entry = first_queue.pop();
    if (!first_distance[entry.vertex].has_value() ||
        !(entry.distance == *first_distance[entry.vertex])) {
      continue;
    }
    for (const std::size_t edge_id : outgoing_edge_ids[entry.vertex]) {
      const auto& edge = edges[edge_id];
      const auto candidate = detail::suurballe_wide_add_u64(
          entry.distance, static_cast<std::uint64_t>(edge.weight));
      if (!first_distance[edge.to].has_value() ||
          detail::suurballe_wide_less(candidate, *first_distance[edge.to])) {
        first_distance[edge.to] = candidate;
        first_predecessor[edge.to] = edge_id;
        first_queue.push(SuurballeQueueEntry{candidate, edge.to});
      }
    }
  }

  if (!first_distance[target].has_value()) {
    return std::nullopt;
  }

  const auto first_path_ids = detail::suurballe_reconstruct_original_path(
      source, target, first_predecessor, edges);
  std::vector<bool> first_path_edge(edges.size(), false);
  for (const std::size_t edge_id : first_path_ids) {
    first_path_edge[edge_id] = true;
  }

  std::vector<std::vector<detail::SuurballeResidualArc>> residual(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const auto& edge = edges[edge_id];
    if (first_path_edge[edge_id]) {
      residual[edge.to].push_back(detail::SuurballeResidualArc{
          edge.from, SuurballeWideUnsigned{}, edge_id, true});
      continue;
    }
    if (!first_distance[edge.from].has_value()) {
      continue;
    }
    if (!first_distance[edge.to].has_value()) {
      throw std::logic_error(
          "Suurballe reachable edge ended at an unreachable vertex");
    }
    const auto shifted = detail::suurballe_wide_add_u64(
        *first_distance[edge.from], static_cast<std::uint64_t>(edge.weight));
    const auto reduced = detail::suurballe_wide_subtract(
        shifted, *first_distance[edge.to]);
    residual[edge.from].push_back(detail::SuurballeResidualArc{
        edge.to, reduced, edge_id, false});
  }

  algorithms::data_structures::BinaryHeap<SuurballeQueueEntry,
                                           SuurballeQueueCompare>
      second_queue;
  std::vector<std::optional<SuurballeWideUnsigned>> second_distance(vertex_count);
  std::vector<std::optional<detail::SuurballeResidualPredecessor>>
      second_predecessor(vertex_count);
  second_distance[source] = SuurballeWideUnsigned{};
  second_queue.push(SuurballeQueueEntry{SuurballeWideUnsigned{}, source});

  while (!second_queue.empty()) {
    const SuurballeQueueEntry entry = second_queue.pop();
    if (!second_distance[entry.vertex].has_value() ||
        !(entry.distance == *second_distance[entry.vertex])) {
      continue;
    }
    const auto& adjacency = residual[entry.vertex];
    for (std::size_t index = 0; index < adjacency.size(); ++index) {
      const auto& arc = adjacency[index];
      const auto candidate =
          detail::suurballe_wide_add(entry.distance, arc.cost);
      if (!second_distance[arc.to].has_value() ||
          detail::suurballe_wide_less(candidate, *second_distance[arc.to])) {
        second_distance[arc.to] = candidate;
        second_predecessor[arc.to] =
            detail::SuurballeResidualPredecessor{entry.vertex, index};
        second_queue.push(SuurballeQueueEntry{candidate, arc.to});
      }
    }
  }

  if (!second_distance[target].has_value()) {
    return std::nullopt;
  }

  const auto residual_path = detail::suurballe_reconstruct_residual_path(
      source, target, second_predecessor, residual);
  std::vector<bool> selected(edges.size(), false);
  for (const std::size_t edge_id : first_path_ids) {
    selected[edge_id] = true;
  }
  for (const auto& ref : residual_path) {
    const auto& arc = residual[ref.from][ref.adjacency_index];
    if (arc.reverses_first_path) {
      if (!selected[arc.original_edge_id]) {
        throw std::logic_error("Suurballe reverse arc cancelled absent edge");
      }
      selected[arc.original_edge_id] = false;
    } else {
      if (selected[arc.original_edge_id]) {
        throw std::logic_error("Suurballe forward residual edge already selected");
      }
      selected[arc.original_edge_id] = true;
    }
  }

  std::vector<bool> remaining = selected;
  SuurballeResult result;
  result.paths[0] = detail::suurballe_extract_flow_path(
      source, target, edges, outgoing_edge_ids, remaining);
  result.paths[1] = detail::suurballe_extract_flow_path(
      source, target, edges, outgoing_edge_ids, remaining);

  detail::SuurballeWideUnsigned total{};
  total = detail::suurballe_wide_add_u64(
      total, static_cast<std::uint64_t>(result.paths[0].total_cost));
  total = detail::suurballe_wide_add_u64(
      total, static_cast<std::uint64_t>(result.paths[1].total_cost));
  result.total_cost = detail::suurballe_narrow_nonnegative(total);
  return result;
}

}  // namespace algorithms::graphs
