#include "algorithms/graphs/chinese_postman.hpp"

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

using Cost = std::uint64_t;
constexpr Cost kTooLarge =
    static_cast<Cost>(std::numeric_limits<Weight>::max()) + Cost{1};
constexpr Cost kUnreachable = std::numeric_limits<Cost>::max();
constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

struct LogicalEdge {
  Vertex from = 0;
  Vertex to = 0;
  Weight weight = 0;
  std::size_t adjacency_index = 0;
};

struct Arc {
  Vertex to = 0;
  std::size_t edge_id = 0;
};

struct QueueEntry {
  Cost distance = 0;
  Vertex vertex = 0;
};

struct QueueEntryCompare {
  [[nodiscard]] bool operator()(const QueueEntry& first,
                                const QueueEntry& second) const noexcept {
    if (first.distance != second.distance) {
      return first.distance < second.distance;
    }
    return first.vertex < second.vertex;
  }
};

struct ShortestPaths {
  std::vector<Cost> distance;
  std::vector<std::optional<Vertex>> parent_vertex;
  std::vector<std::optional<std::size_t>> parent_edge;
};

struct Occurrence {
  std::size_t edge_id = 0;
  bool duplicated = false;
};

[[nodiscard]] Cost saturated_add(Cost first, Cost second) noexcept {
  if (first >= kTooLarge || second >= kTooLarge) {
    return kTooLarge;
  }
  const Cost max = static_cast<Cost>(std::numeric_limits<Weight>::max());
  if (second > max - first) {
    return kTooLarge;
  }
  return first + second;
}

[[nodiscard]] std::vector<LogicalEdge> collect_logical_edges(const Graph& graph) {
  std::vector<LogicalEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const Edge& edge = neighbors[index];
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "minimum_chinese_postman_tour requires non-negative edge weights");
      }
      if (from <= edge.to) {
        edges.push_back(LogicalEdge{from, edge.to, edge.weight, index});
      }
    }
  }
  return edges;
}

[[nodiscard]] std::vector<std::vector<Arc>> build_edge_adjacency(
    std::size_t vertex_count, const std::vector<LogicalEdge>& edges) {
  std::vector<std::vector<Arc>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const LogicalEdge& edge = edges[edge_id];
    adjacency[edge.from].push_back(Arc{edge.to, edge_id});
    if (edge.from != edge.to) {
      adjacency[edge.to].push_back(Arc{edge.from, edge_id});
    }
  }
  return adjacency;
}

