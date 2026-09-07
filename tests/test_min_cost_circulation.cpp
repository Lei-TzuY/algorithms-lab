#include "algorithms/graphs/min_cost_circulation.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::Capacity;
using algorithms::graphs::CirculationCost;
using algorithms::graphs::LowerBoundCostEdge;
using algorithms::graphs::MinCostCirculationResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::min_cost_circulation;

struct OracleResult {
  CirculationCost cost;
  std::vector<Capacity> flow;
};

void enumerate_flows(std::span<const LowerBoundCostEdge> edges,
                     std::span<const Capacity> demand, std::size_t index,
                     std::vector<Capacity>& flow,
                     std::optional<OracleResult>& best) {
  if (index == edges.size()) {
    std::vector<Capacity> net(demand.size(), 0);
    CirculationCost cost = 0;
    for (std::size_t i = 0; i < edges.size(); ++i) {
      const auto& edge = edges[i];
      net[edge.to] += flow[i];
      net[edge.from] -= flow[i];
      cost += flow[i] * edge.cost;
    }
    if (!std::equal(net.begin(), net.end(), demand.begin(), demand.end())) {
      return;
    }
    if (!best.has_value() || cost < best->cost) {
      best = OracleResult{cost, flow};
    }
    return;
  }
  for (Capacity value = edges[index].lower; value <= edges[index].upper;
       ++value) {
    flow[index] = value;
    enumerate_flows(edges, demand, index + 1U, flow, best);
    if (value == std::numeric_limits<Capacity>::max()) {
      break;
    }
  }
}

[[nodiscard]] std::optional<OracleResult> exhaustive_oracle(
    std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> demand) {
  std::vector<Capacity> flow(edges.size(), 0);
  std::optional<OracleResult> best;
  enumerate_flows(edges, demand, 0U, flow, best);
  return best;
}

void verify_small_certificate(std::span<const LowerBoundCostEdge> input,
                              std::span<const Capacity> demand,
                              const MinCostCirculationResult& result) {
  REQUIRE_EQ(result.edges.size(), input.size());
  REQUIRE_EQ(result.residual_potential.size(), demand.size());
  std::vector<Capacity> net(demand.size(), 0);
  CirculationCost total = 0;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const auto& edge = input[i];
    const auto& actual = result.edges[i];
    REQUIRE_EQ(actual.from, edge.from);
    REQUIRE_EQ(actual.to, edge.to);
    REQUIRE_EQ(actual.lower, edge.lower);
    REQUIRE_EQ(actual.upper, edge.upper);
    REQUIRE_EQ(actual.cost, edge.cost);
    REQUIRE(actual.flow >= edge.lower);
    REQUIRE(actual.flow <= edge.upper);
    net[edge.to] += actual.flow;
    net[edge.from] -= actual.flow;
    total += actual.flow * edge.cost;

    if (actual.flow < edge.upper) {
      const CirculationCost reduced =
          edge.cost + result.residual_potential[edge.from] -
          result.residual_potential[edge.to];
      REQUIRE(reduced >= 0);
    }
    if (actual.flow > edge.lower) {
      const CirculationCost reduced =
          -edge.cost + result.residual_potential[edge.to] -
          result.residual_potential[edge.from];
      REQUIRE(reduced >= 0);
    }
  }
  REQUIRE(std::equal(net.begin(), net.end(), demand.begin(), demand.end()));
  REQUIRE_EQ(total, result.cost);
}

TEST_CASE(lower_bounds_and_demands_are_respected) {
  const std::vector<LowerBoundCostEdge> edges{{0U, 1U, 1, 3, 2}};
  const std::vector<Capacity> demand{-2, 2};
  const auto result = min_cost_circulation(2U, edges, demand);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->cost, 4);
  REQUIRE_EQ(result->edges[0].flow, 2);
  verify_small_certificate(edges, demand, *result);
}

TEST_CASE(negative_residual_cycles_are_cancelled) {
  {
    const std::vector<LowerBoundCostEdge> edges{
        {0U, 1U, 0, 2, -3}, {1U, 0U, 0, 2, 1}};
    const std::vector<Capacity> demand{0, 0};
    const auto result = min_cost_circulation(2U, edges, demand);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->cost, -4);
    REQUIRE_EQ(result->edges[0].flow, 2);
    REQUIRE_EQ(result->edges[1].flow, 2);
    verify_small_certificate(edges, demand, *result);
  }
  {
    const std::vector<LowerBoundCostEdge> edges{{0U, 0U, 0, 3, -5}};
    const std::vector<Capacity> demand{0};
    const auto result = min_cost_circulation(1U, edges, demand);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->cost, -15);
    REQUIRE_EQ(result->edges[0].flow, 3);
    verify_small_certificate(edges, demand, *result);
  }
}

