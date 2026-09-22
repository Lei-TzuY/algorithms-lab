#pragma once

#include "algorithms/combinatorial/stirling_transform.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::combinatorial::inverse_stirling_transform;
using algorithms::combinatorial::stirling_transform;
using algorithms::combinatorial::stirling_transform_tables;

namespace stirling_transform_test_detail {

inline std::vector<std::uint64_t> partition_counts(
    const std::size_t n) {
  std::vector<std::uint64_t> counts(n + 1U, 0U);
  if (n == 0U) {
    counts[0U] = 1U;
    return counts;
  }

  std::vector<std::size_t> blocks(n, 0U);
  const auto visit = [&](const auto& self, const std::size_t position,
                         const std::size_t max_block) -> void {
    if (position == n) {
      ++counts[max_block + 1U];
      return;
    }
    for (std::size_t block = 0U; block <= max_block + 1U; ++block) {
      blocks[position] = block;
      self(self, position + 1U, std::max(max_block, block));
    }
  };

  blocks[0U] = 0U;
  visit(visit, 1U, 0U);
  return counts;
}

inline std::vector<std::uint64_t> permutation_cycle_counts(
    const std::size_t n) {
  std::vector<std::uint64_t> counts(n + 1U, 0U);
  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(), 0U);

  do {
    std::vector<bool> seen(n, false);
    std::size_t cycles = 0U;
    for (std::size_t start = 0U; start < n; ++start) {
      if (seen[start]) {
        continue;
      }
      ++cycles;
      std::size_t current = start;
      while (!seen[current]) {
        seen[current] = true;
        current = permutation[current];
      }
    }
    ++counts[cycles];
  } while (std::next_permutation(
      permutation.begin(), permutation.end()));

  return counts;
}

inline std::uint64_t signed_residue(
    const std::uint64_t magnitude, const bool negative,
    const std::uint64_t modulus) {
  const std::uint64_t residue = magnitude % modulus;
  if (!negative || residue == 0U) {
    return residue;
  }
  return modulus - residue;
}

}  // namespace stirling_transform_test_detail

TEST_CASE(stirling_transform_known_rows_and_bell_transform) {
  constexpr std::uint64_t modulus = 1000003U;
  const auto tables = stirling_transform_tables(6U, modulus);

  REQUIRE(tables.second_kind[4U] ==
          std::vector<std::uint64_t>({0U, 1U, 7U, 6U, 1U, 0U, 0U}));
  REQUIRE(tables.signed_first_kind[4U] ==
          std::vector<std::uint64_t>(
              {0U, modulus - 6U, 11U, modulus - 6U, 1U, 0U, 0U}));

  const std::vector<std::uint64_t> ones(
      7U, 1U);
  const std::vector<std::uint64_t> bells{
      1U, 1U, 2U, 5U, 15U, 52U, 203U};

  REQUIRE(stirling_transform(ones, modulus) == bells);
  REQUIRE(inverse_stirling_transform(bells, modulus) == ones);
}

TEST_CASE(stirling_transform_rejects_invalid_modulus_and_residues) {
  REQUIRE_THROWS_AS(
      stirling_transform_tables(3U, 0U), std::invalid_argument);
  REQUIRE_THROWS_AS(
      stirling_transform_tables(3U, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(
      stirling_transform({0U, 7U}, 7U), std::invalid_argument);
  REQUIRE_THROWS_AS(
      inverse_stirling_transform({9U}, 9U), std::invalid_argument);
  REQUIRE_THROWS_AS(
      stirling_transform_tables(
          std::numeric_limits<std::size_t>::max(), 17U),
      std::length_error);

  REQUIRE(stirling_transform({}, 2U).empty());
  REQUIRE(inverse_stirling_transform({}, 2U).empty());
}

TEST_CASE(stirling_tables_match_independent_combinatorial_enumeration) {
  using namespace stirling_transform_test_detail;

  constexpr std::uint64_t modulus = 1000003U;
  constexpr std::size_t max_n = 7U;
  const auto tables = stirling_transform_tables(max_n, modulus);

  for (std::size_t n = 0U; n <= max_n; ++n) {
    const auto partitions = partition_counts(n);
    const auto cycles = permutation_cycle_counts(n);

    for (std::size_t k = 0U; k <= n; ++k) {
      REQUIRE_EQ(
          tables.second_kind[n][k],
          partitions[k] % modulus);

      const bool negative = ((n - k) & 1U) != 0U;
      REQUIRE_EQ(
          tables.signed_first_kind[n][k],
          signed_residue(cycles[k], negative, modulus));
    }

    for (std::size_t k = n + 1U; k <= max_n; ++k) {
      REQUIRE_EQ(tables.second_kind[n][k], 0U);
      REQUIRE_EQ(tables.signed_first_kind[n][k], 0U);
    }
  }
}

TEST_CASE(stirling_forward_inverse_random_vectors_multiple_moduli) {
  std::mt19937_64 random(0x571A11A6ULL);
  const std::vector<std::uint64_t> moduli{
      2U,
      6U,
      97U,
      1000U,
      UINT64_C(18446744073709551557)};

  for (const std::uint64_t modulus : moduli) {
    for (std::size_t trial = 0U; trial < 180U; ++trial) {
      const std::size_t size =
          static_cast<std::size_t>(random() % 22U);
      std::vector<std::uint64_t> values(size);
      for (std::uint64_t& value : values) {
        value = random() % modulus;
      }

      const auto transformed =
          stirling_transform(values, modulus);
      const auto restored =
          inverse_stirling_transform(transformed, modulus);
      REQUIRE(restored == values);

      const auto inverse_first =
          inverse_stirling_transform(values, modulus);
      REQUIRE(stirling_transform(inverse_first, modulus) == values);
    }
  }
}

TEST_CASE(stirling_transform_basis_vectors_replay_tables) {
  constexpr std::uint64_t modulus = 101U;
  constexpr std::size_t size = 12U;
  const auto tables =
      stirling_transform_tables(size - 1U, modulus);

  for (std::size_t basis = 0U; basis < size; ++basis) {
    std::vector<std::uint64_t> vector(size, 0U);
    vector[basis] = 1U;

    const auto forward = stirling_transform(vector, modulus);
    const auto inverse =
        inverse_stirling_transform(vector, modulus);

    for (std::size_t n = 0U; n < size; ++n) {
      const std::uint64_t expected_second =
          basis <= n ? tables.second_kind[n][basis] : 0U;
      const std::uint64_t expected_first =
          basis <= n ? tables.signed_first_kind[n][basis] : 0U;
      REQUIRE_EQ(forward[n], expected_second);
      REQUIRE_EQ(inverse[n], expected_first);
    }
  }
}
