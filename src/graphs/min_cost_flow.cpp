#include "algorithms/graphs/min_cost_flow.hpp"

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct ResidualArc {
  Vertex to;
  std::size_t reverse_index;
  Capacity residual;
  FlowCost cost;
};

struct ArcLocation {
  Vertex from;
  std::size_t index;
  bool materialized;
};

struct ParentArc {
  Vertex from;
  std::size_t index;
};

[[nodiscard]] std::int64_t checked_add(std::int64_t left,
                                       std::int64_t right,
                                       const char* message) {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error(message);
  }
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error(message);
  }
  return left + right;
}

[[nodiscard]] std::int64_t checked_subtract(std::int64_t left,
                                            std::int64_t right,
                                            const char* message) {
  if (right > 0 && left < std::numeric_limits<std::int64_t>::min() + right) {
    throw std::overflow_error(message);
  }
  if (right < 0 && left > std::numeric_limits<std::int64_t>::max() + right) {
    throw std::overflow_error(message);
  }
  return left - right;
}

[[nodiscard]] FlowCost checked_flow_cost_product(Capacity flow,
                                                 FlowCost cost) {
  if (flow == 0 || cost == 0) {
    return 0;
  }
  if (flow < 0 || cost == std::numeric_limits<FlowCost>::min()) {
    throw std::logic_error("invalid min-cost-flow product operands");
  }
  const std::uint64_t magnitude =
      cost < 0 ? static_cast<std::uint64_t>(-cost)
               : static_cast<std::uint64_t>(cost);
  const std::uint64_t unsigned_flow = static_cast<std::uint64_t>(flow);
  const std::uint64_t positive_limit =
      static_cast<std::uint64_t>(std::numeric_limits<FlowCost>::max());
  const std::uint64_t magnitude_limit =
      cost < 0 ? positive_limit + std::uint64_t{1} : positive_limit;
  if (unsigned_flow > magnitude_limit / magnitude) {
    throw std::overflow_error("min-cost-flow total cost is not representable");
  }
  const std::uint64_t unsigned_product = unsigned_flow * magnitude;
  if (cost >= 0) {
    return static_cast<FlowCost>(unsigned_product);
  }
  if (unsigned_product == positive_limit + std::uint64_t{1}) {
    return std::numeric_limits<FlowCost>::min();
  }
  return -static_cast<FlowCost>(unsigned_product);
}

class Solver {
 public:
  explicit Solver(std::size_t vertex_count) : adjacency_(vertex_count) {}

  [[nodiscard]] ArcLocation add_edge(Vertex from, Vertex to, Capacity capacity,
                                     FlowCost cost) {
    if (from == to) {
      return ArcLocation{from, 0U, false};
    }
    const std::size_t forward_index = adjacency_[from].size();
    const std::size_t reverse_index = adjacency_[to].size();
    adjacency_[from].push_back(
        ResidualArc{to, reverse_index, capacity, cost});
    adjacency_[to].push_back(
        ResidualArc{from, forward_index, 0, -cost});
    return ArcLocation{from, forward_index, true};
  }

  [[nodiscard]] std::vector<FlowCost> feasible_potentials(
      bool input_validation) const {
    std::vector<FlowCost> distance(adjacency_.size(), 0);
    for (std::size_t pass = 0U; pass < adjacency_.size(); ++pass) {
      bool changed = false;
      for (Vertex from = 0U; from < adjacency_.size(); ++from) {
        for (const auto& arc : adjacency_[from]) {
          if (arc.residual <= 0) {
            continue;
          }
          const FlowCost candidate = checked_add(
              distance[from], arc.cost,
              "min-cost-flow Bellman-Ford distance is not representable");
          if (candidate < distance[arc.to]) {
            distance[arc.to] = candidate;
            changed = true;
            if (pass + 1U == adjacency_.size()) {
              if (input_validation) {
                throw std::invalid_argument(
                    "min-cost-flow input contains a negative-cost cycle");
              }
              throw std::logic_error(
                  "min-cost-flow residual graph contains a negative-cost cycle");
            }
          }
        }
      }
      if (!changed) {
        break;
      }
    }
    return distance;
  }

