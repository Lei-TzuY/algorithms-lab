#include "algorithms/graphs/min_cost_flow.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::Capacity;
using algorithms::graphs::CostCapacityEdge;
using algorithms::graphs::FlowCost;
using algorithms::graphs::MinCostMaxFlowResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::min_cost_max_flow;

namespace {

struct OracleResult {
  Capacity value{-1};
  FlowCost cost{0};
};

OracleResult exhaustive_optimum(std::size_t vertex_count,
                                const std::vector<CostCapacityEdge>& edges,
                                Vertex source, Vertex sink) {
  OracleResult best;
  std::vector<Capacity> flow(edges.size(), 0);

  const auto search = [&](auto&& self, std::size_t index) -> void {
    if (index != edges.size()) {
      for (Capacity value = 0; value <= edges[index].capacity; ++value) {
        flow[index] = value;
        self(self, index + 1U);
      }
      return;
    }

    std::vector<Capacity> balance(vertex_count, 0);
    FlowCost cost = 0;
    for (std::size_t edge_index = 0U; edge_index < edges.size();
         ++edge_index) {
      const auto& edge = edges[edge_index];
      balance[edge.from] -= flow[edge_index];
      balance[edge.to] += flow[edge_index];
      cost += flow[edge_index] * edge.cost;
    }
    for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
      if (vertex != source && vertex != sink && balance[vertex] != 0) {
        return;
      }
    }
    if (balance[source] > 0) {
      return;
    }
    const Capacity value = -balance[source];
    if (balance[sink] != value) {
      return;
    }
    if (value > best.value || (value == best.value && cost < best.cost)) {
      best = OracleResult{value, cost};
    }
  };
  search(search, 0U);
  return best;
}

void replay_witness(std::size_t vertex_count,
                    const MinCostMaxFlowResult& result, Vertex source,
                    Vertex sink) {
  std::vector<Capacity> balance(vertex_count, 0);
  FlowCost cost = 0;
  Capacity cut = 0;
  for (const auto& edge : result.edges) {
    REQUIRE(edge.flow >= 0);
    REQUIRE(edge.flow <= edge.capacity);
    balance[edge.from] -= edge.flow;
    balance[edge.to] += edge.flow;
    cost += edge.flow * edge.cost;
    if (result.source_side_min_cut[edge.from] &&
        !result.source_side_min_cut[edge.to]) {
      cut += edge.capacity;
    }
  }
  REQUIRE_EQ(balance[source], -result.value);
  REQUIRE_EQ(balance[sink], result.value);
  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (vertex != source && vertex != sink) {
      REQUIRE_EQ(balance[vertex], 0);
    }
  }
  REQUIRE_EQ(cost, result.cost);
  REQUIRE_EQ(cut, result.cut_capacity);
  REQUIRE_EQ(cut, result.value);
  REQUIRE_EQ(result.residual_potential.size(), vertex_count);
  for (const auto& edge : result.edges) {
    if (edge.flow < edge.capacity) {
      const FlowCost reduced = edge.cost + result.residual_potential[edge.from] -
                               result.residual_potential[edge.to];
      REQUIRE(reduced >= 0);
    }
    if (edge.flow > 0) {
      const FlowCost reverse_reduced =
          -edge.cost + result.residual_potential[edge.to] -
          result.residual_potential[edge.from];
      REQUIRE(reverse_reduced >= 0);
    }
  }
}

}  // namespace

TEST_CASE(min_cost_max_flow_deterministic_negative_cost_and_parallel_edges) {
  const std::vector<CostCapacityEdge> edges{
      {0U, 1U, 2, -4}, {1U, 3U, 2, 1}, {0U, 2U, 1, 2},
      {2U, 3U, 1, 1},  {0U, 1U, 1, 7}, {1U, 3U, 1, 7}};
  const auto result = min_cost_max_flow(4U, edges, 0U, 3U);
  REQUIRE_EQ(result.value, 4);
  REQUIRE_EQ(result.cost, 11);
  replay_witness(4U, result, 0U, 3U);
}

TEST_CASE(min_cost_max_flow_residual_rerouting_and_exact_cost_minimum) {
  const std::vector<CostCapacityEdge> edges{
      {0U, 1U, 1, 0}, {0U, 2U, 1, 0}, {1U, 2U, 1, -5},
      {1U, 3U, 1, 0}, {2U, 3U, 1, 0}};
  const auto result = min_cost_max_flow(4U, edges, 0U, 3U);
  REQUIRE_EQ(result.value, 2);
  REQUIRE_EQ(result.cost, 0);
  REQUIRE_EQ(result.edges[2].flow, 0);
  replay_witness(4U, result, 0U, 3U);
}

