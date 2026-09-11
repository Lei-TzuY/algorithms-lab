#include "algorithms/graphs/minimum_mean_cycle.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::MinimumMeanCycleResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::minimum_mean_cycle;

struct SmallRatio {
  std::int64_t numerator{};
  std::int64_t denominator{1};
};

std::int64_t positive_gcd(std::int64_t first, std::int64_t second) {
  if (first < 0) {
    first = -first;
  }
  while (second != 0) {
    const auto remainder = first % second;
    first = second;
    second = remainder;
  }
  return first;
}

SmallRatio normalize_small_ratio(std::int64_t numerator,
                                 std::int64_t denominator) {
  const auto divisor = positive_gcd(numerator, denominator);
  numerator /= divisor;
  denominator /= divisor;
  if (denominator < 0) {
    numerator = -numerator;
    denominator = -denominator;
  }
  return SmallRatio{numerator, denominator};
}

bool smaller_small_ratio(const SmallRatio& first, const SmallRatio& second) {
  // The exhaustive oracle is used only with <= 6 vertices and weights in
  // [-8,8], so these cross products are intentionally tiny and independent of
  // the production overflow-free fraction comparator.
  return first.numerator * second.denominator <
         second.numerator * first.denominator;
}

std::optional<SmallRatio> exhaustive_simple_cycle_oracle(const Graph& graph) {
  const auto vertex_count = graph.vertex_count();
  std::optional<SmallRatio> best;

  for (Vertex start = 0; start < vertex_count; ++start) {
    std::vector<unsigned char> used(vertex_count, 0);
    used[start] = 1;
    std::int64_t weight = 0;
    std::size_t length = 0;

    const auto search = [&](const auto& self, Vertex vertex) -> void {
      for (const auto& edge : graph.neighbors(vertex)) {
        if (edge.to == start) {
          const auto candidate = normalize_small_ratio(
              weight + edge.weight,
              static_cast<std::int64_t>(length + 1));
          if (!best || smaller_small_ratio(candidate, *best)) {
            best = candidate;
          }
          continue;
        }
        if (used[edge.to] != 0) {
          continue;
        }
        used[edge.to] = 1;
        weight += edge.weight;
        ++length;
        self(self, edge.to);
        --length;
        weight -= edge.weight;
        used[edge.to] = 0;
      }
    };
    search(search, start);
  }
  return best;
}

void require_witness(const Graph& graph, const MinimumMeanCycleResult& result) {
  REQUIRE(!result.cycle.empty());
  std::int64_t weight = 0;
  for (std::size_t index = 0; index < result.cycle.size(); ++index) {
    const auto& edge = result.cycle[index];
    REQUIRE(edge.from < graph.vertex_count());
    REQUIRE(edge.adjacency_index < graph.neighbors(edge.from).size());
    const auto actual = graph.neighbors(edge.from)[edge.adjacency_index];
    REQUIRE_EQ(actual.to, edge.to);
    REQUIRE_EQ(actual.weight, edge.weight);
    REQUIRE_EQ(edge.to,
               result.cycle[(index + 1) % result.cycle.size()].from);
    weight += edge.weight;
  }
  REQUIRE_EQ(weight, result.cycle_weight);

  // Deterministic tests and randomized-oracle instances keep these products in
  // a tiny range. Full-width arithmetic boundaries are tested separately.
  REQUIRE_EQ(weight * static_cast<std::int64_t>(result.mean_denominator),
             result.mean_numerator *
                 static_cast<std::int64_t>(result.cycle.size()));
}

TEST_CASE(minimum_mean_cycle_validation_and_acyclic_graph) {
  Graph undirected(2, false);
  REQUIRE_THROWS_AS(minimum_mean_cycle(undirected), std::invalid_argument);

  Graph empty(0, true);
  REQUIRE(!minimum_mean_cycle(empty).has_value());

  Graph dag(5, true);
  dag.add_edge(0, 1, -4);
  dag.add_edge(0, 2, 3);
  dag.add_edge(1, 3, 1);
  dag.add_edge(2, 3, -7);
  dag.add_edge(3, 4, 2);
  REQUIRE(!minimum_mean_cycle(dag).has_value());
}

TEST_CASE(minimum_mean_cycle_rational_parallel_and_disconnected_cycles) {
  Graph graph(7, true);
  // Mean 1/2, with a worse parallel copy.
  graph.add_edge(0, 1, 0);
  graph.add_edge(1, 0, 1);
  graph.add_edge(0, 1, 5);

  // Mean -4/3 in a disconnected component: (-5 + -2 + 3) / 3.
  graph.add_edge(3, 4, -5);
  graph.add_edge(4, 5, -2);
  graph.add_edge(5, 3, 3);

  // A zero self-loop must not beat the negative rational cycle.
  graph.add_edge(6, 6, 0);

  const auto result = minimum_mean_cycle(graph);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->mean_numerator, -4);
  REQUIRE_EQ(result->mean_denominator, 3U);
  require_witness(graph, *result);
  REQUIRE_EQ(*result, *minimum_mean_cycle(graph));
}

TEST_CASE(minimum_mean_cycle_self_loop_and_full_width_boundaries) {
  Graph negative_loop(1, true);
  negative_loop.add_edge(0, 0, -7);
  const auto negative = minimum_mean_cycle(negative_loop);
  REQUIRE(negative.has_value());
  REQUIRE_EQ(negative->mean_numerator, -7);
  REQUIRE_EQ(negative->mean_denominator, 1U);
  require_witness(negative_loop, *negative);

  Graph minimum(1, true);
  minimum.add_edge(0, 0, std::numeric_limits<std::int64_t>::min());
  const auto low = minimum_mean_cycle(minimum);
  REQUIRE(low.has_value());
  REQUIRE_EQ(low->mean_numerator, std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(low->mean_denominator, 1U);

  Graph maximum(1, true);
  maximum.add_edge(0, 0, std::numeric_limits<std::int64_t>::max());
  const auto high = minimum_mean_cycle(maximum);
  REQUIRE(high.has_value());
  REQUIRE_EQ(high->mean_numerator, std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(high->mean_denominator, 1U);

  Graph unrepresentable_walk(2, true);
  unrepresentable_walk.add_edge(0, 1,
                                std::numeric_limits<std::int64_t>::max());
  unrepresentable_walk.add_edge(1, 1,
                                std::numeric_limits<std::int64_t>::max());
  REQUIRE_THROWS_AS(minimum_mean_cycle(unrepresentable_walk),
                    std::overflow_error);
}

TEST_CASE(minimum_mean_cycle_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xC1C1EULL);
  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(rng() % 7U);
    Graph graph(vertex_count, true);
    const std::size_t edge_count =
        vertex_count == 0 ? 0 : static_cast<std::size_t>(rng() % 15U);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      const auto from = static_cast<Vertex>(rng() % vertex_count);
      const auto to = static_cast<Vertex>(rng() % vertex_count);
      const auto weight = static_cast<std::int64_t>(
          static_cast<int>(rng() % 17U) - 8);
      graph.add_edge(from, to, weight);
    }

    const auto expected = exhaustive_simple_cycle_oracle(graph);
    const auto actual = minimum_mean_cycle(graph);
    REQUIRE_EQ(actual.has_value(), expected.has_value());
    if (!actual) {
      continue;
    }
    REQUIRE_EQ(actual->mean_numerator, expected->numerator);
    REQUIRE_EQ(actual->mean_denominator,
               static_cast<std::uint64_t>(expected->denominator));
    require_witness(graph, *actual);
  }
}

}  // namespace
