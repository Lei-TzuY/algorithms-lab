#pragma once

#include "algorithms/combinatorial/weighted_matroid_intersection.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace weighted_matroid_tests {

using algorithms::combinatorial::MatroidIndependenceOracle;
using algorithms::combinatorial::maximum_weight_matroid_intersection;

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

struct GraphicEdge {
  std::size_t first;
  std::size_t second;
};

MatroidIndependenceOracle graphic_oracle(std::size_t vertex_count,
                                         std::vector<GraphicEdge> edges) {
  return [vertex_count, edges = std::move(edges)](
             const std::vector<std::size_t>& subset) {
    std::vector<std::size_t> parent(vertex_count);
    std::iota(parent.begin(), parent.end(), 0U);
    std::function<std::size_t(std::size_t)> find = [&](std::size_t vertex) {
      while (parent[vertex] != vertex) {
        vertex = parent[vertex];
      }
      return vertex;
    };
    for (const std::size_t element : subset) {
      if (element >= edges.size()) {
        return false;
      }
      const auto edge = edges[element];
      if (edge.first >= vertex_count || edge.second >= vertex_count) {
        return false;
      }
      const std::size_t first_root = find(edge.first);
      const std::size_t second_root = find(edge.second);
      if (first_root == second_root) {
        return false;
      }
      parent[second_root] = first_root;
    }
    return true;
  };
}

std::pair<std::size_t, std::int64_t> exhaustive_optimum(
    const std::vector<std::int64_t>& weights,
    const MatroidIndependenceOracle& first,
    const MatroidIndependenceOracle& second) {
  if (weights.size() > 20U) {
    throw std::invalid_argument("weighted exhaustive oracle ground set too large");
  }

  std::pair<std::size_t, std::int64_t> best{0U, 0};
  const std::uint64_t limit = std::uint64_t{1} << weights.size();
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    std::vector<std::size_t> subset;
    std::int64_t total = 0;
    for (std::size_t element = 0; element < weights.size(); ++element) {
      if ((mask & (std::uint64_t{1} << element)) != 0U) {
        subset.push_back(element);
        total += weights[element];
      }
    }
    if (first(subset) && second(subset) &&
        (subset.size() > best.first ||
         (subset.size() == best.first && total > best.second))) {
      best = {subset.size(), total};
    }
  }
  return best;
}

}  // namespace weighted_matroid_tests

TEST_CASE(weighted_matroid_intersection_contract_and_weighted_exchange) {
  using namespace weighted_matroid_tests;

  // Bipartite-edge partition matroids. Element 0 has the largest individual
  // weight, but the cardinality-two optimum requires exchanging it out.
  const auto first = partition_oracle({0, 0, 1});
  const auto second = partition_oracle({0, 1, 0});
  const auto result =
      maximum_weight_matroid_intersection({100, 60, 60}, first, second);

  REQUIRE_EQ(result.selected_elements, (std::vector<std::size_t>{1, 2}));
  REQUIRE_EQ(result.total_weight, 120);
  REQUIRE_EQ(result.augmentation_count, 2U);
  REQUIRE(first(result.selected_elements));
  REQUIRE(second(result.selected_elements));

  const auto repeated =
      maximum_weight_matroid_intersection({100, 60, 60}, first, second);
  REQUIRE_EQ(repeated.selected_elements, result.selected_elements);
  REQUIRE_EQ(repeated.total_weight, result.total_weight);
}

TEST_CASE(weighted_matroid_intersection_cardinality_precedes_weight_and_bounds) {
  using namespace weighted_matroid_tests;

  const auto rank_one = partition_oracle({0, 0, 0});
  const auto negative =
      maximum_weight_matroid_intersection({-9, -2, -5}, rank_one, rank_one);
  REQUIRE_EQ(negative.selected_elements.size(), 1U);
  REQUIRE_EQ(negative.total_weight, -2);

  const MatroidIndependenceOracle missing;
  REQUIRE_THROWS_AS(
      maximum_weight_matroid_intersection({}, missing, rank_one),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      maximum_weight_matroid_intersection(
          {std::numeric_limits<std::int64_t>::min()}, rank_one, rank_one),
      std::overflow_error);

  const auto free_two = partition_oracle({0, 1});
  REQUIRE_THROWS_AS(
      maximum_weight_matroid_intersection(
          {std::numeric_limits<std::int64_t>::max(), 1}, free_two, free_two),
      std::overflow_error);
}

TEST_CASE(weighted_matroid_intersection_partition_randomized_exhaustive) {
  using namespace weighted_matroid_tests;

  std::mt19937_64 rng(0x5745494748544544ULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t ground_size = static_cast<std::size_t>(rng() % 11U);
    const std::size_t first_label_count =
        1U + static_cast<std::size_t>(rng() % 5U);
    const std::size_t second_label_count =
        1U + static_cast<std::size_t>(rng() % 5U);

    std::vector<std::size_t> first_labels(ground_size);
    std::vector<std::size_t> second_labels(ground_size);
    std::vector<std::int64_t> weights(ground_size);
    for (std::size_t element = 0; element < ground_size; ++element) {
      first_labels[element] =
          static_cast<std::size_t>(rng() % first_label_count);
      second_labels[element] =
          static_cast<std::size_t>(rng() % second_label_count);
      weights[element] = static_cast<std::int64_t>(rng() % 81U) - 40;
    }

    const auto first = partition_oracle(first_labels);
    const auto second = partition_oracle(second_labels);
    const auto result = maximum_weight_matroid_intersection(weights, first, second);
    const auto optimum = exhaustive_optimum(weights, first, second);

    REQUIRE_EQ(result.selected_elements.size(), optimum.first);
    REQUIRE_EQ(result.total_weight, optimum.second);
    REQUIRE(first(result.selected_elements));
    REQUIRE(second(result.selected_elements));
  }
}

TEST_CASE(weighted_matroid_intersection_graphic_partition_randomized_exhaustive) {
  using namespace weighted_matroid_tests;

  std::mt19937_64 rng(0x4752415048574D49ULL);
  for (std::size_t trial = 0; trial < 400U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(rng() % 6U);
    const std::size_t edge_count = static_cast<std::size_t>(rng() % 10U);
    const std::size_t color_count = 1U + static_cast<std::size_t>(rng() % 5U);

    std::vector<GraphicEdge> edges;
    std::vector<std::size_t> colors;
    std::vector<std::int64_t> weights;
    edges.reserve(edge_count);
    colors.reserve(edge_count);
    weights.reserve(edge_count);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      const std::size_t from = static_cast<std::size_t>(rng() % vertex_count);
      std::size_t to = static_cast<std::size_t>(rng() % vertex_count);
      if (vertex_count > 1U && from == to) {
        to = (to + 1U) % vertex_count;
      }
      edges.push_back(GraphicEdge{from, to});
      colors.push_back(static_cast<std::size_t>(rng() % color_count));
      weights.push_back(static_cast<std::int64_t>(rng() % 81U) - 40);
    }

    const auto first = graphic_oracle(vertex_count, edges);
    const auto second = partition_oracle(colors);
    const auto result = maximum_weight_matroid_intersection(weights, first, second);
    const auto optimum = exhaustive_optimum(weights, first, second);

    REQUIRE_EQ(result.selected_elements.size(), optimum.first);
    REQUIRE_EQ(result.total_weight, optimum.second);
    REQUIRE(first(result.selected_elements));
    REQUIRE(second(result.selected_elements));
  }
}
