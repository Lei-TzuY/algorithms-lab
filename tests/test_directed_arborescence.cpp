#include "algorithms/graphs/directed_arborescence.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::DirectedArborescenceEdge;
using algorithms::graphs::MinimumArborescenceResult;
using algorithms::graphs::chu_liu_edmonds_minimum_arborescence;

bool valid_witness(std::size_t vertex_count, std::size_t root,
                   const std::vector<DirectedArborescenceEdge>& edges,
                   const MinimumArborescenceResult& result,
                   std::int64_t expected_cost) {
  if (result.incoming_edge_index.size() != vertex_count ||
      result.edge_indices.size() != vertex_count - 1U ||
      result.incoming_edge_index[root].has_value() ||
      result.total_cost != expected_cost) {
    return false;
  }
  std::vector<std::vector<std::size_t>> outgoing(vertex_count);
  std::int64_t total = 0;
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex == root) continue;
    if (!result.incoming_edge_index[vertex].has_value()) return false;
    const std::size_t edge_index = *result.incoming_edge_index[vertex];
    if (edge_index >= edges.size() || edges[edge_index].to != vertex ||
        edges[edge_index].from == vertex) return false;
    outgoing[edges[edge_index].from].push_back(vertex);
    total += edges[edge_index].cost;
  }
  if (total != expected_cost) return false;
  std::vector<bool> seen(vertex_count, false);
  std::vector<std::size_t> stack{root};
  seen[root] = true;
  while (!stack.empty()) {
    const std::size_t from = stack.back();
    stack.pop_back();
    for (const std::size_t to : outgoing[from]) {
      if (!seen[to]) { seen[to] = true; stack.push_back(to); }
    }
  }
  return std::find(seen.begin(), seen.end(), false) == seen.end();
}

struct OracleResult { bool exists = false; std::int64_t cost = 0; };

OracleResult exhaustive_arborescence(
    std::size_t vertex_count, std::size_t root,
    const std::vector<DirectedArborescenceEdge>& edges) {
  std::vector<std::vector<std::size_t>> choices(vertex_count);
  for (std::size_t edge_index = 0; edge_index < edges.size(); ++edge_index) {
    const auto& edge = edges[edge_index];
    if (edge.from != edge.to && edge.to != root) choices[edge.to].push_back(edge_index);
  }
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != root && choices[vertex].empty()) return {};
  }

  OracleResult best;
  std::vector<std::size_t> incoming(vertex_count,
      std::numeric_limits<std::size_t>::max());
  const auto recurse = [&](auto&& self, std::size_t vertex) -> void {
    while (vertex < vertex_count && vertex == root) ++vertex;
    if (vertex == vertex_count) {
      std::vector<std::vector<std::size_t>> outgoing(vertex_count);
      std::int64_t total = 0;
      for (std::size_t target = 0; target < vertex_count; ++target) {
        if (target == root) continue;
        const auto& edge = edges[incoming[target]];
        outgoing[edge.from].push_back(target);
        total += edge.cost;
      }
      std::vector<bool> seen(vertex_count, false);
      std::vector<std::size_t> stack{root};
      seen[root] = true;
      while (!stack.empty()) {
        const std::size_t from = stack.back();
        stack.pop_back();
        for (const std::size_t to : outgoing[from]) {
          if (!seen[to]) { seen[to] = true; stack.push_back(to); }
        }
      }
      if (std::find(seen.begin(), seen.end(), false) != seen.end()) return;
      if (!best.exists || total < best.cost) { best.exists = true; best.cost = total; }
      return;
    }
    for (const std::size_t edge_index : choices[vertex]) {
      incoming[vertex] = edge_index;
      self(self, vertex + 1U);
    }
  };
  recurse(recurse, 0U);
  return best;
}

TEST_CASE(directed_arborescence_acyclic_ties_parallel_and_self_loops) {
  const std::vector<DirectedArborescenceEdge> edges{
      {0, 0, -99}, {0, 1, 2}, {0, 1, 2}, {0, 2, 10}, {1, 2, 3}, {2, 1, 8}};
  const auto result = chu_liu_edmonds_minimum_arborescence(3, edges, 0);
  REQUIRE_EQ(result.total_cost, 5);
  REQUIRE_EQ(result.edge_indices, std::vector<std::size_t>({1, 4}));
  REQUIRE(valid_witness(3, 0, edges, result, 5));

  const auto singleton = chu_liu_edmonds_minimum_arborescence(
      1, std::span<const DirectedArborescenceEdge>{}, 0);
  REQUIRE_EQ(singleton.total_cost, 0);
  REQUIRE(singleton.edge_indices.empty());
  REQUIRE_EQ(singleton.incoming_edge_index.size(), 1U);
}

