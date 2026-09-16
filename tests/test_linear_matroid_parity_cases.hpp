#pragma once

#include "algorithms/combinatorial/linear_matroid_parity.hpp"
#include "algorithms/linear_algebra/finite_field_gaussian_elimination.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::combinatorial::LinearMatroidParityPair;
using algorithms::combinatorial::LinearMatroidParityResult;
using algorithms::combinatorial::kLinearMatroidParityMaxDimension;
using algorithms::combinatorial::kLinearMatroidParityMaxPairs;
using algorithms::combinatorial::maximum_cardinality_linear_matroid_parity;
using algorithms::linear_algebra::solve_linear_system_mod_prime;

std::size_t oracle_rank(
    const std::vector<std::vector<std::uint64_t>>& vectors,
    std::size_t dimension, std::uint64_t modulus) {
  std::vector<std::uint64_t> rhs(vectors.size(), 0U);
  return solve_linear_system_mod_prime(dimension, vectors, rhs, modulus).rank;
}

bool oracle_selection_independent(
    const std::vector<LinearMatroidParityPair>& pairs,
    const std::vector<std::size_t>& selected, std::uint64_t modulus) {
  const std::size_t dimension = pairs.empty() ? 0U : pairs.front().first.size();
  std::vector<std::vector<std::uint64_t>> vectors;
  vectors.reserve(selected.size() * 2U);
  std::size_t previous = 0U;
  bool first_index = true;
  for (const std::size_t pair_index : selected) {
    if (pair_index >= pairs.size()) {
      return false;
    }
    if (!first_index && pair_index <= previous) {
      return false;
    }
    first_index = false;
    previous = pair_index;
    vectors.push_back(pairs[pair_index].first);
    vectors.push_back(pairs[pair_index].second);
  }
  return oracle_rank(vectors, dimension, modulus) == vectors.size();
}

std::size_t exhaustive_optimum_pair_count(
    const std::vector<LinearMatroidParityPair>& pairs, std::uint64_t modulus) {
  const std::size_t pair_count = pairs.size();
  REQUIRE(pair_count <= 20U);
  const std::size_t dimension = pairs.empty() ? 0U : pairs.front().first.size();
  const std::uint64_t subset_count = std::uint64_t{1} << pair_count;
  std::size_t optimum = 0U;
  for (std::uint64_t mask = 0U; mask < subset_count; ++mask) {
    const std::size_t pair_total = static_cast<std::size_t>(std::popcount(mask));
    if (pair_total <= optimum) {
      continue;
    }
    std::vector<std::vector<std::uint64_t>> vectors;
    vectors.reserve(pair_total * 2U);
    for (std::size_t index = 0U; index < pair_count; ++index) {
      if ((mask & (std::uint64_t{1} << index)) == 0U) {
        continue;
      }
      vectors.push_back(pairs[index].first);
      vectors.push_back(pairs[index].second);
    }
    if (oracle_rank(vectors, dimension, modulus) == vectors.size()) {
      optimum = pair_total;
    }
  }
  return optimum;
}

void require_valid_result(const std::vector<LinearMatroidParityPair>& pairs,
                          std::uint64_t modulus,
                          const LinearMatroidParityResult& result) {
  REQUIRE(oracle_selection_independent(pairs, result.selected_pairs, modulus));
  REQUIRE(result.search_nodes >= 1U);
  REQUIRE(result.pruned_nodes <= result.search_nodes);
  REQUIRE(result.rank_bound_evaluations <= result.search_nodes);
}

}  // namespace

