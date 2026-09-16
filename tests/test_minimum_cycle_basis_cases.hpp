#pragma once

#include "algorithms/graphs/minimum_cycle_basis.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace minimum_cycle_basis_test_detail {

using algorithms::graphs::CycleBasisCycle;
using algorithms::graphs::MinimumCycleBasisResult;

inline std::uint64_t exhaustive_optimum(const MinimumCycleBasisResult& result,
                                       std::size_t vertex_count) {
  const std::size_t edge_count = result.logical_edges.size();
  REQUIRE(edge_count <= 15U);
  const std::size_t limit = std::size_t{1} << edge_count;
  struct Vector { std::uint64_t mask; std::uint64_t weight; };
  std::vector<Vector> vectors;
  for (std::size_t raw = 1; raw < limit; ++raw) {
    std::vector<unsigned char> parity(vertex_count, 0U);
    std::uint64_t weight = 0U;
    for (std::size_t edge_id = 0; edge_id < edge_count; ++edge_id) {
      if (((raw >> edge_id) & 1U) == 0U) continue;
      const auto& edge = result.logical_edges[edge_id];
      if (edge.from != edge.to) {
        parity[edge.from] ^= 1U;
        parity[edge.to] ^= 1U;
      }
      REQUIRE(weight <= std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(edge.weight));
      weight += static_cast<std::uint64_t>(edge.weight);
    }
    if (std::all_of(parity.begin(), parity.end(), [](unsigned char p) { return p == 0U; })) {
      vectors.push_back(Vector{static_cast<std::uint64_t>(raw), weight});
    }
  }
  std::sort(vectors.begin(), vectors.end(), [](const Vector& first, const Vector& second) {
    if (first.weight != second.weight) return first.weight < second.weight;
    return first.mask < second.mask;
  });
  std::vector<std::uint64_t> basis(edge_count, 0U);
  std::size_t selected = 0;
  std::uint64_t total = 0U;
  for (const auto vector : vectors) {
    std::uint64_t reduced = vector.mask;
    for (std::size_t pivot = edge_count; pivot-- > 0;) {
      if (((reduced >> pivot) & 1U) == 0U) continue;
      if (basis[pivot] != 0U) {
        reduced ^= basis[pivot];
      } else {
        basis[pivot] = reduced;
        total += vector.weight;
        ++selected;
        break;
      }
    }
    if (selected == result.cycle_space_dimension) break;
  }
  REQUIRE_EQ(selected, result.cycle_space_dimension);
  return total;
}

inline bool valid_cycle(const MinimumCycleBasisResult& result,
                        const CycleBasisCycle& cycle,
                        std::size_t vertex_count) {
  std::vector<unsigned char> parity(vertex_count, 0U);
  std::uint64_t weight = 0U;
  std::size_t previous = 0;
  bool first = true;
  for (const auto edge_id : cycle.logical_edge_ids) {
    if (edge_id >= result.logical_edges.size()) return false;
    if (!first && edge_id <= previous) return false;
    first = false;
    previous = edge_id;
    const auto& edge = result.logical_edges[edge_id];
    if (edge.from != edge.to) {
      parity[edge.from] ^= 1U;
      parity[edge.to] ^= 1U;
    }
    if (weight > std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(edge.weight)) return false;
    weight += static_cast<std::uint64_t>(edge.weight);
  }
  return weight == cycle.weight &&
         std::all_of(parity.begin(), parity.end(), [](unsigned char p) { return p == 0U; });
}

}  // namespace minimum_cycle_basis_test_detail

TEST_CASE(minimum_cycle_basis_deterministic_multigraph_cases) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::minimum_weight_cycle_basis;
  {
    Graph graph(0, false);
    const auto result = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(result.cycle_space_dimension, 0U);
    REQUIRE(result.cycles.empty());
  }
  {
    Graph graph(4, false);
    graph.add_edge(0, 1, 1);
    graph.add_edge(1, 2, 2);
    graph.add_edge(2, 3, 3);
    REQUIRE_EQ(minimum_weight_cycle_basis(graph).cycle_space_dimension, 0U);
  }
  {
    Graph graph(4, false);
    graph.add_edge(0, 1, 1);
    graph.add_edge(1, 2, 1);
    graph.add_edge(2, 3, 1);
    graph.add_edge(3, 0, 1);
    graph.add_edge(0, 2, 1);
    const auto result = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(result.cycle_space_dimension, 2U);
    REQUIRE_EQ(result.total_weight, 6U);
  }
  {
    Graph graph(2, false);
    graph.add_edge(0, 0, 2);
    graph.add_edge(1, 0, 1);
    graph.add_edge(0, 1, 3);
    const auto result = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(result.cycle_space_dimension, 2U);
    REQUIRE_EQ(result.total_weight, 6U);
    REQUIRE_EQ(result.logical_edges.size(), 3U);
  }
  {
    Graph graph(5, false);
    graph.add_edge(0, 1, 1);
    graph.add_edge(1, 2, 1);
    graph.add_edge(2, 0, 1);
    graph.add_edge(3, 4, 5);
    graph.add_edge(3, 4, 6);
    const auto result = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(result.cycle_space_dimension, 2U);
    REQUIRE_EQ(result.total_weight, 14U);
  }
}

TEST_CASE(minimum_cycle_basis_rejects_unsupported_or_unrepresentable_inputs) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::minimum_weight_cycle_basis;
  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(minimum_weight_cycle_basis(directed), std::invalid_argument);

  Graph negative(2, false);
  negative.add_edge(0, 1, -1);
  REQUIRE_THROWS_AS(minimum_weight_cycle_basis(negative), std::invalid_argument);

  Graph overflow(2, false);
  overflow.add_edge(0, 1, std::numeric_limits<std::int64_t>::max());
  overflow.add_edge(0, 1, std::numeric_limits<std::int64_t>::max());
  overflow.add_edge(0, 1, std::numeric_limits<std::int64_t>::max());
  REQUIRE_THROWS_AS(minimum_weight_cycle_basis(overflow), std::overflow_error);
}

TEST_CASE(minimum_cycle_basis_randomized_exact_cycle_space_differential) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::minimum_weight_cycle_basis;
  std::mt19937_64 random(0xC1C1EBA515ULL);
  for (std::size_t trial = 0; trial < 450U; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(random() % 7U);
    Graph graph(vertex_count, false);
    const std::size_t edge_count = vertex_count == 0U ? 0U : static_cast<std::size_t>(random() % 11U);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      const auto first = static_cast<std::size_t>(random() % vertex_count);
      const auto second = static_cast<std::size_t>(random() % vertex_count);
      const auto weight = static_cast<std::int64_t>(1U + random() % 9U);
      graph.add_edge(first, second, weight);
    }
    const auto result = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(result.logical_edges.size(), edge_count);
    REQUIRE_EQ(result.cycles.size(), result.cycle_space_dimension);
    REQUIRE_EQ(result.total_weight,
               minimum_cycle_basis_test_detail::exhaustive_optimum(result, vertex_count));
    for (const auto& cycle : result.cycles) {
      REQUIRE(minimum_cycle_basis_test_detail::valid_cycle(result, cycle, vertex_count));
    }
    const auto replay = minimum_weight_cycle_basis(graph);
    REQUIRE_EQ(replay.logical_edges, result.logical_edges);
    REQUIRE_EQ(replay.cycles, result.cycles);
    REQUIRE_EQ(replay.total_weight, result.total_weight);
  }
}
