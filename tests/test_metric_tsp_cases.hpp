#pragma once

#include "algorithms/approximation/metric_tsp.hpp"
#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/hamiltonian_cycle.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using MetricTspResult = algorithms::approximation::MetricTspApproximationResult;
using algorithms::approximation::approximate_metric_tsp_double_tree;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

Weight metric_tsp_exact_permutation(const std::vector<std::vector<Weight>>& d) {
  const std::size_t n = d.size();
  if (n <= 1) return 0;
  std::vector<Vertex> p(n - 1);
  std::iota(p.begin(), p.end(), Vertex{1});
  Weight best = std::numeric_limits<Weight>::max();
  do {
    Weight total = 0;
    Vertex previous = 0;
    for (const Vertex v : p) { total += d[previous][v]; previous = v; }
    total += d[previous][0];
    best = std::min(best, total);
  } while (std::next_permutation(p.begin(), p.end()));
  return best;
}

std::vector<std::vector<Weight>> metric_tsp_manhattan_matrix(
    const std::vector<std::pair<int, int>>& points) {
  const std::size_t n = points.size();
  std::vector<std::vector<Weight>> d(n, std::vector<Weight>(n, 0));
  for (Vertex a = 0; a < n; ++a) for (Vertex b = 0; b < n; ++b) {
    std::int64_t dx = static_cast<std::int64_t>(points[a].first) - points[b].first;
    std::int64_t dy = static_cast<std::int64_t>(points[a].second) - points[b].second;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    d[a][b] = dx + dy;
  }
  return d;
}

void metric_tsp_verify_tour(const std::vector<std::vector<Weight>>& d,
                            const MetricTspResult& r) {
  const std::size_t n = d.size();
  if (n == 0) { REQUIRE(r.tour.empty()); REQUIRE_EQ(r.total_weight, Weight{0}); return; }
  if (n == 1) { REQUIRE_EQ(r.tour, std::vector<Vertex>({0})); REQUIRE_EQ(r.total_weight, Weight{0}); return; }
  REQUIRE_EQ(r.tour.size(), n + 1);
  REQUIRE_EQ(r.tour.front(), Vertex{0});
  REQUIRE_EQ(r.tour.back(), Vertex{0});
  std::vector<bool> seen(n, false);
  for (std::size_t i = 0; i < n; ++i) { REQUIRE(r.tour[i] < n); REQUIRE(!seen[r.tour[i]]); seen[r.tour[i]] = true; }
  for (const bool present : seen) REQUIRE(present);
  Weight replayed = 0;
  for (std::size_t i = 1; i < r.tour.size(); ++i) replayed += d[r.tour[i - 1]][r.tour[i]];
  REQUIRE_EQ(replayed, r.total_weight);
  REQUIRE_EQ(r.minimum_spanning_tree_edges.size(), n - 1);
  Weight tree_weight = 0;
  for (const auto& edge : r.minimum_spanning_tree_edges) { REQUIRE(edge.from < n); REQUIRE(edge.to < n); REQUIRE_EQ(edge.weight, d[edge.from][edge.to]); tree_weight += edge.weight; }
  REQUIRE_EQ(tree_weight, r.minimum_spanning_tree_weight);
}

Graph metric_tsp_directed_complete_graph(const std::vector<std::vector<Weight>>& d) {
  Graph g(d.size(), true);
  for (Vertex u = 0; u < d.size(); ++u) for (Vertex v = 0; v < d.size(); ++v) if (u != v) g.add_edge(u, v, d[u][v]);
  return g;
}

TEST_CASE(metric_tsp_double_tree_trivial_and_validation_contracts) {
  metric_tsp_verify_tour({}, approximate_metric_tsp_double_tree({}));
  const std::vector<std::vector<Weight>> singleton{{0}};
  metric_tsp_verify_tour(singleton, approximate_metric_tsp_double_tree(singleton));
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, 1}, {1}}), std::invalid_argument);
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{1}}), std::invalid_argument);
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, 0}, {0, 0}}), std::invalid_argument);
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, -1}, {-1, 0}}), std::invalid_argument);
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, 1}, {2, 0}}), std::invalid_argument);
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, 1, 3}, {1, 0, 1}, {3, 1, 0}}), std::invalid_argument);
  const Weight maximum = std::numeric_limits<Weight>::max();
  REQUIRE_THROWS_AS(approximate_metric_tsp_double_tree({{0, maximum}, {maximum, 0}}), std::overflow_error);
}

TEST_CASE(metric_tsp_double_tree_deterministic_square_witness) {
  const std::vector<std::vector<Weight>> square{{0,1,2,1},{1,0,1,2},{2,1,0,1},{1,2,1,0}};
  const auto first = approximate_metric_tsp_double_tree(square);
  const auto second = approximate_metric_tsp_double_tree(square);
  metric_tsp_verify_tour(square, first);
  REQUIRE_EQ(first.tour, second.tour);
  REQUIRE_EQ(first.minimum_spanning_tree_edges, second.minimum_spanning_tree_edges);
  REQUIRE_EQ(first.total_weight, Weight{4});
  REQUIRE_EQ(first.minimum_spanning_tree_weight, Weight{3});
}

TEST_CASE(metric_tsp_double_tree_randomized_exact_quality_and_cross_integration) {
  std::mt19937_64 rng(0x4D45545249435453ULL);
  std::uniform_int_distribution<int> coordinate(-15, 15);
  for (int trial = 0; trial < 350; ++trial) {
    const std::size_t n = 2 + static_cast<std::size_t>(rng() % 7U);
    std::vector<std::pair<int,int>> points;
    while (points.size() < n) {
      const std::pair<int,int> candidate{coordinate(rng), coordinate(rng)};
      if (std::find(points.begin(), points.end(), candidate) == points.end()) points.push_back(candidate);
    }
    const auto d = metric_tsp_manhattan_matrix(points);
    const auto result = approximate_metric_tsp_double_tree(d);
    metric_tsp_verify_tour(d, result);
    const Weight optimum = metric_tsp_exact_permutation(d);
    REQUIRE(result.minimum_spanning_tree_weight <= optimum);
    REQUIRE(result.total_weight <= 2 * optimum);
    const auto exact = algorithms::graphs::minimum_directed_hamiltonian_cycle(metric_tsp_directed_complete_graph(d));
    REQUIRE(exact.has_value());
    REQUIRE_EQ(exact->total_weight, optimum);
  }
}
}  // namespace