  [[nodiscard]] std::pair<Capacity, FlowCost> solve(Vertex source,
                                                     Vertex sink) {
    auto potential = feasible_potentials(true);
    Capacity total_flow = 0;
    FlowCost total_cost = 0;

    while (true) {
      std::vector<std::optional<FlowCost>> distance(adjacency_.size());
      std::vector<std::optional<ParentArc>> parent(adjacency_.size());
      using QueueEntry = std::pair<FlowCost, Vertex>;
      data_structures::BinaryHeap<QueueEntry> queue;
      distance[source] = 0;
      queue.push(QueueEntry{0, source});

      while (!queue.empty()) {
        const auto [current_distance, vertex] = queue.pop();
        if (!distance[vertex].has_value() ||
            current_distance != *distance[vertex]) {
          continue;
        }
        for (std::size_t index = 0U; index < adjacency_[vertex].size();
             ++index) {
          const auto& arc = adjacency_[vertex][index];
          if (arc.residual <= 0) {
            continue;
          }
          FlowCost reduced = checked_add(
              arc.cost, potential[vertex],
              "min-cost-flow reduced cost is not representable");
          reduced = checked_subtract(
              reduced, potential[arc.to],
              "min-cost-flow reduced cost is not representable");
          if (reduced < 0) {
            throw std::logic_error(
                "min-cost-flow feasible potential produced negative reduced cost");
          }
          const FlowCost candidate = checked_add(
              current_distance, reduced,
              "min-cost-flow Dijkstra distance is not representable");
          if (!distance[arc.to].has_value() || candidate < *distance[arc.to]) {
            distance[arc.to] = candidate;
            parent[arc.to] = ParentArc{vertex, index};
            queue.push(QueueEntry{candidate, arc.to});
          }
        }
      }

      if (!distance[sink].has_value()) {
        break;
      }
      for (Vertex vertex = 0U; vertex < adjacency_.size(); ++vertex) {
        if (distance[vertex].has_value()) {
          potential[vertex] = checked_add(
              potential[vertex], *distance[vertex],
              "min-cost-flow vertex potential is not representable");
        }
      }

      Capacity augment = std::numeric_limits<Capacity>::max();
      FlowCost path_cost = 0;
      for (Vertex vertex = sink; vertex != source;) {
        if (!parent[vertex].has_value()) {
          throw std::logic_error("min-cost-flow parent chain is incomplete");
        }
        const ParentArc step = *parent[vertex];
        const auto& arc = adjacency_[step.from][step.index];
        augment = std::min(augment, arc.residual);
        path_cost = checked_add(
            path_cost, arc.cost,
            "min-cost-flow augmenting path cost is not representable");
        vertex = step.from;
      }
      if (augment <= 0) {
        throw std::logic_error("min-cost-flow selected a non-positive augment");
      }

      for (Vertex vertex = sink; vertex != source;) {
        const ParentArc step = *parent[vertex];
        auto& arc = adjacency_[step.from][step.index];
        arc.residual -= augment;
        auto& reverse = adjacency_[arc.to][arc.reverse_index];
        reverse.residual = checked_add(
            reverse.residual, augment,
            "min-cost-flow reverse residual is not representable");
        vertex = step.from;
      }

      total_flow = checked_add(total_flow, augment,
                               "min-cost-flow value is not representable");
      total_cost = checked_add(total_cost,
                               checked_flow_cost_product(augment, path_cost),
                               "min-cost-flow total cost is not representable");
    }

    return {total_flow, total_cost};
  }

  [[nodiscard]] Capacity residual_at(const ArcLocation& location) const {
    return adjacency_[location.from][location.index].residual;
  }

  [[nodiscard]] std::vector<bool> residual_reachable(Vertex source) const {
    std::vector<bool> seen(adjacency_.size(), false);
    std::queue<Vertex> queue;
    seen[source] = true;
    queue.push(source);
    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const auto& arc : adjacency_[vertex]) {
        if (arc.residual > 0 && !seen[arc.to]) {
          seen[arc.to] = true;
          queue.push(arc.to);
        }
      }
    }
    return seen;
  }

  [[nodiscard]] const std::vector<std::vector<ResidualArc>>& adjacency()
      const noexcept {
    return adjacency_;
  }

 private:
  std::vector<std::vector<ResidualArc>> adjacency_;
};

