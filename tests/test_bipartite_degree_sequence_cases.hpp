#pragma once

#include "algorithms/graphs/bipartite_degree_sequence.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <utility>
#include <vector>

namespace bipartite_degree_sequence_test_detail {

using DegreePair =
    std::pair<std::vector<std::size_t>, std::vector<std::size_t>>;

inline std::set<DegreePair> realizable_pairs(std::size_t left_count,
                                             std::size_t right_count) {
  const std::size_t edge_count = left_count * right_count;
  REQUIRE(edge_count < 63U);
  const std::uint64_t graph_count = std::uint64_t{1} << edge_count;
  std::set<DegreePair> realizable;
  for (std::uint64_t mask = 0; mask < graph_count; ++mask) {
    std::vector<std::size_t> left(left_count, 0U);
    std::vector<std::size_t> right(right_count, 0U);
    for (std::size_t l = 0; l < left_count; ++l) {
      for (std::size_t r = 0; r < right_count; ++r) {
        const std::size_t bit = l * right_count + r;
        if (((mask >> bit) & std::uint64_t{1}) != 0U) {
          ++left[l];
          ++right[r];
        }
      }
    }
    realizable.emplace(std::move(left), std::move(right));
  }
  return realizable;
}

inline void enumerate_vectors(std::size_t length, std::size_t maximum,
                              std::vector<std::size_t>& current,
                              const auto& callback) {
  if (current.size() == length) {
    callback(current);
    return;
  }
  for (std::size_t value = 0; value <= maximum; ++value) {
    current.push_back(value);
    enumerate_vectors(length, maximum, current, callback);
    current.pop_back();
  }
}

inline void verify_realization(
    const std::vector<std::size_t>& left,
    const std::vector<std::size_t>& right,
    const algorithms::graphs::BipartiteDegreeRealization& realization) {
  REQUIRE(algorithms::graphs::valid_bipartite_degree_realization(
      left, right, realization));
  REQUIRE(std::is_sorted(realization.edges.begin(), realization.edges.end()));
  REQUIRE(std::adjacent_find(realization.edges.begin(), realization.edges.end()) ==
          realization.edges.end());
}

}  // namespace bipartite_degree_sequence_test_detail