TEST_CASE(min_cost_max_flow_validation_negative_cycle_and_overflow) {
  REQUIRE_THROWS_AS(min_cost_max_flow(2U, {}, 0U, 0U),
                    std::invalid_argument);
  const std::vector<CostCapacityEdge> negative_capacity{{0U, 1U, -1, 0}};
  REQUIRE_THROWS_AS(min_cost_max_flow(2U, negative_capacity, 0U, 1U),
                    std::invalid_argument);

  const std::vector<CostCapacityEdge> minimum_cost{{
      0U, 1U, 1, std::numeric_limits<FlowCost>::min()}};
  REQUIRE_THROWS_AS(min_cost_max_flow(2U, minimum_cost, 0U, 1U),
                    std::invalid_argument);

  const std::vector<CostCapacityEdge> negative_cycle{
      {0U, 1U, 1, 0}, {1U, 2U, 1, -2},
      {2U, 1U, 1, 1}, {2U, 3U, 1, 0}};
  REQUIRE_THROWS_AS(min_cost_max_flow(4U, negative_cycle, 0U, 3U),
                    std::invalid_argument);

  const std::vector<CostCapacityEdge> disconnected_negative_cycle{
      {0U, 4U, 1, 0}, {1U, 2U, 1, -2}, {2U, 1U, 1, 1}};
  REQUIRE_THROWS_AS(
      min_cost_max_flow(5U, disconnected_negative_cycle, 0U, 4U),
      std::invalid_argument);

  const std::vector<CostCapacityEdge> zero_capacity_cycle{
      {0U, 3U, 1, 2}, {1U, 2U, 0, -10}, {2U, 1U, 0, 0}};
  const auto zero_cycle =
      min_cost_max_flow(4U, zero_capacity_cycle, 0U, 3U);
  REQUIRE_EQ(zero_cycle.value, 1);
  REQUIRE_EQ(zero_cycle.cost, 2);

  const std::vector<CostCapacityEdge> exact_minimum_total{{
      0U, 1U, 2, std::numeric_limits<FlowCost>::min() / 2}};
  const auto exact_minimum =
      min_cost_max_flow(2U, exact_minimum_total, 0U, 1U);
  REQUIRE_EQ(exact_minimum.value, 2);
  REQUIRE_EQ(exact_minimum.cost, std::numeric_limits<FlowCost>::min());
  replay_witness(2U, exact_minimum, 0U, 1U);

  const std::vector<CostCapacityEdge> overflowing_cost{{
      0U, 1U, std::numeric_limits<Capacity>::max(), 2}};
  REQUIRE_THROWS_AS(min_cost_max_flow(2U, overflowing_cost, 0U, 1U),
                    std::overflow_error);
}

TEST_CASE(min_cost_max_flow_randomized_against_exhaustive_flow_assignments) {
  std::mt19937_64 rng(0xC057F10ULL);
  std::uniform_int_distribution<std::size_t> vertex_dist(2U, 5U);
  std::uniform_int_distribution<Capacity> capacity_dist(0, 2);
  std::uniform_int_distribution<FlowCost> cost_dist(-5, 8);

  for (std::size_t trial = 0U; trial < 300U; ++trial) {
    const std::size_t vertex_count = vertex_dist(rng);
    std::vector<std::pair<Vertex, Vertex>> possible;
    for (Vertex from = 0U; from < vertex_count; ++from) {
      for (Vertex to = from + 1U; to < vertex_count; ++to) {
        possible.emplace_back(from, to);
      }
    }

    std::uniform_int_distribution<std::size_t> edge_count_dist(
        0U, std::min<std::size_t>(7U, possible.size() + 2U));
    const std::size_t edge_count = edge_count_dist(rng);
    std::vector<CostCapacityEdge> edges;
    edges.reserve(edge_count);
    if (!possible.empty()) {
      std::uniform_int_distribution<std::size_t> pair_dist(
          0U, possible.size() - 1U);
      for (std::size_t index = 0U; index < edge_count; ++index) {
        const auto [from, to] = possible[pair_dist(rng)];
        edges.push_back(
            CostCapacityEdge{from, to, capacity_dist(rng), cost_dist(rng)});
      }
    }

    const auto expected =
        exhaustive_optimum(vertex_count, edges, 0U, vertex_count - 1U);
    const auto actual =
        min_cost_max_flow(vertex_count, edges, 0U, vertex_count - 1U);
    REQUIRE_EQ(actual.value, expected.value);
    REQUIRE_EQ(actual.cost, expected.cost);
    replay_witness(vertex_count, actual, 0U, vertex_count - 1U);
  }
}
