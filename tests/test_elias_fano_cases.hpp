#pragma once

#include "algorithms/data_structures/elias_fano.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::data_structures::EliasFanoMonotoneSequence;

TEST_CASE(elias_fano_empty_and_monotone_validation) {
  const std::vector<std::uint64_t> empty;
  const EliasFanoMonotoneSequence index{std::span<const std::uint64_t>{empty}};
  REQUIRE(index.empty());
  REQUIRE_EQ(index.size(), 0U);
  REQUIRE_EQ(index.lower_bound_index(0U), 0U);
  REQUIRE(!index.predecessor_index(0U).has_value());
  REQUIRE(!index.contains(0U));
  REQUIRE(index.valid_structure());
  REQUIRE_THROWS_AS(index.at(0U), std::out_of_range);

  const std::vector<std::uint64_t> decreasing{1U, 4U, 3U};
  REQUIRE_THROWS_AS(
      EliasFanoMonotoneSequence(std::span<const std::uint64_t>{decreasing}),
      std::invalid_argument);
}

TEST_CASE(elias_fano_full_width_duplicates_and_boundaries) {
  const std::vector<std::uint64_t> values{
      0U, 0U, 1U, 5U, 5U, 5U,
      (std::uint64_t{1} << 63U),
      std::numeric_limits<std::uint64_t>::max()};
  const EliasFanoMonotoneSequence index{std::span<const std::uint64_t>{values}};
  REQUIRE_EQ(index.size(), values.size());
  for (std::size_t i = 0U; i < values.size(); ++i) {
    REQUIRE_EQ(index.at(i), values[i]);
  }
  REQUIRE_EQ(index.lower_bound_index(0U), 0U);
  REQUIRE_EQ(index.lower_bound_index(5U), 3U);
  REQUIRE_EQ(index.predecessor_index(5U), std::optional<std::size_t>{5U});
  REQUIRE_EQ(index.predecessor_index(4U), std::optional<std::size_t>{2U});
  REQUIRE(index.predecessor_index(0U).has_value());
  REQUIRE_EQ(*index.predecessor_index(0U), 1U);
  REQUIRE(index.contains(5U));
  REQUIRE(!index.contains(4U));
  REQUIRE(index.contains(std::numeric_limits<std::uint64_t>::max()));
  REQUIRE(index.valid_structure());
}

TEST_CASE(elias_fano_singleton_extremes) {
  const std::vector<std::uint64_t> zero{0U};
  const EliasFanoMonotoneSequence zero_index{std::span<const std::uint64_t>{zero}};
  REQUIRE_EQ(zero_index.low_bit_width(), 0U);
  REQUIRE_EQ(zero_index.at(0U), 0U);
  REQUIRE(zero_index.valid_structure());

  const std::vector<std::uint64_t> maximum{std::numeric_limits<std::uint64_t>::max()};
  const EliasFanoMonotoneSequence max_index{std::span<const std::uint64_t>{maximum}};
  REQUIRE_EQ(max_index.low_bit_width(), 63U);
  REQUIRE_EQ(max_index.high_bitvector_size(), 2U);
  REQUIRE_EQ(max_index.at(0U), std::numeric_limits<std::uint64_t>::max());
  REQUIRE(max_index.valid_structure());
}

TEST_CASE(elias_fano_randomized_differential) {
  std::mt19937_64 rng{0xE11A5FA0ULL};
  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 257U);
    std::vector<std::uint64_t> values(length);
    if (trial % 3U == 0U) {
      for (auto& value : values) {
        value = rng() % 33U;
      }
    } else {
      for (auto& value : values) {
        value = rng();
      }
    }
    std::sort(values.begin(), values.end());
    const EliasFanoMonotoneSequence index{std::span<const std::uint64_t>{values}};
    REQUIRE(index.valid_structure());
    REQUIRE_EQ(index.size(), values.size());
    for (std::size_t i = 0U; i < values.size(); ++i) {
      REQUIRE_EQ(index.at(i), values[i]);
    }
    for (std::size_t query_round = 0U; query_round < 80U; ++query_round) {
      const std::uint64_t query = rng();
      const auto lower = std::lower_bound(values.begin(), values.end(), query);
      const auto upper = std::upper_bound(values.begin(), values.end(), query);
      const std::size_t expected_lower = static_cast<std::size_t>(lower - values.begin());
      REQUIRE_EQ(index.lower_bound_index(query), expected_lower);
      const bool expected_contains = lower != values.end() && *lower == query;
      REQUIRE_EQ(index.contains(query), expected_contains);
      if (upper == values.begin()) {
        REQUIRE(!index.predecessor_index(query).has_value());
      } else {
        const auto expected = static_cast<std::size_t>((upper - values.begin()) - 1);
        REQUIRE_EQ(index.predecessor_index(query), std::optional<std::size_t>{expected});
      }
    }
  }
}

TEST_CASE(elias_fano_logical_bit_budget) {
  std::mt19937_64 rng{0xB17B0A4DULL};
  for (std::size_t trial = 0U; trial < 300U; ++trial) {
    const std::size_t length = 1U + static_cast<std::size_t>(rng() % 512U);
    std::vector<std::uint64_t> values(length);
    for (auto& value : values) {
      value = rng();
    }
    std::sort(values.begin(), values.end());
    const EliasFanoMonotoneSequence index{std::span<const std::uint64_t>{values}};
    REQUIRE(index.high_bitvector_size() < 3U * length);
    REQUIRE_EQ(index.encoded_bit_count(),
               index.high_bitvector_size() + length * index.low_bit_width());
    REQUIRE(index.logical_payload_bytes() >=
            (index.encoded_bit_count() + 7U) / 8U);
  }
}

}  // namespace
