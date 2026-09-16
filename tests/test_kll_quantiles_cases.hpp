#pragma once

#include "algorithms/streaming/kll_quantiles.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace kll_recovery_test {

using algorithms::streaming::KllQuantileSketch;

inline std::uint64_t exact_rank(const std::vector<std::int64_t>& values,
                                std::int64_t query) {
  return static_cast<std::uint64_t>(
      std::count_if(values.begin(), values.end(),
                    [query](std::int64_t value) { return value <= query; }));
}

inline bool value_rank_intersects(const std::vector<std::int64_t>& stream,
                                  std::int64_t value,
                                  std::uint64_t target_rank,
                                  std::uint64_t error) {
  std::vector<std::int64_t> sorted = stream;
  std::sort(sorted.begin(), sorted.end());
  const auto first = std::lower_bound(sorted.begin(), sorted.end(), value);
  const auto after = std::upper_bound(sorted.begin(), sorted.end(), value);
  if (first == after) {
    return false;
  }
  const std::uint64_t first_rank =
      static_cast<std::uint64_t>(first - sorted.begin()) + 1U;
  const std::uint64_t last_rank =
      static_cast<std::uint64_t>(after - sorted.begin());
  const std::uint64_t lower =
      target_rank > error ? target_rank - error : 1U;
  const std::uint64_t upper =
      error > std::numeric_limits<std::uint64_t>::max() - target_rank
          ? std::numeric_limits<std::uint64_t>::max()
          : target_rank + error;
  return first_rank <= upper && last_rank >= lower;
}

}  // namespace kll_recovery_test