TEST_CASE(bipartite_degree_sequence_contracts_and_known_instances) {
  using algorithms::graphs::bipartite_havel_hakimi_realization;
  using algorithms::graphs::gale_ryser_bipartite_graphical;
  using bipartite_degree_sequence_test_detail::verify_realization;

  const std::vector<std::size_t> empty;
  REQUIRE(gale_ryser_bipartite_graphical(empty, empty));
  const auto empty_result = bipartite_havel_hakimi_realization(empty, empty);
  REQUIRE(empty_result.has_value());
  verify_realization(empty, empty, *empty_result);

  const std::vector<std::size_t> left_empty;
  const std::vector<std::size_t> right_zeros{0U, 0U, 0U};
  REQUIRE(gale_ryser_bipartite_graphical(left_empty, right_zeros));
  const auto one_empty =
      bipartite_havel_hakimi_realization(left_empty, right_zeros);
  REQUIRE(one_empty.has_value());
  verify_realization(left_empty, right_zeros, *one_empty);

  const std::vector<std::size_t> left{2U, 1U, 1U};
  const std::vector<std::size_t> right{2U, 1U, 1U};
  REQUIRE(gale_ryser_bipartite_graphical(left, right));
  const auto realization = bipartite_havel_hakimi_realization(left, right);
  REQUIRE(realization.has_value());
  verify_realization(left, right, *realization);
  REQUIRE_EQ(*realization, *bipartite_havel_hakimi_realization(left, right));

  const std::vector<std::size_t> impossible_left{3U, 3U, 0U};
  const std::vector<std::size_t> impossible_right{3U, 3U, 0U};
  REQUIRE(!gale_ryser_bipartite_graphical(impossible_left, impossible_right));
  REQUIRE(!bipartite_havel_hakimi_realization(impossible_left, impossible_right)
               .has_value());

  const std::vector<std::size_t> unequal_left{2U, 1U};
  const std::vector<std::size_t> unequal_right{1U, 1U};
  REQUIRE(!gale_ryser_bipartite_graphical(unequal_left, unequal_right));
  REQUIRE(!bipartite_havel_hakimi_realization(unequal_left, unequal_right)
               .has_value());

  REQUIRE_THROWS_AS(
      gale_ryser_bipartite_graphical(std::vector<std::size_t>{2U},
                                     std::vector<std::size_t>{1U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      bipartite_havel_hakimi_realization(std::vector<std::size_t>{1U},
                                         std::vector<std::size_t>{2U}),
      std::invalid_argument);
}

TEST_CASE(bipartite_degree_sequence_exhaustive_small_oracle) {
  using algorithms::graphs::bipartite_havel_hakimi_realization;
  using algorithms::graphs::gale_ryser_bipartite_graphical;
  using bipartite_degree_sequence_test_detail::DegreePair;
  using bipartite_degree_sequence_test_detail::enumerate_vectors;
  using bipartite_degree_sequence_test_detail::realizable_pairs;
  using bipartite_degree_sequence_test_detail::verify_realization;

  for (std::size_t left_count = 0; left_count <= 3U; ++left_count) {
    for (std::size_t right_count = 0; right_count <= 3U; ++right_count) {
      const auto oracle = realizable_pairs(left_count, right_count);
      std::vector<std::size_t> left;
      enumerate_vectors(left_count, right_count, left,
                        [&](const std::vector<std::size_t>& left_degrees) {
        std::vector<std::size_t> right;
        enumerate_vectors(
            right_count, left_count, right,
            [&](const std::vector<std::size_t>& right_degrees) {
          const bool expected =
              oracle.contains(DegreePair{left_degrees, right_degrees});
          REQUIRE_EQ(gale_ryser_bipartite_graphical(left_degrees, right_degrees),
                     expected);
          const auto realization = bipartite_havel_hakimi_realization(
              left_degrees, right_degrees);
          REQUIRE_EQ(realization.has_value(), expected);
          if (realization.has_value()) {
            verify_realization(left_degrees, right_degrees, *realization);
          }
        });
      });
    }
  }
}

TEST_CASE(bipartite_degree_sequence_random_graphs_and_permutations) {
  using algorithms::graphs::bipartite_havel_hakimi_realization;
  using algorithms::graphs::gale_ryser_bipartite_graphical;
  using bipartite_degree_sequence_test_detail::verify_realization;

  std::mt19937_64 rng(0x6a1e'7e57'2026ULL);
  for (std::size_t trial = 0; trial < 1200U; ++trial) {
    const std::size_t left_count = static_cast<std::size_t>(rng() % 21U);
    const std::size_t right_count = static_cast<std::size_t>(rng() % 21U);
    std::vector<std::size_t> left(left_count, 0U);
    std::vector<std::size_t> right(right_count, 0U);
    for (std::size_t l = 0; l < left_count; ++l) {
      for (std::size_t r = 0; r < right_count; ++r) {
        if ((rng() % 5U) < 2U) {
          ++left[l];
          ++right[r];
        }
      }
    }

    REQUIRE(gale_ryser_bipartite_graphical(left, right));
    auto realization = bipartite_havel_hakimi_realization(left, right);
    REQUIRE(realization.has_value());
    verify_realization(left, right, *realization);

    std::shuffle(left.begin(), left.end(), rng);
    std::shuffle(right.begin(), right.end(), rng);
    REQUIRE(gale_ryser_bipartite_graphical(left, right));
    realization = bipartite_havel_hakimi_realization(left, right);
    REQUIRE(realization.has_value());
    verify_realization(left, right, *realization);

    if (!left.empty() && left.front() < right.size()) {
      ++left.front();
      REQUIRE(!gale_ryser_bipartite_graphical(left, right));
      REQUIRE(!bipartite_havel_hakimi_realization(left, right).has_value());
    }
  }
}

TEST_CASE(bipartite_degree_sequence_validator_rejects_bad_witnesses) {
  using algorithms::graphs::BipartiteDegreeRealization;
  using algorithms::graphs::valid_bipartite_degree_realization;

  const std::vector<std::size_t> left{1U, 1U};
  const std::vector<std::size_t> right{1U, 1U};

  REQUIRE(!valid_bipartite_degree_realization(
      left, right, BipartiteDegreeRealization{3U, 2U, {{0U, 0U}, {1U, 1U}}, 2U}));
  REQUIRE(!valid_bipartite_degree_realization(
      left, right, BipartiteDegreeRealization{2U, 2U, {{0U, 0U}, {0U, 0U}}, 2U}));
  REQUIRE(!valid_bipartite_degree_realization(
      left, right, BipartiteDegreeRealization{2U, 2U, {{0U, 0U}, {1U, 2U}}, 2U}));
  REQUIRE(valid_bipartite_degree_realization(
      left, right, BipartiteDegreeRealization{2U, 2U, {{0U, 0U}, {1U, 1U}}, 2U}));
}
