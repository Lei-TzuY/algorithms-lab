#include "algorithms/graphs/steiner_tree.hpp"

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

using Cost = std::uint64_t;
constexpr Cost kUnreachable = std::numeric_limits<Cost>::max();
constexpr Cost kTooLarge =
    static_cast<Cost>(std::numeric_limits<Weight>::max()) + Cost{1};

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

enum class ParentKind : std::uint8_t { None, Seed, Merge, Path };

struct Parent {
  ParentKind kind = ParentKind::None;
  std::size_t first = 0;
  std::size_t second = 0;
};

struct State {
  Cost cost = kUnreachable;
  Parent parent{};
};

struct QueueEntry {
  Cost cost = 0;
  Vertex vertex = 0;
};

struct QueueEntryCompare {
  [[nodiscard]] bool operator()(const QueueEntry& first,
                                const QueueEntry& second) const noexcept {
    if (first.cost != second.cost) {
      return first.cost < second.cost;
    }
    return first.vertex < second.vertex;
  }
};

[[nodiscard]] Cost saturated_add(Cost first, Cost second) noexcept {
  if (first >= kTooLarge || second >= kTooLarge) {
    return kTooLarge;
  }
  const Cost representable = static_cast<Cost>(std::numeric_limits<Weight>::max());
  if (second > representable - first) {
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
            "minimum_steiner_tree requires non-negative edge weights");
      }
      if (from <= edge.to) {
        edges.push_back(LogicalEdge{from, edge.to, edge.weight, index});
      }
    }
  }
  return edges;
}

class DisjointSet {
 public:
  explicit DisjointSet(std::size_t count) : parent_(count), size_(count, 1) {
    for (std::size_t index = 0; index < count; ++index) {
      parent_[index] = index;
    }
  }

  [[nodiscard]] std::size_t find(std::size_t value) {
    std::size_t root = value;
    while (parent_[root] != root) {
      root = parent_[root];
    }
    while (parent_[value] != value) {
      const std::size_t next = parent_[value];
      parent_[value] = root;
      value = next;
    }
    return root;
  }

  [[nodiscard]] bool unite(std::size_t first, std::size_t second) {
    first = find(first);
    second = find(second);
    if (first == second) {
      return false;
    }
    if (size_[first] < size_[second]) {
      std::swap(first, second);
    }
    parent_[second] = first;
    size_[first] += size_[second];
    return true;
  }

 private:
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> size_;
};

}  // namespace

