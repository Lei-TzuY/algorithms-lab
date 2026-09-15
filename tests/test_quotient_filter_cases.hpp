#pragma once

#include "algorithms/data_structures/quotient_filter.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

TEST_CASE(quotient_filter_validation_duplicate_and_capacity_boundaries) {
  using algorithms::data_structures::QuotientFilter64;

  REQUIRE_THROWS_AS(QuotientFilter64(1U, 4U), std::invalid_argument);
  REQUIRE_THROWS_AS(QuotientFilter64(4U, 0U), std::invalid_argument);
  REQUIRE_THROWS_AS(QuotientFilter64(20U, 45U), std::invalid_argument);

  QuotientFilter64 filter(8U, 16U, 123U);
  REQUIRE(filter.empty());
  REQUIRE(filter.valid_structure());
  for (const std::uint64_t value :
       std::vector<std::uint64_t>{9U, 1U, 100U, 999U, 42U, 77U, 1000U, 12345U}) {
    REQUIRE(filter.insert(value));
    REQUIRE(filter.contains(value));
    REQUIRE(filter.valid_structure());
  }
  const std::size_t before_duplicate = filter.size();
  REQUIRE(!filter.insert(42U));
  REQUIRE_EQ(filter.size(), before_duplicate);
  REQUIRE(filter.valid_structure());

  QuotientFilter64 tiny(2U, 12U, 5U);
  std::uint64_t candidate = 0;
  while (tiny.size() < tiny.capacity()) {
    static_cast<void>(tiny.insert(candidate));
    ++candidate;
  }
  REQUIRE(tiny.valid_structure());
  bool overflowed = false;
  for (std::size_t attempts = 0; attempts < 100000U && !overflowed; ++attempts) {
    try {
      static_cast<void>(tiny.insert(candidate));
      ++candidate;
    } catch (const std::overflow_error&) {
      overflowed = true;
    }
  }
  REQUIRE(overflowed);
}

TEST_CASE(quotient_filter_canonical_state_is_insertion_order_independent) {
  using algorithms::data_structures::QuotientFilter64;

  QuotientFilter64 forward(9U, 18U, 7U);
  QuotientFilter64 reverse(9U, 18U, 7U);
  std::vector<std::uint64_t> values;
  for (std::uint64_t index = 0; index < 300U; ++index) {
    values.push_back(index * UINT64_C(1000003) + 17U);
  }
  for (const auto value : values) {
    REQUIRE(forward.insert(value));
  }
  for (auto it = values.rbegin(); it != values.rend(); ++it) {
    REQUIRE(reverse.insert(*it));
  }

  REQUIRE(forward.valid_structure());
  REQUIRE(reverse.valid_structure());
  REQUIRE_EQ(forward.debug_slots(), reverse.debug_slots());
}

TEST_CASE(quotient_filter_high_load_wraparound_preserves_membership_and_metadata) {
  using algorithms::data_structures::QuotientFilter64;

  std::mt19937_64 generator(UINT64_C(0x51464c544552));
  for (std::uint64_t trial = 0; trial < 300U; ++trial) {
    QuotientFilter64 filter(5U, 24U, trial * UINT64_C(0x9e3779b97f4a7c15));
    std::vector<std::uint64_t> attempted;
    while (filter.size() < filter.capacity()) {
      const std::uint64_t value = generator();
      attempted.push_back(value);
      static_cast<void>(filter.insert(value));
      REQUIRE(filter.valid_structure());
    }
    for (const auto value : attempted) {
      REQUIRE(filter.contains(value));
    }
  }
}

TEST_CASE(quotient_filter_randomized_no_false_negatives_against_exact_membership) {
  using algorithms::data_structures::QuotientFilter64;

  std::mt19937_64 generator(UINT64_C(0xC001D00D5EED));
  for (std::uint64_t trial = 0; trial < 240U; ++trial) {
    QuotientFilter64 filter(8U, 20U, trial + UINT64_C(0xA17C0DE));
    std::set<std::uint64_t> exact;
    while (exact.size() < 180U) {
      const std::uint64_t value = generator();
      exact.insert(value);
      static_cast<void>(filter.insert(value));
    }
    REQUIRE(filter.valid_structure());
    for (const auto value : exact) {
      REQUIRE(filter.contains(value));
    }
  }
}

TEST_CASE(quotient_filter_exposes_real_false_positive_boundary_and_replay) {
  using algorithms::data_structures::QuotientFilter64;

  QuotientFilter64 first(4U, 4U, 1U);
  QuotientFilter64 replay(4U, 4U, 1U);
  REQUIRE(first.insert(123456U));
  REQUIRE(replay.insert(123456U));
  REQUIRE_EQ(first.debug_slots(), replay.debug_slots());

  bool observed_false_positive = false;
  std::uint64_t false_positive_value = 0;
  for (std::uint64_t value = 0; value < 100000U && !observed_false_positive; ++value) {
    if (value != 123456U && first.contains(value)) {
      observed_false_positive = true;
      false_positive_value = value;
    }
  }
  REQUIRE(observed_false_positive);
  const std::size_t fingerprint_count = first.size();
  REQUIRE(!first.insert(false_positive_value));
  REQUIRE_EQ(first.size(), fingerprint_count);
  REQUIRE(first.valid_structure());

  QuotientFilter64 different_seed(4U, 4U, 2U);
  REQUIRE(different_seed.insert(123456U));
  REQUIRE(!(different_seed.debug_slots() == first.debug_slots()));
}
