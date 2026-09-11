#include "algorithms/combinatorial/matroid_intersection.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::combinatorial::MatroidIndependenceOracle;
using algorithms::combinatorial::maximum_cardinality_matroid_intersection;

namespace {

MatroidIndependenceOracle partition_oracle(std::vector<std::size_t> label) {
  return [label = std::move(label)](const std::vector<std::size_t>& subset) {
    std::vector<std::size_t> seen;
    seen.reserve(subset.size());
    for (const std::size_t element : subset) {
      if (element >= label.size()) {
        return false;
      }
      if (std::find(seen.begin(), seen.end(), label[element]) != seen.end()) {
        return false;
      }
      seen.push_back(label[element]);
    }
    return true;
  };
}

struct GraphicEdge { std::size_t first; std::size_t second; };

MatroidIndependenceOracle graphic_oracle(std::size_t vertex_count,
                                         std::vector<GraphicEdge> edges) {
  return [vertex_count, edges = std::move(edges)](
             const std::vector<std::size_t>& subset) {
    std::vector<std::size_t> parent(vertex_count);
    std::iota(parent.begin(), parent.end(), 0U);
    std::function<std::size_t(std::size_t)> find = [&](std::size_t v) {
      while (parent[v] != v) {
        v = parent[v];
      }
      return v;
    };
    for (const std::size_t element : subset) {
      if (element >= edges.size()) {
        return false;
      }
      const auto edge = edges[element];
      if (edge.first >= vertex_count || edge.second >= vertex_count) {
        return false;
      }
      const std::size_t a = find(edge.first);
      const std::size_t b = find(edge.second);
      if (a == b) {
        return false;
      }
      parent[b] = a;
    }
    return true;
  };
}

std::size_t exhaustive_optimum(std::size_t ground_size,
                               const MatroidIndependenceOracle& first,
                               const MatroidIndependenceOracle& second) {
  if (ground_size > 20U) {
    throw std::invalid_argument("exhaustive oracle ground set too large");
  }
  const std::uint64_t limit = std::uint64_t{1} << ground_size;
  std::size_t best = 0;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    std::vector<std::size_t> subset;
    for (std::size_t element = 0; element < ground_size; ++element) {
      if ((mask & (std::uint64_t{1} << element)) != 0U) {
        subset.push_back(element);
      }
    }
    if (subset.size() > best && first(subset) && second(subset)) {
      best = subset.size();
    }
  }
  return best;
}

bool is_sorted_unique(const std::vector<std::size_t>& values) {
  return std::is_sorted(values.begin(), values.end()) &&
         std::adjacent_find(values.begin(), values.end()) == values.end();
}

}  // namespace

TEST_CASE(matroid_intersection_empty_and_oracle_contract) {
  const auto always = [](const std::vector<std::size_t>&) { return true; };
  const auto result = maximum_cardinality_matroid_intersection(0, always, always);
  REQUIRE(result.selected_elements.empty());
  REQUIRE_EQ(result.augmentation_count, 0U);
  REQUIRE(result.first_oracle_calls >= 1U);
  REQUIRE(result.second_oracle_calls >= 1U);

  const MatroidIndependenceOracle missing;
  REQUIRE_THROWS_AS(maximum_cardinality_matroid_intersection(0, missing, always),
                    std::invalid_argument);
  const auto rejects_empty = [](const std::vector<std::size_t>& subset) {
    return !subset.empty();
  };
  REQUIRE_THROWS_AS(
      maximum_cardinality_matroid_intersection(1, rejects_empty, always),
      std::invalid_argument);
}

TEST_CASE(matroid_intersection_exchange_path_beats_greedy) {
  // Ground elements are bipartite edges:
  // 0=(L0,R0), 1=(L0,R1), 2=(L1,R0).
  // Picking element 0 first is locally maximal, but the exchange path
  // 2 -> 0 -> 1 augments to the optimum {1,2}.
  const auto first = partition_oracle({0, 0, 1});
  const auto second = partition_oracle({0, 1, 0});
  const auto result = maximum_cardinality_matroid_intersection(3, first, second);
  REQUIRE_EQ(result.selected_elements, (std::vector<std::size_t>{1, 2}));
  REQUIRE_EQ(result.augmentation_count, 2U);
  REQUIRE(first(result.selected_elements));
  REQUIRE(second(result.selected_elements));

  const auto repeated = maximum_cardinality_matroid_intersection(3, first, second);
  REQUIRE_EQ(repeated.selected_elements, result.selected_elements);
}

TEST_CASE(matroid_intersection_partition_randomized_exhaustive) {
  std::mt19937_64 rng(0x4D4154524F4944ULL);
  for (std::size_t trial = 0; trial < 1000U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 12U);
    const std::size_t label_count_a = 1U + static_cast<std::size_t>(rng() % 5U);
    const std::size_t label_count_b = 1U + static_cast<std::size_t>(rng() % 5U);
    std::vector<std::size_t> a(n);
    std::vector<std::size_t> b(n);
    for (std::size_t i = 0; i < n; ++i) {
      a[i] = static_cast<std::size_t>(rng() % label_count_a);
      b[i] = static_cast<std::size_t>(rng() % label_count_b);
    }
    const auto first = partition_oracle(a);
    const auto second = partition_oracle(b);
    const auto result = maximum_cardinality_matroid_intersection(n, first, second);
    const std::size_t optimum = exhaustive_optimum(n, first, second);
    REQUIRE_EQ(result.selected_elements.size(), optimum);
    REQUIRE(is_sorted_unique(result.selected_elements));
    REQUIRE(first(result.selected_elements));
    REQUIRE(second(result.selected_elements));
  }
}

TEST_CASE(matroid_intersection_graphic_partition_randomized_exhaustive) {
  std::mt19937_64 rng(0x47524150484943ULL);
  for (std::size_t trial = 0; trial < 600U; ++trial) {
    const std::size_t vertices = 1U + static_cast<std::size_t>(rng() % 6U);
    const std::size_t edge_count = static_cast<std::size_t>(rng() % 11U);
    std::vector<GraphicEdge> edges;
    std::vector<std::size_t> colors;
    edges.reserve(edge_count);
    colors.reserve(edge_count);
    const std::size_t color_count = 1U + static_cast<std::size_t>(rng() % 5U);
    for (std::size_t i = 0; i < edge_count; ++i) {
      const std::size_t u = static_cast<std::size_t>(rng() % vertices);
      std::size_t v = static_cast<std::size_t>(rng() % vertices);
      if (vertices > 1U && v == u) {
        v = (v + 1U) % vertices;
      }
      edges.push_back(GraphicEdge{u, v});
      colors.push_back(static_cast<std::size_t>(rng() % color_count));
    }
    const auto first = graphic_oracle(vertices, edges);
    const auto second = partition_oracle(colors);
    const auto result = maximum_cardinality_matroid_intersection(
        edge_count, first, second);
    const std::size_t optimum = exhaustive_optimum(edge_count, first, second);
    REQUIRE_EQ(result.selected_elements.size(), optimum);
    REQUIRE(first(result.selected_elements));
    REQUIRE(second(result.selected_elements));
  }
}
