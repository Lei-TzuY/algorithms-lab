#include "algorithms/streaming/misra_gries.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <vector>

namespace {

using algorithms::streaming::MisraGries;
using algorithms::streaming::MisraGriesSummary;
using algorithms::streaming::misra_gries_summary;

std::map<std::int64_t, std::size_t> exact_counts(
    const std::vector<std::int64_t>& stream) {
  std::map<std::int64_t, std::size_t> counts;
  for (const auto item : stream) {
    ++counts[item];
  }
  return counts;
}

std::size_t counter_value(const MisraGriesSummary& summary,
                          std::int64_t item) {
  for (const auto& counter : summary.counters) {
    if (counter.item == item) {
      return counter.residual_count;
    }
  }
  return 0U;
}

void verify_summary(const std::vector<std::int64_t>& stream,
                    const MisraGriesSummary& summary) {
  REQUIRE(summary.k >= 2U);
  REQUIRE_EQ(summary.processed_count, stream.size());
  REQUIRE(summary.counters.size() <= summary.k - 1U);
  REQUIRE(std::is_sorted(summary.counters.begin(), summary.counters.end(),
                         [](const auto& lhs, const auto& rhs) {
                           return lhs.item < rhs.item;
                         }));

  std::size_t residual_sum = 0U;
  for (const auto& counter : summary.counters) {
    REQUIRE(counter.residual_count > 0U);
    residual_sum += counter.residual_count;
  }

  REQUIRE(summary.decrement_rounds <= stream.size() / summary.k);
  REQUIRE(residual_sum <= stream.size());
  REQUIRE_EQ(stream.size() - residual_sum,
             summary.k * summary.decrement_rounds);

  const auto exact = exact_counts(stream);
  for (const auto& [item, frequency] : exact) {
    const auto residual = counter_value(summary, item);
    REQUIRE(residual <= frequency);
    REQUIRE(frequency - residual <= summary.decrement_rounds);
    if (frequency > stream.size() / summary.k) {
      REQUIRE(residual > 0U);
    }
  }
}

void enumerate_streams(std::vector<std::int64_t>& current,
                       std::size_t remaining,
                       const std::array<std::int64_t, 3>& alphabet,
                       std::size_t k,
                       std::size_t& checked) {
  if (remaining == 0U) {
    verify_summary(current, misra_gries_summary(current, k));
    ++checked;
    return;
  }
  for (const auto item : alphabet) {
    current.push_back(item);
    enumerate_streams(current, remaining - 1U, alphabet, k, checked);
    current.pop_back();
  }
}

}  // namespace

TEST_CASE(misra_gries_validation_streaming_and_one_shot_agree) {
  REQUIRE_THROWS_AS(MisraGries(0U), std::invalid_argument);
  REQUIRE_THROWS_AS(MisraGries(1U), std::invalid_argument);

  const std::vector<std::int64_t> stream{1, 2, 1, 3, 1, 2, 1};
  MisraGries state(3U);
  for (const auto item : stream) {
    state.update(item);
  }

  const auto incremental = state.summary();
  const auto one_shot = misra_gries_summary(stream, 3U);
  REQUIRE_EQ(incremental.counters, one_shot.counters);
  REQUIRE_EQ(incremental.decrement_rounds, one_shot.decrement_rounds);
  REQUIRE_EQ(state.counter_count(), incremental.counters.size());
  REQUIRE(state.is_candidate(1));
  REQUIRE_EQ(state.residual_count(1), counter_value(incremental, 1));
  verify_summary(stream, incremental);
}

TEST_CASE(misra_gries_exhaustive_bounded_streams_satisfy_guarantees) {
  const std::array<std::int64_t, 3> alphabet{-1, 0, 1};
  std::size_t checked = 0U;
  for (std::size_t k = 2U; k <= 4U; ++k) {
    for (std::size_t length = 0U; length <= 8U; ++length) {
      std::vector<std::int64_t> stream;
      enumerate_streams(stream, length, alphabet, k, checked);
    }
  }
  REQUIRE_EQ(checked, 29523U);
}

TEST_CASE(misra_gries_randomized_exact_frequency_differential) {
  std::mt19937_64 rng(0x16A16A16ULL);
  std::uniform_int_distribution<int> length_dist(0, 250);
  std::uniform_int_distribution<int> item_dist(-12, 12);
  std::uniform_int_distribution<int> k_dist(2, 10);

  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const auto length = static_cast<std::size_t>(length_dist(rng));
    const auto k = static_cast<std::size_t>(k_dist(rng));
    std::vector<std::int64_t> stream;
    stream.reserve(length);
    for (std::size_t index = 0U; index < length; ++index) {
      stream.push_back(static_cast<std::int64_t>(item_dist(rng)));
    }
    verify_summary(stream, misra_gries_summary(stream, k));
  }
}

TEST_CASE(misra_gries_cancellation_witness_and_output_are_deterministic) {
  const std::vector<std::int64_t> stream{10, 20, 30, 10, 40, 10, 20, 50,
                                         10, 20, 60, 10, 20, 70, 10};
  const auto first = misra_gries_summary(stream, 4U);
  const auto second = misra_gries_summary(stream, 4U);
  REQUIRE_EQ(first.counters, second.counters);
  REQUIRE_EQ(first.decrement_rounds, second.decrement_rounds);
  verify_summary(stream, first);
}