std::optional<SteinerTreeResult> minimum_steiner_tree(
    const Graph& graph, std::vector<Vertex> terminals) {
  if (graph.directed()) {
    throw std::invalid_argument("minimum_steiner_tree requires an undirected graph");
  }

  const std::vector<LogicalEdge> edges = collect_logical_edges(graph);

  for (Vertex terminal : terminals) {
    graph.validate_vertex(terminal);
  }
  std::sort(terminals.begin(), terminals.end());
  terminals.erase(std::unique(terminals.begin(), terminals.end()), terminals.end());

  SteinerTreeResult trivial;
  trivial.terminals = terminals;
  if (terminals.size() <= 1) {
    return trivial;
  }

  if (terminals.size() >= std::numeric_limits<std::size_t>::digits) {
    throw std::length_error("too many terminals for subset-state representation");
  }

  const std::size_t vertex_count = graph.vertex_count();
  const std::size_t mask_count = std::size_t{1} << terminals.size();
  if (vertex_count != 0 &&
      mask_count > std::numeric_limits<std::size_t>::max() / vertex_count) {
    throw std::length_error("Steiner DP state count is not representable");
  }
  const std::size_t state_count = mask_count * vertex_count;
  std::vector<State> states(state_count);

  std::vector<std::vector<Arc>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const LogicalEdge& edge = edges[edge_id];
    adjacency[edge.from].push_back(Arc{edge.to, edge_id});
    if (edge.from != edge.to) {
      adjacency[edge.to].push_back(Arc{edge.from, edge_id});
    }
  }

  const auto state_index = [vertex_count](std::size_t mask, Vertex vertex) {
    return mask * vertex_count + vertex;
  };

  for (std::size_t terminal_index = 0; terminal_index < terminals.size();
       ++terminal_index) {
    const std::size_t mask = std::size_t{1} << terminal_index;
    State& state = states[state_index(mask, terminals[terminal_index])];
    state.cost = 0;
    state.parent = Parent{ParentKind::Seed, terminal_index, 0};
  }

  for (std::size_t mask = 1; mask < mask_count; ++mask) {
    if ((mask & (mask - 1)) != 0) {
      for (std::size_t subset = (mask - 1) & mask; subset != 0;
           subset = (subset - 1) & mask) {
        const std::size_t other = mask ^ subset;
        if (subset > other || other == 0) {
          continue;
        }
        for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
          const State& left = states[state_index(subset, vertex)];
          const State& right = states[state_index(other, vertex)];
          if (left.cost == kUnreachable || right.cost == kUnreachable) {
            continue;
          }
          const Cost candidate = saturated_add(left.cost, right.cost);
          State& current = states[state_index(mask, vertex)];
          if (candidate < current.cost) {
            current.cost = candidate;
            current.parent = Parent{ParentKind::Merge, subset, 0};
          }
        }
      }
    }

    algorithms::data_structures::BinaryHeap<QueueEntry, QueueEntryCompare> heap;
    for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
      const Cost cost = states[state_index(mask, vertex)].cost;
      if (cost != kUnreachable) {
        heap.push(QueueEntry{cost, vertex});
      }
    }

    while (!heap.empty()) {
      const QueueEntry current_entry = heap.pop();
      State& current_state = states[state_index(mask, current_entry.vertex)];
      if (current_entry.cost != current_state.cost) {
        continue;
      }
      for (const Arc& arc : adjacency[current_entry.vertex]) {
        const LogicalEdge& edge = edges[arc.edge_id];
        const Cost candidate = saturated_add(
            current_entry.cost, static_cast<Cost>(edge.weight));
        State& next_state = states[state_index(mask, arc.to)];
        if (candidate < next_state.cost) {
          next_state.cost = candidate;
          next_state.parent =
              Parent{ParentKind::Path, current_entry.vertex, arc.edge_id};
          heap.push(QueueEntry{candidate, arc.to});
        }
      }
    }
  }

  const std::size_t full_mask = mask_count - 1;
  Cost best_cost = kUnreachable;
  Vertex best_root = 0;
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    const Cost cost = states[state_index(full_mask, vertex)].cost;
    if (cost < best_cost) {
      best_cost = cost;
      best_root = vertex;
    }
  }
  if (best_cost == kUnreachable) {
    return std::nullopt;
  }
  if (best_cost >= kTooLarge) {
    throw std::overflow_error("minimum Steiner-tree weight exceeds int64_t");
  }

  std::vector<bool> selected(edges.size(), false);
  std::vector<std::pair<std::size_t, Vertex>> stack;
  stack.push_back({full_mask, best_root});
  while (!stack.empty()) {
    const auto [mask, vertex] = stack.back();
    stack.pop_back();
    const State& state = states[state_index(mask, vertex)];
    switch (state.parent.kind) {
      case ParentKind::Seed:
        break;
      case ParentKind::Merge: {
        const std::size_t subset = state.parent.first;
        stack.push_back({subset, vertex});
        stack.push_back({mask ^ subset, vertex});
        break;
      }
      case ParentKind::Path:
        selected[state.parent.second] = true;
        stack.push_back({mask, state.parent.first});
        break;
      case ParentKind::None:
        throw std::logic_error("reachable Steiner state lacks reconstruction parent");
    }
  }

  DisjointSet forest(vertex_count);
  Cost witness_cost = 0;
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    if (!selected[edge_id]) {
      continue;
    }
    const LogicalEdge& edge = edges[edge_id];
    if (edge.from == edge.to || !forest.unite(edge.from, edge.to)) {
      continue;
    }
    witness_cost = saturated_add(witness_cost, static_cast<Cost>(edge.weight));
    trivial.edges.push_back(
        SteinerTreeEdge{edge.from, edge.adjacency_index, edge.to, edge.weight});
  }

  if (witness_cost != best_cost) {
    throw std::logic_error("Steiner reconstruction cost disagrees with DP optimum");
  }
  const std::size_t representative = forest.find(terminals.front());
  for (Vertex terminal : terminals) {
    if (forest.find(terminal) != representative) {
      throw std::logic_error("Steiner reconstruction does not connect all terminals");
    }
  }

  trivial.total_weight = static_cast<Weight>(best_cost);
  return trivial;
}

}  // namespace algorithms::graphs
