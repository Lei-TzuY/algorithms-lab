#pragma once

#include "algorithms/combinatorial/binary_permanent.hpp"
#include "algorithms/graphs/bipartite_matching.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace binary_permanent_tests {

using algorithms::combinatorial::binary_matrix_permanent;
using algorithms::graphs::BipartiteEdge;
using algorithms::graphs::hopcroft_karp;

[[nodiscard]] inline std::uint64_t permutation_oracle(
    std::span<const std::uint64_t> rows) {
  const std::size_t n = rows.size();
  if (n == 0) {
    return 1;
  }
  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(), std::size_t{0});
  std::uint64_t count = 0;
  do {
    bool valid = true;
    for (std::size_t row = 0; row < n; ++row) {
      if ((rows[row] & (std::uint64_t{1} << permutation[row])) == 0ULL) {
        valid = false;
        break;
      }
    }
    if (valid) {
      ++count;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return count;
}

[[nodiscard]] inline std::size_t matching_cardinality(
    std::span<const std::uint64_t> rows) {
  const std::size_t n = rows.size();
  std::vector<BipartiteEdge> edges;
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < n; ++column) {
      if ((rows[row] & (std::uint64_t{1} << column)) != 0ULL) {
        edges.push_back(BipartiteEdge{row, column});
      }
    }
  }
  return hopcroft_karp(n, n, std::span<const BipartiteEdge>(edges)).cardinality;
}

inline void verify_against_oracles(const std::vector<std::uint64_t>& rows) {
  const auto result = binary_matrix_permanent(rows);
  const std::uint64_t expected = permutation_oracle(rows);
  REQUIRE_EQ(result.permanent, expected);
  REQUIRE_EQ(result.dimension, rows.size());
  const std::uint64_t max_subsets = rows.empty()
                                        ? 1ULL
                                        : ((std::uint64_t{1} << rows.size()) - 1ULL);
  REQUIRE(result.subsets_evaluated <= max_subsets);
  REQUIRE((result.permanent > 0ULL) ==
          (matching_cardinality(rows) == rows.size()));
  REQUIRE(result == binary_matrix_permanent(rows));
}

}  // namespace binary_permanent_tests

TEST_CASE(binary_permanent_deterministic_shapes) {
  using namespace binary_permanent_tests;

  REQUIRE_EQ(binary_matrix_permanent(std::vector<std::uint64_t>{}).permanent,
             1ULL);
  REQUIRE_EQ(binary_matrix_permanent(std::vector<std::uint64_t>{0}).permanent,
             0ULL);
  REQUIRE_EQ(binary_matrix_permanent(std::vector<std::uint64_t>{1}).permanent,
             1ULL);

  const std::vector<std::uint64_t> identity{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
  verify_against_oracles(identity);
  REQUIRE_EQ(binary_matrix_permanent(identity).permanent, 1ULL);

  const std::vector<std::uint64_t> all_ones(4, 0b1111ULL);
  verify_against_oracles(all_ones);
  REQUIRE_EQ(binary_matrix_permanent(all_ones).permanent, 24ULL);

  const std::vector<std::uint64_t> two_matchings{0b110ULL, 0b101ULL, 0b011ULL};
  verify_against_oracles(two_matchings);
  REQUIRE_EQ(binary_matrix_permanent(two_matchings).permanent, 2ULL);

  const std::vector<std::uint64_t> triangular{0b001ULL, 0b011ULL, 0b111ULL};
  verify_against_oracles(triangular);
  REQUIRE_EQ(binary_matrix_permanent(triangular).permanent, 1ULL);

  const std::vector<std::uint64_t> hall_deficient{0b001ULL, 0b001ULL, 0b110ULL};
  verify_against_oracles(hall_deficient);
  REQUIRE_EQ(binary_matrix_permanent(hall_deficient).permanent, 0ULL);
}

TEST_CASE(binary_permanent_validation_and_bounds) {
  using namespace binary_permanent_tests;

  REQUIRE_THROWS_AS(binary_matrix_permanent(
                        std::vector<std::uint64_t>{0b100ULL, 0b01ULL}),
                    std::invalid_argument);

  const std::vector<std::uint64_t> boundary(20, 0ULL);
  const auto boundary_result = binary_matrix_permanent(boundary);
  REQUIRE_EQ(boundary_result.permanent, 0ULL);
  REQUIRE_EQ(boundary_result.dimension, std::size_t{20});
  REQUIRE_EQ(boundary_result.subsets_evaluated, 0ULL);

  const std::vector<std::uint64_t> too_large(21, 0ULL);
  REQUIRE_THROWS_AS(binary_matrix_permanent(too_large), std::length_error);

  const std::vector<std::uint64_t> all_ones_13(
      13, (std::uint64_t{1} << 13U) - 1ULL);
  const auto factorial_result = binary_matrix_permanent(all_ones_13);
  REQUIRE_EQ(factorial_result.permanent, 6227020800ULL);
  REQUIRE_EQ(factorial_result.subsets_evaluated,
             (std::uint64_t{1} << 13U) - 1ULL);
}

TEST_CASE(binary_permanent_randomized_permutation_differential) {
  using namespace binary_permanent_tests;

  std::mt19937_64 rng(0xB1A4E2D5ULL);
  for (int trial = 0; trial < 500; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 9ULL);
    const std::uint64_t mask =
        n == 0 ? 0ULL : ((std::uint64_t{1} << n) - 1ULL);
    std::vector<std::uint64_t> rows(n, 0ULL);
    for (std::size_t row = 0; row < n; ++row) {
      rows[row] = rng() & mask;
    }
    verify_against_oracles(rows);
  }
}
