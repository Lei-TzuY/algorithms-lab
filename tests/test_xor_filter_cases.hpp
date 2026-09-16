#pragma once

#include "algorithms/data_structures/xor_filter.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

TEST_CASE(xor_filter_empty_duplicate_and_replay_semantics) {
  using algorithms::data_structures::Xor8Filter64;

  REQUIRE_THROWS_AS(Xor8Filter64::build({1U}, 0U, 0U), std::invalid_argument);

  const auto empty = Xor8Filter64::build({}, 77U, 4U);
  REQUIRE(empty.empty());
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_EQ(empty.slot_count(), 0U);
  REQUIRE_EQ(empty.construction_attempts(), 0U);
  REQUIRE_EQ(empty.requested_seed(), UINT64_C(77));
  REQUIRE_EQ(empty.seed(), UINT64_C(77));
  REQUIRE(!empty.contains(0U));
  REQUIRE(!empty.contains(UINT64_MAX));

  const std::vector<std::uint64_t> values{
      9U, 1U, 9U, 42U, UINT64_MAX, 17U, 42U, 1000003U};
  auto forward = Xor8Filter64::build(values, UINT64_C(0x12345678), 64U);
  auto reversed = values;
  std::reverse(reversed.begin(), reversed.end());
  auto replay = Xor8Filter64::build(reversed, UINT64_C(0x12345678), 64U);

  REQUIRE_EQ(forward.size(), 6U);
  REQUIRE_EQ(forward.block_size(), 4U);
  REQUIRE_EQ(forward.slot_count(), 12U);
  REQUIRE_EQ(forward.seed(), replay.seed());
  REQUIRE_EQ(forward.construction_attempts(), replay.construction_attempts());
  REQUIRE_EQ(forward.debug_fingerprints(), replay.debug_fingerprints());
  for (const std::uint64_t value : values) {
    REQUIRE(forward.contains(value));
  }
}

TEST_CASE(xor_filter_surfaces_unpeelable_attempt_and_retries_deterministically) {
  using algorithms::data_structures::Xor8Filter64;

  std::vector<std::uint64_t> keys;
  for (std::uint64_t index = 0; index < 80U; ++index) {
    keys.push_back(index * UINT64_C(1000003) + 17U);
  }

  REQUIRE_THROWS_AS(Xor8Filter64::build(keys, 32U, 1U), std::runtime_error);
  const auto filter = Xor8Filter64::build(keys, 32U, 64U);
  REQUIRE_EQ(filter.construction_attempts(), 2U);
  REQUIRE_EQ(filter.seed(), UINT64_C(0x9e3779b97f4a7c35));
  for (const std::uint64_t key : keys) {
    REQUIRE(filter.contains(key));
  }
}

TEST_CASE(xor_filter_randomized_static_sets_have_no_false_negatives) {
  using algorithms::data_structures::Xor8Filter64;

  std::mt19937_64 generator(UINT64_C(0x584f5246494c5445));
  for (std::uint64_t trial = 0; trial < 300U; ++trial) {
    const std::size_t target = 1U + static_cast<std::size_t>(generator() % 240U);
    std::set<std::uint64_t> exact;
    while (exact.size() < target) {
      exact.insert(generator());
    }
    std::vector<std::uint64_t> keys(exact.begin(), exact.end());
    if ((trial & 1U) != 0U) {
      std::reverse(keys.begin(), keys.end());
    }
    const std::uint64_t base_seed =
        trial * UINT64_C(0xd6e8feb86659fd93) + UINT64_C(0xa17c0de);
    const auto filter = Xor8Filter64::build(keys, base_seed, 256U);
    REQUIRE_EQ(filter.size(), exact.size());
    REQUIRE_EQ(filter.slot_count(), 3U * (2U * (exact.size() / 3U) + exact.size() % 3U));
    REQUIRE(filter.construction_attempts() >= 1U);
    REQUIRE(filter.construction_attempts() <= 256U);
    for (const std::uint64_t key : exact) {
      REQUIRE(filter.contains(key));
    }

    const auto replay = Xor8Filter64::build(keys, base_seed, 256U);
    REQUIRE_EQ(replay.seed(), filter.seed());
    REQUIRE_EQ(replay.construction_attempts(), filter.construction_attempts());
    REQUIRE_EQ(replay.debug_fingerprints(), filter.debug_fingerprints());
  }
}

TEST_CASE(xor_filter_exposes_real_false_positive_boundary_without_exactness_claim) {
  using algorithms::data_structures::Xor8Filter64;

  std::vector<std::uint64_t> keys;
  for (std::uint64_t index = 0; index < 128U; ++index) {
    keys.push_back(index * UINT64_C(65537) + UINT64_C(0x1234));
  }
  const auto filter = Xor8Filter64::build(keys, UINT64_C(0xfeedface), 256U);
  const std::set<std::uint64_t> exact(keys.begin(), keys.end());

  bool observed_false_positive = false;
  std::uint64_t false_positive = 0U;
  for (std::uint64_t candidate = 0; candidate < 200000U; ++candidate) {
    if (!exact.contains(candidate) && filter.contains(candidate)) {
      observed_false_positive = true;
      false_positive = candidate;
      break;
    }
  }
  REQUIRE(observed_false_positive);
  REQUIRE(!exact.contains(false_positive));
  REQUIRE(filter.contains(false_positive));

  for (const std::uint64_t key : keys) {
    REQUIRE(filter.contains(key));
  }
}