[[nodiscard]] bool active_subgraph_connected(
    const std::vector<std::size_t>& degree,
    const std::vector<std::vector<Arc>>& adjacency) {
  std::optional<Vertex> start;
  for (Vertex vertex = 0; vertex < degree.size(); ++vertex) {
    if (degree[vertex] != 0U) {
      start = vertex;
      break;
    }
  }
  if (!start.has_value()) {
    return true;
  }

  std::vector<bool> visited(degree.size(), false);
  std::vector<Vertex> stack{*start};
  visited[*start] = true;
  while (!stack.empty()) {
    const Vertex from = stack.back();
    stack.pop_back();
    for (const Arc& arc : adjacency[from]) {
      if (!visited[arc.to]) {
        visited[arc.to] = true;
        stack.push_back(arc.to);
      }
    }
  }

  for (Vertex vertex = 0; vertex < degree.size(); ++vertex) {
    if (degree[vertex] != 0U && !visited[vertex]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] ShortestPaths dijkstra_from(
    Vertex source, const std::vector<LogicalEdge>& edges,
    const std::vector<std::vector<Arc>>& adjacency) {
  ShortestPaths result;
  result.distance.assign(adjacency.size(), kUnreachable);
  result.parent_vertex.resize(adjacency.size());
  result.parent_edge.resize(adjacency.size());
  result.distance[source] = 0;

  data_structures::BinaryHeap<QueueEntry, QueueEntryCompare> heap;
  heap.push(QueueEntry{0, source});
  while (!heap.empty()) {
    const QueueEntry current = heap.pop();
    if (current.distance != result.distance[current.vertex]) {
      continue;
    }
    for (const Arc& arc : adjacency[current.vertex]) {
      const Cost candidate = saturated_add(
          current.distance, static_cast<Cost>(edges[arc.edge_id].weight));
      if (candidate < result.distance[arc.to]) {
        result.distance[arc.to] = candidate;
        result.parent_vertex[arc.to] = current.vertex;
        result.parent_edge[arc.to] = arc.edge_id;
        heap.push(QueueEntry{candidate, arc.to});
      }
    }
  }
  return result;
}

[[nodiscard]] PostmanEdgeReference edge_reference(const LogicalEdge& edge) {
  return PostmanEdgeReference{edge.from, edge.adjacency_index, edge.to,
                              edge.weight};
}

[[nodiscard]] std::vector<std::size_t> reconstruct_path_edge_ids(
    Vertex source, Vertex target, const ShortestPaths& shortest) {
  std::vector<std::size_t> reversed;
  Vertex current = target;
  while (current != source) {
    if (!shortest.parent_vertex[current].has_value() ||
        !shortest.parent_edge[current].has_value()) {
      throw std::logic_error("reachable postman metric path lacks parent");
    }
    reversed.push_back(*shortest.parent_edge[current]);
    current = *shortest.parent_vertex[current];
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

[[nodiscard]] std::pair<std::vector<Vertex>, std::vector<std::size_t>>
closed_eulerian_walk(std::size_t vertex_count,
                     const std::vector<LogicalEdge>& edges,
                     const std::vector<Occurrence>& occurrences) {
  if (occurrences.empty()) {
    if (vertex_count == 0U) {
      return {{}, {}};
    }
    return {{0}, {}};
  }

  std::vector<std::vector<std::size_t>> incident(vertex_count);
  for (std::size_t occurrence_id = 0; occurrence_id < occurrences.size();
       ++occurrence_id) {
    const LogicalEdge& edge = edges[occurrences[occurrence_id].edge_id];
    incident[edge.from].push_back(occurrence_id);
    if (edge.from != edge.to) {
      incident[edge.to].push_back(occurrence_id);
    }
  }

  Vertex start = 0;
  while (start < vertex_count && incident[start].empty()) {
    ++start;
  }
  if (start == vertex_count) {
    throw std::logic_error("non-empty postman occurrence set has no endpoint");
  }

  std::vector<bool> used(occurrences.size(), false);
  std::vector<std::size_t> cursor(vertex_count, 0);
  std::vector<Vertex> vertex_stack{start};
  std::vector<std::optional<std::size_t>> incoming_stack{std::nullopt};
  std::vector<Vertex> reversed_vertices;
  std::vector<std::size_t> reversed_occurrences;

  while (!vertex_stack.empty()) {
    const Vertex vertex = vertex_stack.back();
    auto& index = cursor[vertex];
    while (index < incident[vertex].size() && used[incident[vertex][index]]) {
      ++index;
    }
    if (index == incident[vertex].size()) {
      reversed_vertices.push_back(vertex);
      if (incoming_stack.back().has_value()) {
        reversed_occurrences.push_back(*incoming_stack.back());
      }
      vertex_stack.pop_back();
      incoming_stack.pop_back();
      continue;
    }

    const std::size_t occurrence_id = incident[vertex][index++];
    if (used[occurrence_id]) {
      continue;
    }
    used[occurrence_id] = true;
    const LogicalEdge& edge = edges[occurrences[occurrence_id].edge_id];
    const Vertex next = edge.from == vertex ? edge.to : edge.from;
    vertex_stack.push_back(next);
    incoming_stack.push_back(occurrence_id);
  }

  if (reversed_occurrences.size() != occurrences.size()) {
    throw std::logic_error("postman augmentation is not Eulerian-connected");
  }
  std::reverse(reversed_vertices.begin(), reversed_vertices.end());
  std::reverse(reversed_occurrences.begin(), reversed_occurrences.end());
  if (reversed_vertices.front() != reversed_vertices.back()) {
    throw std::logic_error("postman Hierholzer witness is not closed");
  }
  return {std::move(reversed_vertices), std::move(reversed_occurrences)};
}

}  // namespace

std::optional<ChinesePostmanResult> minimum_chinese_postman_tour(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "minimum_chinese_postman_tour requires an undirected graph");
  }

  const std::vector<LogicalEdge> edges = collect_logical_edges(graph);
  const auto adjacency = build_edge_adjacency(graph.vertex_count(), edges);
  std::vector<std::size_t> degree(graph.vertex_count(), 0);
  Cost original_cost = 0;
  for (const LogicalEdge& edge : edges) {
    original_cost = saturated_add(original_cost, static_cast<Cost>(edge.weight));
    if (edge.from == edge.to) {
      degree[edge.from] += 2U;
    } else {
      ++degree[edge.from];
      ++degree[edge.to];
    }
  }

  if (!active_subgraph_connected(degree, adjacency)) {
    return std::nullopt;
  }
  if (original_cost >= kTooLarge) {
    throw std::overflow_error("original postman edge weight exceeds int64_t");
  }

  std::vector<Vertex> odd_vertices;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if ((degree[vertex] & 1U) != 0U) {
      odd_vertices.push_back(vertex);
    }
  }
  if ((odd_vertices.size() & 1U) != 0U) {
    throw std::logic_error("undirected graph has an odd count of odd vertices");
  }
  if (odd_vertices.size() >= std::numeric_limits<std::size_t>::digits) {
    throw std::length_error("too many odd vertices for exact subset pairing");
  }

  std::vector<ShortestPaths> shortest;
  shortest.reserve(odd_vertices.size());
  for (Vertex source : odd_vertices) {
    shortest.push_back(dijkstra_from(source, edges, adjacency));
  }

  const std::size_t mask_count = std::size_t{1} << odd_vertices.size();
  if (mask_count > std::vector<Cost>{}.max_size() ||
      mask_count > std::vector<std::size_t>{}.max_size()) {
    throw std::length_error("postman subset pairing state is not representable");
  }
  std::vector<Cost> pairing_cost(mask_count, kUnreachable);
  std::vector<std::size_t> pairing_partner(mask_count, kNoIndex);
  pairing_cost[0] = 0;

  for (std::size_t mask = 1; mask < mask_count; ++mask) {
    if ((std::popcount(mask) & 1) != 0) {
      continue;
    }
    const std::size_t first = static_cast<std::size_t>(std::countr_zero(mask));
    const std::size_t without_first = mask ^ (std::size_t{1} << first);
    for (std::size_t second = first + 1; second < odd_vertices.size(); ++second) {
      const std::size_t second_bit = std::size_t{1} << second;
      if ((without_first & second_bit) == 0U) {
        continue;
      }
      const std::size_t remainder = without_first ^ second_bit;
      const Cost metric = shortest[first].distance[odd_vertices[second]];
      if (metric == kUnreachable || pairing_cost[remainder] == kUnreachable) {
        continue;
      }
      const Cost candidate = saturated_add(metric, pairing_cost[remainder]);
      if (candidate < pairing_cost[mask]) {
        pairing_cost[mask] = candidate;
        pairing_partner[mask] = second;
      }
    }
  }

  const std::size_t full_mask = mask_count - 1U;
  if (pairing_cost[full_mask] == kUnreachable) {
    throw std::logic_error("connected postman graph has unreachable odd pair");
  }
  const Cost total_cost = saturated_add(original_cost, pairing_cost[full_mask]);
  if (total_cost >= kTooLarge) {
    throw std::overflow_error("minimum Chinese-postman tour exceeds int64_t");
  }

  ChinesePostmanResult result;
  result.original_edge_weight = static_cast<Weight>(original_cost);
  result.duplicated_edge_weight = static_cast<Weight>(pairing_cost[full_mask]);
  result.total_weight = static_cast<Weight>(total_cost);

  std::vector<Occurrence> occurrences;
  occurrences.reserve(edges.size());
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    occurrences.push_back(Occurrence{edge_id, false});
  }

  std::size_t mask = full_mask;
  while (mask != 0U) {
    const std::size_t first = static_cast<std::size_t>(std::countr_zero(mask));
    const std::size_t second = pairing_partner[mask];
    if (second == kNoIndex) {
      throw std::logic_error("postman pairing reconstruction lacks partner");
    }
    const Vertex source = odd_vertices[first];
    const Vertex target = odd_vertices[second];
    const auto path_ids =
        reconstruct_path_edge_ids(source, target, shortest[first]);

    PostmanAugmentation augmentation;
    augmentation.first = source;
    augmentation.second = target;
    augmentation.distance = static_cast<Weight>(shortest[first].distance[target]);
    Cost replayed = 0;
    for (std::size_t edge_id : path_ids) {
      replayed = saturated_add(replayed, static_cast<Cost>(edges[edge_id].weight));
      augmentation.path.push_back(edge_reference(edges[edge_id]));
      occurrences.push_back(Occurrence{edge_id, true});
    }
    if (replayed != shortest[first].distance[target]) {
      throw std::logic_error("postman shortest-path reconstruction cost mismatch");
    }
    result.augmentations.push_back(std::move(augmentation));
    mask ^= std::size_t{1} << first;
    mask ^= std::size_t{1} << second;
  }

  const auto [vertices, walk_occurrences] =
      closed_eulerian_walk(graph.vertex_count(), edges, occurrences);
  result.vertices = vertices;
  Cost replayed_total = 0;
  for (std::size_t occurrence_id : walk_occurrences) {
    const Occurrence occurrence = occurrences[occurrence_id];
    const LogicalEdge& edge = edges[occurrence.edge_id];
    replayed_total = saturated_add(replayed_total, static_cast<Cost>(edge.weight));
    result.edge_walk.push_back(
        PostmanTraversalStep{edge_reference(edge), occurrence.duplicated});
  }
  if (replayed_total != total_cost) {
    throw std::logic_error("postman closed-walk cost disagrees with optimum");
  }
  if (!result.edge_walk.empty() &&
      result.vertices.size() != result.edge_walk.size() + 1U) {
    throw std::logic_error("postman walk edge/vertex counts disagree");
  }
  return result;
}

}  // namespace algorithms::graphs
