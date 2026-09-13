#pragma once

#include "algorithms/combinatorial/matroid_union.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::combinatorial::MatroidIndependenceOracle;
using algorithms::combinatorial::MatroidUnionResult;
using algorithms::combinatorial::maximum_cardinality_matroid_union;

MatroidIndependenceOracle union_partition_oracle(
    std::vector<std::size_t> group, std::vector<std::size_t> capacity) {
  return [group = std::move(group), capacity = std::move(capacity)](
             const std::vector<std::size_t>& subset) {
    std::vector<std::size_t> used(capacity.size(), 0U);
    std::size_t previous = 0U;
    bool have_previous = false;
    for (const std::size_t element : subset) {
      if (element >= group.size() ||
          (have_previous && element <= previous) ||
          group[element] >= capacity.size()) {
        return false;
      }
      previous = element;
      have_previous = true;
      if (++used[group[element]] > capacity[group[element]]) return false;
    }
    return true;
  };
}

MatroidIndependenceOracle union_graphic_oracle(
    std::size_t vertex_count,
    std::vector<std::pair<std::size_t, std::size_t>> edges) {
  return [vertex_count, edges = std::move(edges)](
             const std::vector<std::size_t>& subset) {
    std::vector<std::size_t> parent(vertex_count);
    for (std::size_t v = 0U; v < vertex_count; ++v) parent[v] = v;
    const auto find = [&](std::size_t v, auto&& self) -> std::size_t {
      if (parent[v] == v) return v;
      parent[v] = self(parent[v], self);
      return parent[v];
    };
    std::size_t previous = 0U;
    bool have_previous = false;
    for (const std::size_t index : subset) {
      if (index >= edges.size() || (have_previous && index <= previous)) {
        return false;
      }
      previous = index;
      have_previous = true;
      const auto [u, v] = edges[index];
      if (u >= vertex_count || v >= vertex_count || u == v) return false;
      const std::size_t ru = find(u, find);
      const std::size_t rv = find(v, find);
      if (ru == rv) return false;
      parent[ru] = rv;
    }
    return true;
  };
}

std::size_t union_brute_optimum(
    std::size_t ground_size,
    const std::vector<MatroidIndependenceOracle>& matroids) {
  if (matroids.empty()) return 0U;
  std::vector<std::vector<std::size_t>> layers(matroids.size());
  std::size_t best = 0U;
  std::function<void(std::size_t, std::size_t)> visit =
      [&](std::size_t element, std::size_t selected) {
        if (element == ground_size) {
          best = std::max(best, selected);
          return;
        }
        visit(element + 1U, selected);
        for (std::size_t layer = 0U; layer < matroids.size(); ++layer) {
          layers[layer].push_back(element);
          if (matroids[layer](layers[layer])) visit(element + 1U, selected + 1U);
          layers[layer].pop_back();
        }
      };
  visit(0U, 0U);
  return best;
}

void union_replay(std::size_t ground_size,
                  const std::vector<MatroidIndependenceOracle>& matroids,
                  const MatroidUnionResult& result) {
  REQUIRE_EQ(result.independent_sets.size(), matroids.size());
  REQUIRE_EQ(result.assigned_matroid.size(), ground_size);
  REQUIRE_EQ(result.oracle_calls.size(), matroids.size());
  REQUIRE_EQ(result.augmentation_count, result.selected_elements.size());
  REQUIRE(std::is_sorted(result.selected_elements.begin(),
                         result.selected_elements.end()));
  std::vector<bool> seen(ground_size, false);
  std::size_t assigned = 0U;
  for (std::size_t layer = 0U; layer < matroids.size(); ++layer) {
    const auto& set = result.independent_sets[layer];
    REQUIRE(std::is_sorted(set.begin(), set.end()));
    REQUIRE(matroids[layer](set));
    for (const std::size_t element : set) {
      REQUIRE(element < ground_size);
      REQUIRE(!seen[element]);
      seen[element] = true;
      ++assigned;
      REQUIRE(result.assigned_matroid[element].has_value());
      REQUIRE_EQ(*result.assigned_matroid[element], layer);
    }
  }
  REQUIRE_EQ(assigned, result.selected_elements.size());
  for (std::size_t element = 0U; element < ground_size; ++element) {
    REQUIRE_EQ(result.assigned_matroid[element].has_value(), seen[element]);
  }
}

