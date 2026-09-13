#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "algorithms/graphs/maximum_weight_closure.hpp"
#include "test_framework.hpp"

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

[[nodiscard]] bool subset_is_closed(const Graph& graph, std::uint64_t mask) {
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    if ((mask & (std::uint64_t{1} << from)) == 0U) {
      continue;
    }
    for (const auto& edge : graph.neighbors(from)) {
      if ((mask & (std::uint64_t{1} << edge.to)) == 0U) {
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] std::int64_t exhaustive_optimum(
    const Graph& graph, const std::vector<std::int64_t>& weights) {
  const std::uint64_t limit = std::uint64_t{1} << graph.vertex_count();
  std::int64_t best = 0;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    if (!subset_is_closed(graph, mask)) {
      continue;
    }
    std::int64_t total = 0;
    for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
      if ((mask & (std::uint64_t{1} << vertex)) != 0U) {
        total += weights[vertex];
      }
    }
    best = std::max(best, total);
  }
  return best;
}

[[nodiscard]] std::int64_t replay_weight(
    const std::vector<Vertex>& selected,
    const std::vector<std::int64_t>& weights) {
  std::int64_t total = 0;
  for (const Vertex vertex : selected) {
    total += weights[vertex];
  }
  return total;
}

[[nodiscard]] bool witness_is_closed(const Graph& graph,
                                     const std::vector<Vertex>& selected) {
  std::vector<bool> chosen(graph.vertex_count(), false);
  for (const Vertex vertex : selected) {
    if (vertex >= graph.vertex_count() || chosen[vertex]) {
      return false;
    }
    chosen[vertex] = true;
  }
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    if (!chosen[from]) {
      continue;
    }
    for (const auto& edge : graph.neighbors(from)) {
      if (!chosen[edge.to]) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

TEST_CASE(maximum_weight_closure_validation_and_trivial_cases) {
  Graph empty(0, true);
  const std::vector<std::int64_t> no_weights;
  const auto empty_result =
      algorithms::graphs::maximum_weight_closure(empty, no_weights);
  REQUIRE_EQ(empty_result.maximum_weight, 0);
  REQUIRE(empty_result.selected_vertices.empty());

  Graph undirected(2, false);
  const std::vector<std::int64_t> two_weights{1, 2};
  REQUIRE_THROWS_AS(
      algorithms::graphs::maximum_weight_closure(undirected, two_weights),
      std::invalid_argument);

  Graph directed(2, true);
  const std::vector<std::int64_t> wrong_size{1};
  REQUIRE_THROWS_AS(
      algorithms::graphs::maximum_weight_closure(directed, wrong_size),
      std::invalid_argument);

  const std::vector<std::int64_t> non_positive{
      std::numeric_limits<std::int64_t>::min(), 0};
  const auto non_positive_result =
      algorithms::graphs::maximum_weight_closure(directed, non_positive);
  REQUIRE_EQ(non_positive_result.maximum_weight, 0);
  REQUIRE(non_positive_result.selected_vertices.empty());
}

TEST_CASE(maximum_weight_closure_dependencies_cycles_and_ties) {
  Graph graph(4, true);
  graph.add_edge(0, 2, 91);
  graph.add_edge(1, 2, -77);
  graph.add_edge(2, 3, 5);
  graph.add_edge(0, 2, 13);  // parallel copy, ignored structurally
  graph.add_edge(3, 3, -9);  // self-loop, closure-neutral
  const std::vector<std::int64_t> weights{8, 5, -6, -4};
  const auto result = algorithms::graphs::maximum_weight_closure(graph, weights);
  REQUIRE_EQ(result.maximum_weight, 3);
  REQUIRE_EQ(result.selected_vertices,
             (std::vector<Vertex>{0, 1, 2, 3}));
  REQUIRE(witness_is_closed(graph, result.selected_vertices));

  Graph cycle(2, true);
  cycle.add_edge(0, 1, 1);
  cycle.add_edge(1, 0, 2);
  const std::vector<std::int64_t> cycle_weights{7, -3};
  const auto cycle_result =
      algorithms::graphs::maximum_weight_closure(cycle, cycle_weights);
  REQUIRE_EQ(cycle_result.maximum_weight, 4);
  REQUIRE_EQ(cycle_result.selected_vertices, (std::vector<Vertex>{0, 1}));

  Graph tie(2, true);
  tie.add_edge(0, 1);
  const std::vector<std::int64_t> tie_weights{5, -5};
  const auto tie_result =
      algorithms::graphs::maximum_weight_closure(tie, tie_weights);
  REQUIRE_EQ(tie_result.maximum_weight, 0);
  REQUIRE(tie_result.selected_vertices.empty());
}

TEST_CASE(maximum_weight_closure_signed_64_boundaries) {
  Graph forced(2, true);
  forced.add_edge(0, 1);
  const auto max = std::numeric_limits<std::int64_t>::max();
  const auto min = std::numeric_limits<std::int64_t>::min();

  const std::vector<std::int64_t> one_profit{max, -(max - 1)};
  const auto one = algorithms::graphs::maximum_weight_closure(forced, one_profit);
  REQUIRE_EQ(one.maximum_weight, 1);
  REQUIRE_EQ(one.selected_vertices, (std::vector<Vertex>{0, 1}));

  const std::vector<std::int64_t> min_penalty{max, min};
  const auto empty =
      algorithms::graphs::maximum_weight_closure(forced, min_penalty);
  REQUIRE_EQ(empty.maximum_weight, 0);
  REQUIRE(empty.selected_vertices.empty());

  Graph singleton(1, true);
  const std::vector<std::int64_t> max_only{max};
  const auto max_result =
      algorithms::graphs::maximum_weight_closure(singleton, max_only);
  REQUIRE_EQ(max_result.maximum_weight, max);
  REQUIRE_EQ(max_result.selected_vertices, (std::vector<Vertex>{0}));

  Graph overflow_graph(2, true);
  const std::vector<std::int64_t> overflow_weights{max, 1};
  REQUIRE_THROWS_AS(algorithms::graphs::maximum_weight_closure(
                        overflow_graph, overflow_weights),
                    std::overflow_error);
}

TEST_CASE(maximum_weight_closure_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xC105A11ULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 11U);
    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if ((rng() % 100U) < 20U) {
          graph.add_edge(from, to, static_cast<std::int64_t>(rng() % 201U) - 100);
          if ((rng() % 7U) == 0U) {
            graph.add_edge(from, to, -static_cast<std::int64_t>(rng() % 97U));
          }
        }
      }
    }
    std::vector<std::int64_t> weights(n, 0);
    for (auto& weight : weights) {
      weight = static_cast<std::int64_t>(rng() % 25U) - 12;
    }

    const auto expected = exhaustive_optimum(graph, weights);
    const auto actual =
        algorithms::graphs::maximum_weight_closure(graph, weights);
    const auto repeated =
        algorithms::graphs::maximum_weight_closure(graph, weights);

    REQUIRE_EQ(actual.maximum_weight, expected);
    REQUIRE_EQ(actual.selected_vertices, repeated.selected_vertices);
    REQUIRE_EQ(actual.maximum_weight, replay_weight(actual.selected_vertices, weights));
    REQUIRE(witness_is_closed(graph, actual.selected_vertices));
    REQUIRE(std::is_sorted(actual.selected_vertices.begin(),
                           actual.selected_vertices.end()));
    REQUIRE_EQ(actual.maximum_weight,
               actual.total_positive_weight - actual.min_cut_capacity);
  }
}
