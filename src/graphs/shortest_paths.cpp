#include "algorithms/graphs/shortest_paths.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include "algorithms/data_structures/binary_heap.hpp"

namespace algorithms::graphs {
namespace {

Weight checked_add(Weight a, Weight b) {
  constexpr Weight max = std::numeric_limits<Weight>::max();
  constexpr Weight min = std::numeric_limits<Weight>::min();
  if (b > 0 && a > max - b) {
    throw std::overflow_error("shortest-path distance overflow");
  }
  if (b < 0 && a < min - b) {
    throw std::overflow_error("shortest-path distance underflow");
  }
  return a + b;
}

struct QueueEntry {
  Weight distance;
  Vertex vertex;
};

struct QueueEntryCompare {
  bool operator()(const QueueEntry& a, const QueueEntry& b) const noexcept {
    if (a.distance != b.distance) {
      return a.distance < b.distance;
    }
    return a.vertex < b.vertex;
  }
};

using WideDistance = std::uint64_t;

struct WideQueueEntry {
  WideDistance distance;
  Vertex vertex;
};

struct WideQueueEntryCompare {
  bool operator()(const WideQueueEntry& a,
                  const WideQueueEntry& b) const noexcept {
    if (a.distance != b.distance) {
      return a.distance < b.distance;
    }
    return a.vertex < b.vertex;
  }
};

[[nodiscard]] WideDistance nonpositive_magnitude(Weight value) {
  if (value > 0) {
    throw std::logic_error("Johnson potential unexpectedly positive");
  }
  if (value == std::numeric_limits<Weight>::min()) {
    return WideDistance{1} << 63U;
  }
  return static_cast<WideDistance>(-value);
}

[[nodiscard]] WideDistance negative_magnitude(Weight value) {
  if (value >= 0) {
    throw std::logic_error("negative_magnitude requires a negative value");
  }
  return static_cast<WideDistance>(-(value + 1)) + WideDistance{1};
}

[[nodiscard]] WideDistance reweighted_edge_weight(
    Weight edge_weight, Weight from_potential, Weight to_potential) {
  const WideDistance from_magnitude =
      nonpositive_magnitude(from_potential);
  const WideDistance to_magnitude = nonpositive_magnitude(to_potential);

  if (edge_weight >= 0) {
    const WideDistance weight = static_cast<WideDistance>(edge_weight);
    if (to_magnitude >= from_magnitude) {
      const WideDistance delta = to_magnitude - from_magnitude;
      if (weight > std::numeric_limits<WideDistance>::max() - delta) {
        throw std::overflow_error("Johnson reweighted edge exceeds uint64_t");
      }
      return delta + weight;
    }

    const WideDistance delta = from_magnitude - to_magnitude;
    if (weight < delta) {
      throw std::logic_error("Johnson potential produced a negative edge");
    }
    return weight - delta;
  }

  if (to_magnitude < from_magnitude) {
    throw std::logic_error("Johnson potential produced a negative edge");
  }
  const WideDistance delta = to_magnitude - from_magnitude;
  const WideDistance magnitude = negative_magnitude(edge_weight);
  if (delta < magnitude) {
    throw std::logic_error("Johnson potential produced a negative edge");
  }
  return delta - magnitude;
}

[[nodiscard]] Weight restore_original_distance(
    WideDistance reweighted_distance, Weight source_potential,
    Weight target_potential) {
  constexpr WideDistance max_signed =
      static_cast<WideDistance>(std::numeric_limits<Weight>::max());
  constexpr WideDistance min_magnitude = WideDistance{1} << 63U;

  const WideDistance source_magnitude =
      nonpositive_magnitude(source_potential);
  const WideDistance target_magnitude =
      nonpositive_magnitude(target_potential);

  if (reweighted_distance >= target_magnitude) {
    const WideDistance remainder = reweighted_distance - target_magnitude;
    if (source_magnitude > max_signed ||
        remainder > max_signed - source_magnitude) {
      throw std::overflow_error("Johnson final distance exceeds int64_t");
    }
    return static_cast<Weight>(remainder + source_magnitude);
  }

  const WideDistance deficit = target_magnitude - reweighted_distance;
  if (source_magnitude >= deficit) {
    const WideDistance value = source_magnitude - deficit;
    if (value > max_signed) {
      throw std::overflow_error("Johnson final distance exceeds int64_t");
    }
    return static_cast<Weight>(value);
  }

  const WideDistance magnitude = deficit - source_magnitude;
  if (magnitude == min_magnitude) {
    return std::numeric_limits<Weight>::min();
  }
  if (magnitude > max_signed) {
    throw std::overflow_error("Johnson final distance is below int64_t");
  }
  return -static_cast<Weight>(magnitude);
}

[[nodiscard]] std::optional<std::vector<Weight>> johnson_potentials(
    const Graph& graph) {
  std::vector<Weight> potential(graph.vertex_count(), Weight{0});
  if (graph.vertex_count() == 0U) {
    return potential;
  }

  for (std::size_t pass = 0; pass < graph.vertex_count(); ++pass) {
    bool changed = false;
    for (Vertex from = 0; from < graph.vertex_count(); ++from) {
      for (const Edge& edge : graph.neighbors(from)) {
        const Weight candidate = checked_add(potential[from], edge.weight);
        if (candidate < potential[edge.to]) {
          potential[edge.to] = candidate;
          changed = true;
        }
      }
    }
    if (!changed) {
      return potential;
    }
    if (pass + 1U == graph.vertex_count()) {
      return std::nullopt;
    }
  }
  return potential;
}

[[nodiscard]] std::vector<bool> reachable_vertices(const Graph& graph,
                                                   Vertex source) {
  std::vector<bool> reachable(graph.vertex_count(), false);
  std::vector<Vertex> stack;
  stack.push_back(source);
  reachable[source] = true;

  while (!stack.empty()) {
    const Vertex from = stack.back();
    stack.pop_back();
    for (const Edge& edge : graph.neighbors(from)) {
      if (!reachable[edge.to]) {
        reachable[edge.to] = true;
        stack.push_back(edge.to);
      }
    }
  }
  return reachable;
}

struct WideShortestPathResult {
  std::vector<std::optional<WideDistance>> distance;
  std::vector<std::optional<Vertex>> parent;
};

[[nodiscard]] WideShortestPathResult dijkstra_reweighted(
    const Graph& graph, Vertex source, const std::vector<Weight>& potential) {
  WideShortestPathResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = WideDistance{0};

  data_structures::BinaryHeap<WideQueueEntry, WideQueueEntryCompare> queue;
  queue.push(WideQueueEntry{WideDistance{0}, source});

  while (!queue.empty()) {
    const WideQueueEntry current = queue.pop();
    if (!result.distance[current.vertex].has_value() ||
        current.distance != *result.distance[current.vertex]) {
      continue;
    }

    for (const Edge& edge : graph.neighbors(current.vertex)) {
      const WideDistance reweighted = reweighted_edge_weight(
          edge.weight, potential[current.vertex], potential[edge.to]);
      if (reweighted >
          std::numeric_limits<WideDistance>::max() - current.distance) {
        continue;
      }
      const WideDistance candidate = current.distance + reweighted;
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.distance[edge.to] = candidate;
        result.parent[edge.to] = current.vertex;
        queue.push(WideQueueEntry{candidate, edge.to});
      }
    }
  }
  return result;
}

}  // namespace

