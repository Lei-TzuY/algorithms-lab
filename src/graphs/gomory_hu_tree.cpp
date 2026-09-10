#include "algorithms/graphs/gomory_hu_tree.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void validate_input(std::size_t vertex_count,
                    std::span<const UndirectedCapacityEdge> edges) {
  for (const auto& edge : edges) {
    if (edge.first >= vertex_count || edge.second >= vertex_count) {
      throw std::out_of_range("Gomory-Hu edge endpoint is out of range");
    }
    if (edge.capacity < 0) {
      throw std::invalid_argument(
          "Gomory-Hu capacities must be non-negative");
    }
  }
}

[[nodiscard]] std::vector<CapacityEdge> directed_capacity_network(
    std::span<const UndirectedCapacityEdge> edges) {
  std::vector<CapacityEdge> directed;
  for (const auto& edge : edges) {
    if (edge.first == edge.second) {
      continue;
    }
    directed.push_back(CapacityEdge{edge.first, edge.second, edge.capacity});
    directed.push_back(CapacityEdge{edge.second, edge.first, edge.capacity});
  }
  return directed;
}

using WideWeight = std::uint64_t;

[[nodiscard]] WideWeight checked_wide_add(WideWeight first,
                                          WideWeight second) {
  if (second > std::numeric_limits<WideWeight>::max() - first) {
    throw std::overflow_error(
        "Stoer-Wagner total non-loop capacity exceeds uint64_t");
  }
  return first + second;
}

[[nodiscard]] std::vector<bool> normalized_side(
    std::size_t vertex_count, const std::vector<Vertex>& members) {
  std::vector<bool> side(vertex_count, false);
  for (const Vertex vertex : members) {
    side[vertex] = true;
  }
  if (side[0]) {
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      side[vertex] = !side[vertex];
    }
  }
  return side;
}

[[nodiscard]] bool side_is_less(const std::vector<bool>& first,
                                const std::vector<bool>& second) {
  for (std::size_t index = 0; index < first.size(); ++index) {
    if (first[index] != second[index]) {
      return !first[index] && second[index];
    }
  }
  return false;
}

}  // namespace

GomoryHuTree::GomoryHuTree(std::vector<Vertex> parent,
                           std::vector<Capacity> cut_to_parent)
    : parent_(std::move(parent)), cut_to_parent_(std::move(cut_to_parent)) {}

std::size_t GomoryHuTree::vertex_count() const noexcept { return parent_.size(); }

const std::vector<Vertex>& GomoryHuTree::parent() const noexcept {
  return parent_;
}

const std::vector<Capacity>& GomoryHuTree::cut_to_parent() const noexcept {
  return cut_to_parent_;
}

Capacity GomoryHuTree::min_cut(Vertex first, Vertex second) const {
  if (first >= vertex_count() || second >= vertex_count()) {
    throw std::out_of_range("Gomory-Hu query vertex is out of range");
  }
  if (first == second) {
    throw std::invalid_argument("Gomory-Hu query vertices must differ");
  }

  std::vector<std::vector<std::pair<Vertex, Capacity>>> adjacency(vertex_count());
  for (Vertex vertex = 1; vertex < vertex_count(); ++vertex) {
    const Vertex ancestor = parent_[vertex];
    const Capacity capacity = cut_to_parent_[vertex];
    adjacency[vertex].push_back({ancestor, capacity});
    adjacency[ancestor].push_back({vertex, capacity});
  }

  std::vector<bool> seen(vertex_count(), false);
  std::vector<Capacity> bottleneck(
      vertex_count(), std::numeric_limits<Capacity>::max());
  std::queue<Vertex> queue;
  seen[first] = true;
  queue.push(first);

  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const auto& [next, capacity] : adjacency[vertex]) {
      if (seen[next]) {
        continue;
      }
      seen[next] = true;
      bottleneck[next] = std::min(bottleneck[vertex], capacity);
      if (next == second) {
        return bottleneck[next];
      }
      queue.push(next);
    }
  }

  throw std::logic_error("Gomory-Hu tree is disconnected");
}