void validate_input(std::size_t vertex_count,
                    std::span<const CostCapacityEdge> edges, Vertex source,
                    Vertex sink) {
  if (source >= vertex_count || sink >= vertex_count) {
    throw std::out_of_range("min-cost-flow source or sink is out of range");
  }
  if (source == sink) {
    throw std::invalid_argument("min-cost-flow source and sink must differ");
  }
  for (const auto& edge : edges) {
    if (edge.from >= vertex_count || edge.to >= vertex_count) {
      throw std::out_of_range("min-cost-flow edge endpoint is out of range");
    }
    if (edge.capacity < 0) {
      throw std::invalid_argument(
          "min-cost-flow capacities must be non-negative");
    }
    if (edge.cost == std::numeric_limits<FlowCost>::min()) {
      throw std::invalid_argument(
          "min-cost-flow edge cost has no representable residual reverse");
    }
    if (edge.from == edge.to && edge.capacity > 0 && edge.cost < 0) {
      throw std::invalid_argument(
          "min-cost-flow input contains a negative-cost cycle");
    }
  }
}

}  // namespace

MinCostMaxFlowResult min_cost_max_flow(
    std::size_t vertex_count, std::span<const CostCapacityEdge> edges,
    Vertex source, Vertex sink) {
  validate_input(vertex_count, edges, source, sink);

  Solver solver(vertex_count);
  std::vector<ArcLocation> locations;
  locations.reserve(edges.size());
  for (const auto& edge : edges) {
    locations.push_back(
        solver.add_edge(edge.from, edge.to, edge.capacity, edge.cost));
  }

  const auto [value, cost] = solver.solve(source, sink);
  auto source_side = solver.residual_reachable(source);
  if (source_side[sink]) {
    throw std::logic_error("min-cost-flow terminated with a residual s-t path");
  }

  std::vector<MinCostFlowEdge> flows;
  flows.reserve(edges.size());
  Capacity cut_capacity = 0;
  std::vector<Capacity> balance(vertex_count, 0);
  FlowCost replay_cost = 0;
  for (std::size_t index = 0U; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const auto& location = locations[index];
    const Capacity flow = location.materialized
                              ? edge.capacity - solver.residual_at(location)
                              : 0;
    if (flow < 0 || flow > edge.capacity) {
      throw std::logic_error("min-cost-flow edge flow violates capacity");
    }
    flows.push_back(
        MinCostFlowEdge{edge.from, edge.to, edge.capacity, edge.cost, flow});
    balance[edge.from] = checked_subtract(
        balance[edge.from], flow,
        "min-cost-flow witness balance is not representable");
    balance[edge.to] = checked_add(
        balance[edge.to], flow,
        "min-cost-flow witness balance is not representable");
    replay_cost = checked_add(
        replay_cost, checked_flow_cost_product(flow, edge.cost),
        "min-cost-flow witness cost is not representable");
    if (source_side[edge.from] && !source_side[edge.to]) {
      cut_capacity = checked_add(
          cut_capacity, edge.capacity,
          "min-cost-flow cut capacity is not representable");
    }
  }

  if (cut_capacity != value) {
    throw std::logic_error("min-cost-flow max-flow/min-cut certificate mismatch");
  }
  if (balance[source] != -value || balance[sink] != value) {
    throw std::logic_error("min-cost-flow source/sink balance mismatch");
  }
  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (vertex != source && vertex != sink && balance[vertex] != 0) {
      throw std::logic_error("min-cost-flow conservation certificate mismatch");
    }
  }
  if (replay_cost != cost) {
    throw std::logic_error("min-cost-flow cost certificate mismatch");
  }

  auto residual_potential = solver.feasible_potentials(false);
  for (Vertex from = 0U; from < vertex_count; ++from) {
    for (const auto& arc : solver.adjacency()[from]) {
      if (arc.residual <= 0) {
        continue;
      }
      FlowCost reduced = checked_add(
          arc.cost, residual_potential[from],
          "min-cost-flow residual certificate is not representable");
      reduced = checked_subtract(
          reduced, residual_potential[arc.to],
          "min-cost-flow residual certificate is not representable");
      if (reduced < 0) {
        throw std::logic_error(
            "min-cost-flow final residual dual certificate is invalid");
      }
    }
  }

  return MinCostMaxFlowResult{value, cost, cut_capacity, std::move(flows),
                              std::move(source_side),
                              std::move(residual_potential)};
}

}  // namespace algorithms::graphs