ShortestPathResult dijkstra(const Graph& graph, Vertex source) {
  graph.validate_vertex(source);
  for (const auto& edges : graph.adjacency()) {
    for (const Edge& edge : edges) {
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "dijkstra requires globally non-negative edge weights");
      }
    }
  }

  ShortestPathResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = 0;

  data_structures::BinaryHeap<QueueEntry, QueueEntryCompare> queue;
  queue.push(QueueEntry{0, source});

  while (!queue.empty()) {
    const QueueEntry current = queue.pop();
    if (!result.distance[current.vertex].has_value() ||
        current.distance != *result.distance[current.vertex]) {
      continue;  // stale entry
    }

    for (const Edge& edge : graph.neighbors(current.vertex)) {
      const Weight candidate = checked_add(current.distance, edge.weight);
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.distance[edge.to] = candidate;
        result.parent[edge.to] = current.vertex;
        queue.push(QueueEntry{candidate, edge.to});
      }
    }
  }
  return result;
}

BellmanFordResult bellman_ford(const Graph& graph, Vertex source) {
  graph.validate_vertex(source);
  BellmanFordResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = 0;

  if (graph.vertex_count() > 0) {
    for (std::size_t pass = 1; pass < graph.vertex_count(); ++pass) {
      bool changed = false;
      for (Vertex from = 0; from < graph.vertex_count(); ++from) {
        if (!result.distance[from].has_value()) {
          continue;
        }
        for (const Edge& edge : graph.neighbors(from)) {
          const Weight candidate = checked_add(*result.distance[from], edge.weight);
          if (!result.distance[edge.to].has_value() ||
              candidate < *result.distance[edge.to]) {
            result.distance[edge.to] = candidate;
            result.parent[edge.to] = from;
            changed = true;
          }
        }
      }
      if (!changed) {
        break;
      }
    }
  }

  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    if (!result.distance[from].has_value()) {
      continue;
    }
    for (const Edge& edge : graph.neighbors(from)) {
      const Weight candidate = checked_add(*result.distance[from], edge.weight);
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.has_reachable_negative_cycle = true;
        return result;
      }
    }
  }
  return result;
}

std::optional<AllPairsShortestPathResult> johnson_all_pairs_shortest_paths(
    const Graph& graph) {
  const auto potential = johnson_potentials(graph);
  if (!potential.has_value()) {
    return std::nullopt;
  }

  AllPairsShortestPathResult result;
  result.distance.resize(
      graph.vertex_count(),
      std::vector<std::optional<Weight>>(graph.vertex_count()));
  result.parent.resize(
      graph.vertex_count(),
      std::vector<std::optional<Vertex>>(graph.vertex_count()));

  for (Vertex source = 0; source < graph.vertex_count(); ++source) {
    const auto reachable = reachable_vertices(graph, source);
    const auto reweighted = dijkstra_reweighted(graph, source, *potential);
    for (Vertex target = 0; target < graph.vertex_count(); ++target) {
      if (!reweighted.distance[target].has_value()) {
        if (reachable[target]) {
          throw std::overflow_error(
              "Johnson reweighted shortest distance exceeds uint64_t");
        }
        continue;
      }
      result.distance[source][target] = restore_original_distance(
          *reweighted.distance[target], (*potential)[source],
          (*potential)[target]);
      result.parent[source][target] = reweighted.parent[target];
    }
  }
  return result;
}

}  // namespace algorithms::graphs