GomoryHuTree build_gomory_hu_tree(
    std::size_t vertex_count,
    std::span<const UndirectedCapacityEdge> edges) {
  validate_input(vertex_count, edges);
  if (vertex_count == 0) {
    return GomoryHuTree({}, {});
  }

  const auto directed = directed_capacity_network(edges);
  std::vector<Vertex> parent(vertex_count, Vertex{0});
  std::vector<Capacity> cut_to_parent(vertex_count, Capacity{0});

  for (Vertex source = 1; source < vertex_count; ++source) {
    const Vertex sink = parent[source];
    const auto flow = dinic_max_flow(vertex_count, directed, source, sink);
    const auto& source_side = flow.source_side_min_cut;

    for (Vertex vertex = source + 1; vertex < vertex_count; ++vertex) {
      if (parent[vertex] == sink && source_side[vertex]) {
        parent[vertex] = source;
      }
    }

    if (sink != 0 && source_side[parent[sink]]) {
      parent[source] = parent[sink];
      parent[sink] = source;
      cut_to_parent[source] = cut_to_parent[sink];
      cut_to_parent[sink] = flow.value;
    } else {
      cut_to_parent[source] = flow.value;
    }
  }

  return GomoryHuTree(std::move(parent), std::move(cut_to_parent));
}

std::optional<WeightedGlobalMinCutResult> stoer_wagner_global_min_cut(
    std::size_t vertex_count,
    std::span<const UndirectedCapacityEdge> edges) {
  validate_input(vertex_count, edges);
  if (vertex_count < 2) {
    return std::nullopt;
  }

  WideWeight total_non_loop_capacity = 0;
  std::vector<std::vector<WideWeight>> adjacency(
      vertex_count, std::vector<WideWeight>(vertex_count, 0));

  for (const auto& edge : edges) {
    if (edge.first == edge.second) {
      continue;
    }
    const auto capacity = static_cast<WideWeight>(edge.capacity);
    total_non_loop_capacity =
        checked_wide_add(total_non_loop_capacity, capacity);
    adjacency[edge.first][edge.second] =
        checked_wide_add(adjacency[edge.first][edge.second], capacity);
    adjacency[edge.second][edge.first] =
        checked_wide_add(adjacency[edge.second][edge.first], capacity);
  }

  std::vector<Vertex> active(vertex_count);
  std::iota(active.begin(), active.end(), Vertex{0});

  std::vector<std::vector<Vertex>> members(vertex_count);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    members[vertex].push_back(vertex);
  }

  WideWeight best_weight = std::numeric_limits<WideWeight>::max();
  std::vector<bool> best_side;

  while (active.size() > 1) {
    std::vector<bool> added(vertex_count, false);
    std::vector<WideWeight> connection(vertex_count, 0);
    Vertex previous = vertex_count;

    for (std::size_t order = 0; order < active.size(); ++order) {
      Vertex selected = vertex_count;
      for (const Vertex vertex : active) {
        if (added[vertex]) {
          continue;
        }
        if (selected == vertex_count ||
            connection[vertex] > connection[selected] ||
            (connection[vertex] == connection[selected] &&
             vertex < selected)) {
          selected = vertex;
        }
      }

      if (order + 1 == active.size()) {
        const WideWeight phase_cut = connection[selected];
        auto candidate_side =
            normalized_side(vertex_count, members[selected]);
        if (phase_cut < best_weight ||
            (phase_cut == best_weight &&
             (best_side.empty() ||
              side_is_less(candidate_side, best_side)))) {
          best_weight = phase_cut;
          best_side = std::move(candidate_side);
        }

        if (previous == vertex_count) {
          throw std::logic_error(
              "Stoer-Wagner phase ended without a merge predecessor");
        }

        for (const Vertex vertex : active) {
          if (vertex == previous || vertex == selected) {
            continue;
          }
          const WideWeight merged =
              checked_wide_add(adjacency[previous][vertex],
                               adjacency[selected][vertex]);
          adjacency[previous][vertex] = merged;
          adjacency[vertex][previous] = merged;
        }

        members[previous].insert(members[previous].end(),
                                 members[selected].begin(),
                                 members[selected].end());
        members[selected].clear();
        active.erase(std::find(active.begin(), active.end(), selected));
        break;
      }

      added[selected] = true;
      previous = selected;
      for (const Vertex vertex : active) {
        if (!added[vertex]) {
          connection[vertex] =
              checked_wide_add(connection[vertex],
                               adjacency[selected][vertex]);
        }
      }
    }
  }

  return WeightedGlobalMinCutResult{best_weight, std::move(best_side)};
}

}  // namespace algorithms::graphs
