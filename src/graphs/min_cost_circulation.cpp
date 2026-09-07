#include "algorithms/graphs/min_cost_circulation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct ResidualArc {
  Vertex from;
  Vertex to;
  std::size_t edge_index;
  bool forward;
  Capacity residual;
  CirculationCost cost;
};

[[nodiscard]] std::int64_t checked_add(std::int64_t left, std::int64_t right,
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

[[nodiscard]] CirculationCost checked_product(Capacity flow,
                                              CirculationCost cost) {
  if (flow == 0 || cost == 0) return 0;
  if (flow < 0 || cost == std::numeric_limits<CirculationCost>::min()) {
    throw std::logic_error("invalid circulation cost product operands");
  }
  const std::uint64_t magnitude = cost < 0
      ? static_cast<std::uint64_t>(-cost)
      : static_cast<std::uint64_t>(cost);
  const std::uint64_t unsigned_flow = static_cast<std::uint64_t>(flow);
  const std::uint64_t positive_limit =
      static_cast<std::uint64_t>(std::numeric_limits<CirculationCost>::max());
  const std::uint64_t magnitude_limit =
      cost < 0 ? positive_limit + std::uint64_t{1} : positive_limit;
  if (unsigned_flow > magnitude_limit / magnitude) {
    throw std::overflow_error("circulation total cost is not representable");
  }
  const std::uint64_t product = unsigned_flow * magnitude;
  if (cost >= 0) return static_cast<CirculationCost>(product);
  if (product == positive_limit + std::uint64_t{1}) {
    return std::numeric_limits<CirculationCost>::min();
  }
  return -static_cast<CirculationCost>(product);
}

void validate_input(std::size_t vertex_count,
                    std::span<const LowerBoundCostEdge> edges,
                    std::span<const Capacity> demand) {
  if (demand.size() != vertex_count) {
    throw std::invalid_argument("circulation demand size must equal vertex count");
  }
  Capacity demand_sum = 0;
  for (Capacity value : demand) {
    demand_sum = checked_add(demand_sum, value,
                             "circulation demand sum is not representable");
  }
  if (demand_sum != 0) {
    throw std::invalid_argument("circulation demands must sum to zero");
  }
  for (const auto& edge : edges) {
    if (edge.from >= vertex_count || edge.to >= vertex_count) {
      throw std::out_of_range("circulation edge endpoint is out of range");
    }
    if (edge.lower < 0 || edge.upper < edge.lower) {
      throw std::invalid_argument("circulation requires 0 <= lower <= upper");
    }
    if (edge.cost == std::numeric_limits<CirculationCost>::min()) {
      throw std::invalid_argument("circulation edge cost INT64_MIN is unsupported");
    }
  }
  if (vertex_count > std::numeric_limits<std::size_t>::max() - 2U) {
    throw std::length_error("circulation auxiliary vertex count overflows size_t");
  }
}

[[nodiscard]] std::vector<ResidualArc> build_residual(
    std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> flow) {
  std::vector<ResidualArc> residual;
  for (std::size_t i = 0; i < edges.size(); ++i) {
    const auto& edge = edges[i];
    const Capacity forward = edge.upper - flow[i];
    const Capacity reverse = flow[i] - edge.lower;
    if (forward > 0) {
      residual.push_back({edge.from, edge.to, i, true, forward, edge.cost});
    }
    if (reverse > 0) {
      residual.push_back({edge.to, edge.from, i, false, reverse, -edge.cost});
    }
  }
  return residual;
}

struct NegativeCycleSearch {
  std::vector<ResidualArc> arcs;
  std::vector<CirculationCost> potential;
  std::optional<std::vector<std::size_t>> cycle;
};

[[nodiscard]] NegativeCycleSearch find_negative_cycle(
    std::size_t vertex_count, std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> flow) {
  auto arcs = build_residual(edges, flow);
  std::vector<CirculationCost> distance(vertex_count, 0);
  std::vector<std::optional<std::size_t>> parent_arc(vertex_count);
  std::optional<Vertex> updated;

  for (std::size_t pass = 0; pass < vertex_count; ++pass) {
    updated.reset();
    for (std::size_t arc_index = 0; arc_index < arcs.size(); ++arc_index) {
      const auto& arc = arcs[arc_index];
      const CirculationCost candidate = checked_add(
          distance[arc.from], arc.cost,
          "circulation Bellman-Ford distance is not representable");
      if (candidate < distance[arc.to]) {
        distance[arc.to] = candidate;
        parent_arc[arc.to] = arc_index;
        updated = arc.to;
      }
    }
    if (!updated.has_value()) {
      return {std::move(arcs), std::move(distance), std::nullopt};
    }
  }

  if (vertex_count == 0U || !updated.has_value()) {
    return {std::move(arcs), std::move(distance), std::nullopt};
  }

  Vertex inside = *updated;
  for (std::size_t step = 0; step < vertex_count; ++step) {
    if (!parent_arc[inside].has_value()) {
      throw std::logic_error("negative-cycle parent chain is incomplete");
    }
    inside = arcs[*parent_arc[inside]].from;
  }

  std::vector<std::size_t> cycle;
  Vertex vertex = inside;
  do {
    if (!parent_arc[vertex].has_value()) {
      throw std::logic_error("negative-cycle reconstruction is incomplete");
    }
    const std::size_t arc_index = *parent_arc[vertex];
    cycle.push_back(arc_index);
    vertex = arcs[arc_index].from;
    if (cycle.size() > arcs.size() + 1U) {
      throw std::logic_error("negative-cycle reconstruction did not close");
    }
  } while (vertex != inside);

  CirculationCost cycle_cost = 0;
  for (std::size_t arc_index : cycle) {
    cycle_cost = checked_add(cycle_cost, arcs[arc_index].cost,
                             "circulation cycle cost is not representable");
  }
  if (cycle_cost >= 0) {
    throw std::logic_error("Bellman-Ford reconstructed a non-negative cycle");
  }
  return {std::move(arcs), std::move(distance), std::move(cycle)};
}

[[nodiscard]] CirculationCost verify_and_cost(
    std::size_t vertex_count, std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> demand, std::span<const Capacity> flow,
    std::span<const CirculationCost> potential) {
  std::vector<Capacity> net(vertex_count, 0);
  CirculationCost total_cost = 0;
  for (std::size_t i = 0; i < edges.size(); ++i) {
    const auto& edge = edges[i];
    if (flow[i] < edge.lower || flow[i] > edge.upper) {
      throw std::logic_error("circulation flow violates edge bounds");
    }
    net[edge.to] = checked_add(net[edge.to], flow[i],
                               "circulation vertex balance is not representable");
    net[edge.from] = checked_subtract(net[edge.from], flow[i],
                                      "circulation vertex balance is not representable");
    total_cost = checked_add(total_cost, checked_product(flow[i], edge.cost),
                             "circulation total cost is not representable");
  }
  for (Vertex v = 0; v < vertex_count; ++v) {
    if (net[v] != demand[v]) {
      throw std::logic_error("circulation witness violates vertex demand");
    }
  }

  const auto residual = build_residual(edges, flow);
  if (potential.size() != vertex_count) {
    throw std::logic_error("circulation potential has wrong size");
  }
  for (const auto& arc : residual) {
    CirculationCost reduced = checked_add(
        arc.cost, potential[arc.from],
        "circulation reduced cost is not representable");
    reduced = checked_subtract(reduced, potential[arc.to],
                               "circulation reduced cost is not representable");
    if (reduced < 0) {
      throw std::logic_error("circulation residual potential is not feasible");
    }
  }
  return total_cost;
}

}  // namespace