TEST_CASE(kll_quantiles_validation_empty_and_exact_small_regime) {
  using kll_recovery_test::KllQuantileSketch;
  REQUIRE_THROWS_AS(KllQuantileSketch(0U, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(KllQuantileSketch(1U, 1U), std::invalid_argument);

  KllQuantileSketch empty(8U, 0x1234U);
  REQUIRE(empty.valid_state());
  REQUIRE_EQ(empty.count(), 0U);
  REQUIRE_EQ(empty.retained_count(), 0U);
  REQUIRE_EQ(empty.level_count(), 1U);
  REQUIRE_EQ(empty.level_capacity(0U), 9U);
  REQUIRE(!empty.query_rank(1U).has_value());
  REQUIRE_THROWS_AS(empty.level_capacity(1U), std::out_of_range);

  KllQuantileSketch exact(16U, 7U);
  const std::vector<std::int64_t> values = {
      std::numeric_limits<std::int64_t>::min(), -2, 0, 0, 9,
      std::numeric_limits<std::int64_t>::max()};
  for (const std::int64_t value : values) {
    exact.insert(value);
  }
  REQUIRE(exact.valid_state());
  REQUIRE_EQ(exact.compaction_count(), 0U);
  REQUIRE_EQ(exact.deterministic_rank_error_bound(), 0U);
  for (std::uint64_t rank = 1U; rank <= values.size(); ++rank) {
    const auto estimate = exact.query_rank(rank);
    REQUIRE(estimate.has_value());
    REQUIRE(kll_recovery_test::value_rank_intersects(values, estimate->value,
                                                     rank, 0U));
  }
  REQUIRE_THROWS_AS(exact.query_rank(0U), std::out_of_range);
  REQUIRE_THROWS_AS(exact.query_rank(values.size() + 1U), std::out_of_range);
}

TEST_CASE(kll_quantiles_replay_weight_conservation_and_real_compression) {
  using kll_recovery_test::KllQuantileSketch;
  std::vector<std::int64_t> stream;
  for (std::int64_t index = 0; index < 5000; ++index) {
    stream.push_back((index * 37) % 997 - 400);
  }

  KllQuantileSketch first(64U, 0xC011AC7ULL);
  KllQuantileSketch replay(64U, 0xC011AC7ULL);
  for (const std::int64_t value : stream) {
    first.insert(value);
    replay.insert(value);
    REQUIRE(first.valid_state());
  }
  REQUIRE(first.levels() == replay.levels());
  REQUIRE_EQ(first.compaction_count(), replay.compaction_count());
  REQUIRE(first.compaction_count() > 0U);
  REQUIRE(first.retained_count() < stream.size() / 2U);
  REQUIRE(first.deterministic_rank_error_bound() < first.count());

  std::uint64_t retained_mass = 0U;
  for (const auto& sample : first.weighted_samples()) {
    retained_mass += sample.weight;
  }
  REQUIRE_EQ(retained_mass, first.count());
}

TEST_CASE(kll_quantiles_deterministic_certificate_bounds_exact_rank_error) {
  using kll_recovery_test::KllQuantileSketch;
  std::mt19937_64 rng(0x4B4C4C43455254ULL);
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    KllQuantileSketch sketch(24U, rng());
    std::vector<std::int64_t> stream;
    const std::size_t length = 40U + static_cast<std::size_t>(rng() % 220U);
    stream.reserve(length);
    for (std::size_t index = 0U; index < length; ++index) {
      const std::int64_t value = static_cast<std::int64_t>(rng() % 101U) - 50;
      stream.push_back(value);
      sketch.insert(value);
      REQUIRE(sketch.valid_state());
    }

    const std::uint64_t bound = sketch.deterministic_rank_error_bound();
    for (std::int64_t query = -55; query <= 55; ++query) {
      const std::uint64_t exact = kll_recovery_test::exact_rank(stream, query);
      const std::uint64_t approximate = sketch.estimated_rank(query);
      const std::uint64_t difference =
          exact > approximate ? exact - approximate : approximate - exact;
      REQUIRE(difference <= bound);
    }

    for (std::size_t query = 0U; query < 40U; ++query) {
      const std::uint64_t rank = 1U + (rng() % sketch.count());
      const auto estimate = sketch.query_rank(rank);
      REQUIRE(estimate.has_value());
      REQUIRE(kll_recovery_test::value_rank_intersects(
          stream, estimate->value, rank, bound));
    }
  }
}

TEST_CASE(kll_quantiles_finite_randomized_run_is_not_exact) {
  using kll_recovery_test::KllQuantileSketch;
  std::vector<std::int64_t> stream;
  for (std::int64_t value = 0; value < 64; ++value) {
    stream.push_back(value);
  }

  bool found_non_exact = false;
  for (std::uint64_t seed = 0U; seed < 64U && !found_non_exact; ++seed) {
    KllQuantileSketch sketch(2U, seed);
    for (const std::int64_t value : stream) {
      sketch.insert(value);
    }
    const auto estimate = sketch.query_rank(32U);
    REQUIRE(estimate.has_value());
    if (estimate->value != 31) {
      found_non_exact = true;
      const std::uint64_t exact =
          kll_recovery_test::exact_rank(stream, estimate->value);
      const std::uint64_t difference =
          exact > 32U ? exact - 32U : 32U - exact;
      REQUIRE(difference <= sketch.deterministic_rank_error_bound());
    }
  }
  REQUIRE(found_non_exact);
}

TEST_CASE(kll_quantiles_weighted_query_oracle_and_seed_replay) {
  using kll_recovery_test::KllQuantileSketch;
  std::mt19937_64 rng(0x4B4C4C44494646ULL);
  bool different_seed_diverged = false;
  for (std::size_t trial = 0U; trial < 160U; ++trial) {
    const std::uint64_t seed = rng();
    KllQuantileSketch first(32U, seed);
    KllQuantileSketch replay(32U, seed);
    KllQuantileSketch other_seed(32U, seed + 1U);
    const std::size_t length = 100U + static_cast<std::size_t>(rng() % 300U);
    for (std::size_t index = 0U; index < length; ++index) {
      const std::int64_t value = static_cast<std::int64_t>(rng() % 501U) - 250;
      first.insert(value);
      replay.insert(value);
      other_seed.insert(value);
    }
    REQUIRE(first.valid_state());
    REQUIRE(replay.valid_state());
    REQUIRE(other_seed.valid_state());
    REQUIRE(first.levels() == replay.levels());
    if (first.levels() != other_seed.levels()) {
      different_seed_diverged = true;
    }

    const auto samples = first.weighted_samples();
    for (std::size_t query = 0U; query < 30U; ++query) {
      const std::uint64_t rank = 1U + (rng() % first.count());
      const auto estimate = first.query_rank(rank);
      REQUIRE(estimate.has_value());
      std::uint64_t cumulative = 0U;
      std::int64_t oracle = 0;
      for (const auto& sample : samples) {
        cumulative += sample.weight;
        if (cumulative >= rank) {
          oracle = sample.value;
          break;
        }
      }
      REQUIRE_EQ(estimate->value, oracle);
    }
  }
  REQUIRE(different_seed_diverged);
}

TEST_CASE(kll_quantiles_capacity_hierarchy_obeys_two_thirds_lower_bound) {
  using kll_recovery_test::KllQuantileSketch;
  KllQuantileSketch sketch(64U, 99U);
  for (std::int64_t value = 0; value < 10000; ++value) {
    sketch.insert(value);
  }
  REQUIRE(sketch.valid_state());
  REQUIRE(sketch.level_count() > 2U);
  for (std::size_t level = 0U; level < sketch.level_count(); ++level) {
    REQUIRE(sketch.levels()[level].size() < sketch.level_capacity(level));
  }
}