TEST_CASE(matroid_union_validation_and_zero_layer_semantics) {
  const auto empty_union = maximum_cardinality_matroid_union(5U, {});
  REQUIRE(empty_union.selected_elements.empty());
  REQUIRE(empty_union.independent_sets.empty());
  REQUIRE_EQ(empty_union.assigned_matroid.size(), 5U);

  std::vector<MatroidIndependenceOracle> missing{MatroidIndependenceOracle{}};
  REQUIRE_THROWS_AS(maximum_cardinality_matroid_union(3U, missing),
                    std::invalid_argument);
  std::vector<MatroidIndependenceOracle> rejects_empty{
      [](const std::vector<std::size_t>& subset) { return !subset.empty(); }};
  REQUIRE_THROWS_AS(maximum_cardinality_matroid_union(3U, rejects_empty),
                    std::invalid_argument);
  const auto free = [](const std::vector<std::size_t>&) { return true; };
  REQUIRE_THROWS_AS(maximum_cardinality_matroid_union(
                        std::numeric_limits<std::size_t>::max(),
                        std::vector<MatroidIndependenceOracle>{free, free}),
                    std::length_error);
}

TEST_CASE(matroid_union_reassigns_across_layers) {
  const std::vector<MatroidIndependenceOracle> matroids{
      union_partition_oracle({2U, 2U, 2U, 0U}, {1U, 1U, 1U}),
      union_partition_oracle({1U, 2U, 2U, 0U}, {1U, 1U, 1U})};
  const auto result = maximum_cardinality_matroid_union(4U, matroids);
  union_replay(4U, matroids, result);
  REQUIRE_EQ(result.selected_elements,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U}));
}

TEST_CASE(matroid_union_random_partition_instances_match_assignment_oracle) {
  std::mt19937_64 rng(0x4D4154524F494455ULL);
  for (std::size_t trial = 0U; trial < 300U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    const std::size_t k = 1U + static_cast<std::size_t>(rng() % 3U);
    std::vector<MatroidIndependenceOracle> matroids;
    for (std::size_t layer = 0U; layer < k; ++layer) {
      const std::size_t groups_count = 1U + static_cast<std::size_t>(rng() % 4U);
      std::vector<std::size_t> groups(n);
      for (auto& group : groups) group = static_cast<std::size_t>(rng() % groups_count);
      std::vector<std::size_t> capacities(groups_count);
      for (auto& capacity : capacities) capacity = static_cast<std::size_t>(rng() % 3U);
      matroids.push_back(
          union_partition_oracle(std::move(groups), std::move(capacities)));
    }
    const auto result = maximum_cardinality_matroid_union(n, matroids);
    union_replay(n, matroids, result);
    REQUIRE_EQ(result.selected_elements.size(), union_brute_optimum(n, matroids));
    REQUIRE_EQ(result, maximum_cardinality_matroid_union(n, matroids));
  }
}

TEST_CASE(matroid_union_two_graphic_layers_match_assignment_oracle) {
  std::mt19937_64 rng(0x554E494F4E464F52ULL);
  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    const std::size_t vertices = 1U + static_cast<std::size_t>(rng() % 6U);
    const std::size_t requested_edges = static_cast<std::size_t>(rng() % 9U);
    std::vector<std::pair<std::size_t, std::size_t>> edges;
    for (std::size_t edge = 0U; edge < requested_edges && vertices > 1U; ++edge) {
      const std::size_t u = static_cast<std::size_t>(rng() % vertices);
      std::size_t v = static_cast<std::size_t>(rng() % vertices);
      if (u == v) v = (v + 1U) % vertices;
      edges.emplace_back(u, v);
    }
    const std::vector<MatroidIndependenceOracle> matroids{
        union_graphic_oracle(vertices, edges), union_graphic_oracle(vertices, edges)};
    const auto result = maximum_cardinality_matroid_union(edges.size(), matroids);
    union_replay(edges.size(), matroids, result);
    REQUIRE_EQ(result.selected_elements.size(),
               union_brute_optimum(edges.size(), matroids));
  }
}

}  // namespace