TEST_CASE(directed_arborescence_cycle_and_nested_cycle_expansion) {
  {
    const std::vector<DirectedArborescenceEdge> edges{
        {2, 1, 0}, {1, 2, 0}, {0, 1, 4}, {0, 2, 1}};
    const auto result = chu_liu_edmonds_minimum_arborescence(3, edges, 0);
    REQUIRE_EQ(result.total_cost, 1);
    REQUIRE(valid_witness(3, 0, edges, result, 1));
  }
  {
    const std::vector<DirectedArborescenceEdge> edges{
        {2, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 1, 0}, {0, 3, 1}, {0, 1, 5}};
    const auto result = chu_liu_edmonds_minimum_arborescence(4, edges, 0);
    REQUIRE_EQ(result.total_cost, 1);
    REQUIRE_EQ(result.incoming_edge_index[1], std::optional<std::size_t>{3});
    REQUIRE_EQ(result.incoming_edge_index[2], std::optional<std::size_t>{1});
    REQUIRE_EQ(result.incoming_edge_index[3], std::optional<std::size_t>{4});
    REQUIRE(valid_witness(4, 0, edges, result, 1));
  }
}

TEST_CASE(directed_arborescence_validation_and_unreachable) {
  const std::vector<DirectedArborescenceEdge> edges{{0, 1, 1}};
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(0, edges, 0), std::out_of_range);
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(2, edges, 2), std::out_of_range);
  const std::vector<DirectedArborescenceEdge> bad_endpoint{{0, 2, 0}};
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(2, bad_endpoint, 0), std::out_of_range);
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(3, edges, 0), std::invalid_argument);
}

TEST_CASE(directed_arborescence_exact_internal_reduced_costs) {
  using Limits = std::numeric_limits<std::int64_t>;
  const std::vector<DirectedArborescenceEdge> edges{
      {2, 1, Limits::min()}, {1, 2, 0}, {0, 1, Limits::max()}, {0, 2, 5}};
  const auto result = chu_liu_edmonds_minimum_arborescence(3, edges, 0);
  REQUIRE_EQ(result.total_cost, Limits::min() + 5);
  REQUIRE_EQ(result.incoming_edge_index[1], std::optional<std::size_t>{0});
  REQUIRE_EQ(result.incoming_edge_index[2], std::optional<std::size_t>{3});
}

TEST_CASE(directed_arborescence_total_cost_overflow) {
  using Limits = std::numeric_limits<std::int64_t>;
  const std::vector<DirectedArborescenceEdge> positive{
      {0, 1, Limits::max()}, {0, 2, 1}};
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(3, positive, 0), std::overflow_error);
  const std::vector<DirectedArborescenceEdge> negative{
      {0, 1, Limits::min()}, {0, 2, -1}};
  REQUIRE_THROWS_AS(chu_liu_edmonds_minimum_arborescence(3, negative, 0), std::overflow_error);
}

TEST_CASE(directed_arborescence_randomized_differential) {
  std::mt19937_64 rng(0x42ED0D5ULL);
  for (std::size_t trial = 0; trial < 600U; ++trial) {
    const std::size_t vertex_count = 2U + static_cast<std::size_t>(rng() % 5U);
    const std::size_t root = static_cast<std::size_t>(rng() % vertex_count);
    std::vector<DirectedArborescenceEdge> edges;
    std::vector<std::size_t> order;
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      if (vertex != root) order.push_back(vertex);
    }
    std::shuffle(order.begin(), order.end(), rng);
    std::vector<std::size_t> reached{root};
    for (const std::size_t vertex : order) {
      const std::size_t from = reached[static_cast<std::size_t>(rng() % reached.size())];
      edges.push_back({from, vertex,
          static_cast<std::int64_t>(static_cast<int>(rng() % 21U) - 10)});
      reached.push_back(vertex);
    }
    const std::size_t extra_edges = static_cast<std::size_t>(rng() % 8U);
    for (std::size_t i = 0; i < extra_edges; ++i) {
      edges.push_back({static_cast<std::size_t>(rng() % vertex_count),
                       static_cast<std::size_t>(rng() % vertex_count),
                       static_cast<std::int64_t>(static_cast<int>(rng() % 21U) - 10)});
    }
    const OracleResult oracle = exhaustive_arborescence(vertex_count, root, edges);
    REQUIRE(oracle.exists);
    const auto result = chu_liu_edmonds_minimum_arborescence(vertex_count, edges, root);
    REQUIRE_EQ(result.total_cost, oracle.cost);
    REQUIRE(valid_witness(vertex_count, root, edges, result, oracle.cost));
  }
}
}  // namespace