TEST_CASE(infeasibility_and_input_validation_are_explicit) {
  const std::vector<LowerBoundCostEdge> no_edges;
  const std::vector<Capacity> no_demand;
  const auto empty = min_cost_circulation(0U, no_edges, no_demand);
  REQUIRE(empty.has_value());
  REQUIRE_EQ(empty->cost, 0);

  const std::vector<Capacity> impossible{-1, 1};
  REQUIRE(!min_cost_circulation(2U, no_edges, impossible).has_value());

  const std::vector<Capacity> one_demand{0};
  const std::vector<LowerBoundCostEdge> bad_bounds{{0U, 0U, 2, 1, 0}};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, bad_bounds, one_demand),
                    std::invalid_argument);
  const std::vector<LowerBoundCostEdge> negative_lower{{0U, 0U, -1, 1, 0}};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, negative_lower, one_demand),
                    std::invalid_argument);
  const std::vector<LowerBoundCostEdge> bad_vertex{{0U, 1U, 0, 1, 0}};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, bad_vertex, one_demand),
                    std::out_of_range);
  const std::vector<Capacity> wrong_size;
  REQUIRE_THROWS_AS(min_cost_circulation(1U, no_edges, wrong_size),
                    std::invalid_argument);
  const std::vector<Capacity> bad_sum{1};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, no_edges, bad_sum),
                    std::invalid_argument);
  const std::vector<LowerBoundCostEdge> min_cost{{
      0U, 0U, 0, 1, std::numeric_limits<CirculationCost>::min()}};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, min_cost, one_demand),
                    std::invalid_argument);
}

TEST_CASE(total_cost_boundaries_are_checked) {
  {
    const std::vector<LowerBoundCostEdge> huge_self_loop{{
        0U, 0U, std::numeric_limits<Capacity>::max(),
        std::numeric_limits<Capacity>::max(), 0}};
    const std::vector<Capacity> zero_demand{0};
    const auto huge = min_cost_circulation(1U, huge_self_loop, zero_demand);
    REQUIRE(huge.has_value());
    REQUIRE_EQ(huge->edges[0].flow, std::numeric_limits<Capacity>::max());
  }
  const CirculationCost half_min =
      std::numeric_limits<CirculationCost>::min() / 2;
  const std::vector<LowerBoundCostEdge> exact_min{{0U, 0U, 2, 2, half_min}};
  const std::vector<Capacity> demand{0};
  const auto result = min_cost_circulation(1U, exact_min, demand);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->cost, std::numeric_limits<CirculationCost>::min());

  const CirculationCost too_large =
      std::numeric_limits<CirculationCost>::max() / 2 + 1;
  const std::vector<LowerBoundCostEdge> overflow{{0U, 0U, 2, 2, too_large}};
  REQUIRE_THROWS_AS(min_cost_circulation(1U, overflow, demand),
                    std::overflow_error);
}

TEST_CASE(randomized_circulation_matches_exhaustive_oracle) {
  std::mt19937_64 rng(0xC1A0C1A7ULL);
  std::uniform_int_distribution<int> vertex_dist(1, 4);
  std::uniform_int_distribution<int> edge_count_dist(0, 6);
  std::uniform_int_distribution<int> lower_dist(0, 1);
  std::uniform_int_distribution<int> extra_dist(0, 2);
  std::uniform_int_distribution<int> cost_dist(-4, 5);
  std::uniform_int_distribution<int> demand_dist(-2, 2);

  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_dist(rng));
    std::uniform_int_distribution<std::size_t> endpoint_dist(0U,
                                                             vertex_count - 1U);
    const std::size_t edge_count =
        static_cast<std::size_t>(edge_count_dist(rng));
    std::vector<LowerBoundCostEdge> edges;
    edges.reserve(edge_count);
    for (std::size_t i = 0; i < edge_count; ++i) {
      const Capacity lower = lower_dist(rng);
      const Capacity upper = lower + extra_dist(rng);
      edges.push_back({endpoint_dist(rng), endpoint_dist(rng), lower, upper,
                       static_cast<CirculationCost>(cost_dist(rng))});
    }

    std::vector<Capacity> demand(vertex_count, 0);
    if ((trial % 2) == 0) {
      for (const auto& edge : edges) {
        std::uniform_int_distribution<Capacity> flow_dist(edge.lower,
                                                          edge.upper);
        const Capacity flow = flow_dist(rng);
        demand[edge.to] += flow;
        demand[edge.from] -= flow;
      }
    } else if (vertex_count > 1U) {
      Capacity sum = 0;
      for (std::size_t v = 0; v + 1U < vertex_count; ++v) {
        demand[v] = demand_dist(rng);
        sum += demand[v];
      }
      demand.back() = -sum;
    }

    const auto oracle = exhaustive_oracle(edges, demand);
    const auto actual = min_cost_circulation(vertex_count, edges, demand);
    REQUIRE_EQ(actual.has_value(), oracle.has_value());
    if (actual.has_value()) {
      REQUIRE_EQ(actual->cost, oracle->cost);
      verify_small_certificate(edges, demand, *actual);
    }
  }
}

}  // namespace