TEST_CASE(linear_matroid_parity_validates_bounded_prime_field_contract) {
  REQUIRE_THROWS_AS(maximum_cardinality_linear_matroid_parity({}, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(maximum_cardinality_linear_matroid_parity({}, 15U),
                    std::invalid_argument);

  const std::vector<LinearMatroidParityPair> ragged{
      LinearMatroidParityPair{{1U, 0U}, {0U}}};
  REQUIRE_THROWS_AS(maximum_cardinality_linear_matroid_parity(ragged, 5U),
                    std::invalid_argument);

  std::vector<LinearMatroidParityPair> too_many(
      kLinearMatroidParityMaxPairs + 1U,
      LinearMatroidParityPair{{0U}, {0U}});
  REQUIRE_THROWS_AS(maximum_cardinality_linear_matroid_parity(too_many, 5U),
                    std::length_error);

  const std::vector<LinearMatroidParityPair> too_wide{
      LinearMatroidParityPair{
          std::vector<std::uint64_t>(kLinearMatroidParityMaxDimension + 1U, 0U),
          std::vector<std::uint64_t>(kLinearMatroidParityMaxDimension + 1U,
                                     0U)}};
  REQUIRE_THROWS_AS(maximum_cardinality_linear_matroid_parity(too_wide, 5U),
                    std::length_error);

  const LinearMatroidParityResult empty =
      maximum_cardinality_linear_matroid_parity({}, 2U);
  REQUIRE(empty.selected_pairs.empty());
  REQUIRE_EQ(empty.search_nodes, 1U);
}

TEST_CASE(linear_matroid_parity_returns_replayable_maximum_pair_witness) {
  const std::vector<LinearMatroidParityPair> pairs{
      LinearMatroidParityPair{{1U, 0U, 0U, 0U}, {0U, 1U, 0U, 0U}},
      LinearMatroidParityPair{{1U, 0U, 0U, 0U}, {0U, 0U, 1U, 0U}},
      LinearMatroidParityPair{{0U, 0U, 1U, 0U}, {0U, 0U, 0U, 1U}}};

  const LinearMatroidParityResult first =
      maximum_cardinality_linear_matroid_parity(pairs, 5U);
  const LinearMatroidParityResult second =
      maximum_cardinality_linear_matroid_parity(pairs, 5U);
  REQUIRE(first == second);
  REQUIRE_EQ(first.selected_pairs.size(), 2U);
  REQUIRE(first.selected_pairs == std::vector<std::size_t>({0U, 2U}));
  require_valid_result(pairs, 5U, first);
  REQUIRE_EQ(exhaustive_optimum_pair_count(pairs, 5U), 2U);
}

TEST_CASE(linear_matroid_parity_supports_full_width_prime_field_arithmetic) {
  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const std::vector<LinearMatroidParityPair> pairs{
      LinearMatroidParityPair{
          {std::numeric_limits<std::uint64_t>::max(), 0U, 0U, 0U},
          {0U, prime - 2U, 0U, 0U}},
      LinearMatroidParityPair{{0U, 0U, prime - 3U, 0U},
                              {0U, 0U, 0U, prime - 4U}}};

  const LinearMatroidParityResult result =
      maximum_cardinality_linear_matroid_parity(pairs, prime);
  REQUIRE_EQ(result.selected_pairs.size(), 2U);
  require_valid_result(pairs, prime, result);
}

TEST_CASE(linear_matroid_parity_rank_bound_prunes_impossible_pairs) {
  std::vector<LinearMatroidParityPair> pairs;
  for (std::size_t index = 0U; index < 12U; ++index) {
    pairs.push_back(LinearMatroidParityPair{{1U, 0U}, {1U, 0U}});
  }

  const LinearMatroidParityResult result =
      maximum_cardinality_linear_matroid_parity(pairs, 7U);
  REQUIRE(result.selected_pairs.empty());
  REQUIRE_EQ(result.search_nodes, 1U);
  REQUIRE_EQ(result.pruned_nodes, 1U);
  REQUIRE_EQ(result.rank_bound_evaluations, 1U);
}

TEST_CASE(linear_matroid_parity_randomized_matches_exhaustive_pair_subsets) {
  std::mt19937_64 random(0x4D4154524F494450ULL);
  constexpr std::array<std::uint64_t, 5> primes{2U, 3U, 5U, 7U, 11U};

  std::size_t observed_pruning = 0U;
  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t pair_count = static_cast<std::size_t>(random() % 9U);
    const std::size_t dimension = static_cast<std::size_t>(random() % 11U);
    const std::uint64_t modulus = primes[static_cast<std::size_t>(
        random() % static_cast<std::uint64_t>(primes.size()))];

    std::vector<LinearMatroidParityPair> pairs;
    pairs.reserve(pair_count);
    for (std::size_t pair_index = 0U; pair_index < pair_count; ++pair_index) {
      LinearMatroidParityPair pair;
      pair.first.resize(dimension);
      pair.second.resize(dimension);
      for (std::size_t coordinate = 0U; coordinate < dimension; ++coordinate) {
        pair.first[coordinate] = random() % modulus;
        pair.second[coordinate] = random() % modulus;
      }
      pairs.push_back(std::move(pair));
    }

    const LinearMatroidParityResult result =
        maximum_cardinality_linear_matroid_parity(pairs, modulus);
    const LinearMatroidParityResult repeated =
        maximum_cardinality_linear_matroid_parity(pairs, modulus);
    REQUIRE(result == repeated);
    require_valid_result(pairs, modulus, result);
    REQUIRE_EQ(result.selected_pairs.size(),
               exhaustive_optimum_pair_count(pairs, modulus));
    observed_pruning += result.pruned_nodes;
  }
  REQUIRE(observed_pruning > 0U);
}