std::optional<MinCostCirculationResult> min_cost_circulation(
    std::size_t vertex_count, std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> demand) {
  validate_input(vertex_count, edges, demand);
  if (vertex_count == 0U) {
    return MinCostCirculationResult{0, {}, {}};
  }

  std::vector<Capacity> need(demand.begin(), demand.end());
  std::vector<CapacityEdge> feasibility_edges;
  feasibility_edges.reserve(edges.size() + vertex_count);
  for (const auto& edge : edges) {
    feasibility_edges.push_back({edge.from, edge.to, edge.upper - edge.lower});
    if (edge.from != edge.to) {
      need[edge.to] = checked_subtract(
          need[edge.to], edge.lower,
          "circulation lower-bound balance is not representable");
      need[edge.from] = checked_add(
          need[edge.from], edge.lower,
          "circulation lower-bound balance is not representable");
    }
  }

  const Vertex super_source = vertex_count;
  const Vertex super_sink = vertex_count + 1U;
  Capacity required = 0;
  for (Vertex v = 0; v < vertex_count; ++v) {
    if (need[v] < 0) {
      if (need[v] == std::numeric_limits<Capacity>::min()) {
        throw std::overflow_error("circulation auxiliary capacity is not representable");
      }
      feasibility_edges.push_back({super_source, v, -need[v]});
    } else if (need[v] > 0) {
      feasibility_edges.push_back({v, super_sink, need[v]});
      required = checked_add(required, need[v],
                             "circulation required flow is not representable");
    }
  }

  const auto feasible = dinic_max_flow(vertex_count + 2U, feasibility_edges,
                                       super_source, super_sink);
  if (feasible.value != required) {
    return std::nullopt;
  }

  std::vector<Capacity> flow(edges.size(), 0);
  for (std::size_t i = 0; i < edges.size(); ++i) {
    flow[i] = checked_add(edges[i].lower, feasible.edges[i].flow,
                          "circulation feasible flow is not representable");
  }

  std::vector<CirculationCost> final_potential(vertex_count, 0);
  while (true) {
    auto search = find_negative_cycle(vertex_count, edges, flow);
    if (!search.cycle.has_value()) {
      final_potential = std::move(search.potential);
      break;
    }
    Capacity augment = std::numeric_limits<Capacity>::max();
    for (std::size_t arc_index : *search.cycle) {
      augment = std::min(augment, search.arcs[arc_index].residual);
    }
    if (augment <= 0) {
      throw std::logic_error("circulation selected a non-positive cycle augment");
    }
    for (std::size_t arc_index : *search.cycle) {
      const auto& arc = search.arcs[arc_index];
      if (arc.forward) {
        flow[arc.edge_index] = checked_add(
            flow[arc.edge_index], augment,
            "circulation forward cycle augmentation is not representable");
      } else {
        flow[arc.edge_index] = checked_subtract(
            flow[arc.edge_index], augment,
            "circulation reverse cycle augmentation is not representable");
      }
    }
  }

  const CirculationCost total_cost =
      verify_and_cost(vertex_count, edges, demand, flow, final_potential);
  std::vector<MinCostCirculationEdge> result_edges;
  result_edges.reserve(edges.size());
  for (std::size_t i = 0; i < edges.size(); ++i) {
    result_edges.push_back({edges[i].from, edges[i].to, edges[i].lower,
                            edges[i].upper, edges[i].cost, flow[i]});
  }
  return MinCostCirculationResult{total_cost, std::move(result_edges),
                                  std::move(final_potential)};
}

}  // namespace algorithms::graphs
