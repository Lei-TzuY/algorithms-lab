#pragma once

#include "algorithms/approximation/weighted_set_cover.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace weighted_set_cover_test_detail {

using algorithms::approximation::WeightedCoverSet;
using algorithms::approximation::WeightedSetCoverResult;

[[nodiscard]] inline bool subset_covers(
    std::size_t universe_size, const std::vector<WeightedCoverSet>& sets,
    std::uint64_t mask) {
  std::vector<bool> covered(universe_size, false);
  for (std::size_t i = 0U; i < sets.size(); ++i) {
    if ((mask & (std::uint64_t{1} << i)) == 0U) {
      continue;
    }
    for (const std::size_t element : sets[i].elements) {
      if (element < universe_size) {
        covered[element] = true;
      }
    }
  }
  return std::all_of(covered.begin(), covered.end(), [](bool value) {
    return value;
  });
}

[[nodiscard]] inline std::optional<std::uint64_t> exhaustive_optimum(
    std::size_t universe_size, const std::vector<WeightedCoverSet>& sets) {
  if (sets.size() >= 63U) {
    throw std::logic_error("test oracle only supports fewer than 63 sets");
  }
  std::optional<std::uint64_t> best;
  const std::uint64_t subset_count = std::uint64_t{1} << sets.size();
  for (std::uint64_t mask = 0U; mask < subset_count; ++mask) {
    if (!subset_covers(universe_size, sets, mask)) {
      continue;
    }
    std::uint64_t cost = 0U;
    bool overflow = false;
    for (std::size_t i = 0U; i < sets.size(); ++i) {
      if ((mask & (std::uint64_t{1} << i)) == 0U) {
        continue;
      }
      if (sets[i].cost > std::numeric_limits<std::uint64_t>::max() - cost) {
        overflow = true;
        break;
      }
      cost += sets[i].cost;
    }
    if (!overflow && (!best.has_value() || cost < *best)) {
      best = cost;
    }
  }
  return best;
}

inline void replay_result(std::size_t universe_size,
                          const std::vector<WeightedCoverSet>& sets,
                          const WeightedSetCoverResult& result) {
  REQUIRE_EQ(result.first_cover_set.size(), universe_size);
  REQUIRE_EQ(result.steps.size(), result.selected_set_indices.size());
  std::vector<bool> covered(universe_size, false);
  std::uint64_t total = 0U;
  std::size_t covered_count = 0U;

  for (std::size_t step_index = 0U; step_index < result.steps.size();
       ++step_index) {
    const auto& step = result.steps[step_index];
    REQUIRE_EQ(step.set_index, result.selected_set_indices[step_index]);
    REQUIRE(step.set_index < sets.size());

    std::size_t chosen_gain = 0U;
    std::vector<bool> chosen_seen(universe_size, false);
    for (const std::size_t element : sets[step.set_index].elements) {
      REQUIRE(element < universe_size);
      if (!chosen_seen[element]) {
        chosen_seen[element] = true;
        if (!covered[element]) {
          ++chosen_gain;
        }
      }
    }
    REQUIRE_EQ(chosen_gain, step.newly_covered);
    REQUIRE(chosen_gain > 0U);

    // Randomized tests keep costs/gains small enough that these products are an
    // independent, ordinary-integer replay of the greedy ratio choice.
    for (std::size_t candidate = 0U; candidate < sets.size(); ++candidate) {
      std::size_t gain = 0U;
      std::vector<bool> seen(universe_size, false);
      for (const std::size_t element : sets[candidate].elements) {
        if (!seen[element]) {
          seen[element] = true;
          if (!covered[element]) {
            ++gain;
          }
        }
      }
      if (gain == 0U) {
        continue;
      }
      const std::uint64_t lhs =
          sets[step.set_index].cost * static_cast<std::uint64_t>(gain);
      const std::uint64_t rhs =
          sets[candidate].cost * static_cast<std::uint64_t>(chosen_gain);
      REQUIRE(lhs <= rhs);
      if (lhs == rhs && candidate < step.set_index) {
        testfw::fail("greedy ratio tie must choose the smaller set index",
                     __FILE__, __LINE__);
      }
    }

    REQUIRE(sets[step.set_index].cost <=
            std::numeric_limits<std::uint64_t>::max() - total);
    total += sets[step.set_index].cost;
    for (const std::size_t element : sets[step.set_index].elements) {
      if (!covered[element]) {
        covered[element] = true;
        ++covered_count;
        REQUIRE(result.first_cover_set[element].has_value());
        REQUIRE_EQ(*result.first_cover_set[element], step.set_index);
      }
    }
    REQUIRE_EQ(step.covered_after, covered_count);
  }

  REQUIRE_EQ(covered_count, universe_size);
  REQUIRE_EQ(total, result.total_cost);
}

[[nodiscard]] inline std::uint64_t harmonic_numerator(std::size_t n,
                                                       std::uint64_t lcm) {
  std::uint64_t numerator = 0U;
  for (std::size_t k = 1U; k <= n; ++k) {
    numerator += lcm / static_cast<std::uint64_t>(k);
  }
  return numerator;
}

[[nodiscard]] inline std::uint64_t lcm_one_to_n(std::size_t n) {
  std::uint64_t value = 1U;
  for (std::size_t k = 1U; k <= n; ++k) {
    value = std::lcm(value, static_cast<std::uint64_t>(k));
  }
  return value;
}

}  // namespace weighted_set_cover_test_detail

TEST_CASE(weighted_set_cover_deterministic_contracts) {
  using algorithms::approximation::WeightedCoverSet;
  using algorithms::approximation::greedy_weighted_set_cover;

  const auto empty = greedy_weighted_set_cover(
      0U, std::vector<WeightedCoverSet>{{7U, {}}});
  REQUIRE(empty.has_value());
  REQUIRE_EQ(empty->total_cost, 0U);
  REQUIRE(empty->selected_set_indices.empty());

  REQUIRE_THROWS_AS(
      greedy_weighted_set_cover(0U,
                                std::vector<WeightedCoverSet>{{0U, {0U}}}),
      std::out_of_range);

  const std::vector<WeightedCoverSet> sets{{6U, {0U, 1U, 1U}},
                                           {2U, {0U}},
                                           {2U, {1U}},
                                           {1U, {2U}},
                                           {100U, {}}};
  const auto result = greedy_weighted_set_cover(3U, sets);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->selected_set_indices,
             (std::vector<std::size_t>{3U, 1U, 2U}));
  REQUIRE_EQ(result->total_cost, 5U);
  weighted_set_cover_test_detail::replay_result(3U, sets, *result);

  const std::vector<WeightedCoverSet> tie{{2U, {0U}}, {4U, {0U, 1U}}};
  const auto tie_result = greedy_weighted_set_cover(2U, tie);
  REQUIRE(tie_result.has_value());
  REQUIRE_EQ(tie_result->selected_set_indices.front(), 0U);

  const auto impossible = greedy_weighted_set_cover(
      3U, std::vector<WeightedCoverSet>{{1U, {0U, 1U}}});
  REQUIRE(!impossible.has_value());
}

TEST_CASE(weighted_set_cover_full_width_ratio_and_cost_boundaries) {
  using algorithms::approximation::WeightedCoverSet;
  using algorithms::approximation::greedy_weighted_set_cover;

  const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  const std::vector<WeightedCoverSet> ratio_sets{
      {maximum, {0U, 1U}}, {maximum / 2U + 1U, {0U}},
      {maximum / 2U + 1U, {1U}}};
  const auto ratio_result = greedy_weighted_set_cover(2U, ratio_sets);
  REQUIRE(ratio_result.has_value());
  REQUIRE_EQ(ratio_result->selected_set_indices,
             (std::vector<std::size_t>{0U}));
  REQUIRE_EQ(ratio_result->total_cost, maximum);

  REQUIRE_THROWS_AS(
      greedy_weighted_set_cover(
          2U, std::vector<WeightedCoverSet>{{maximum, {0U}},
                                             {maximum, {1U}}}),
      std::overflow_error);

  const auto zero_cost = greedy_weighted_set_cover(
      3U, std::vector<WeightedCoverSet>{{0U, {0U, 1U}},
                                         {0U, {2U}},
                                         {1U, {0U, 1U, 2U}}});
  REQUIRE(zero_cost.has_value());
  REQUIRE_EQ(zero_cost->total_cost, 0U);
}

TEST_CASE(weighted_set_cover_exhaustive_random_differential) {
  using algorithms::approximation::WeightedCoverSet;
  using algorithms::approximation::greedy_weighted_set_cover;

  std::mt19937_64 rng(0x5E7C0A3ULL);
  for (std::size_t trial = 0U; trial < 800U; ++trial) {
    const std::size_t universe_size = static_cast<std::size_t>(rng() % 9U);
    const std::size_t set_count = static_cast<std::size_t>(rng() % 11U);
    std::vector<WeightedCoverSet> sets;
    sets.reserve(set_count);
    for (std::size_t i = 0U; i < set_count; ++i) {
      WeightedCoverSet set;
      set.cost = rng() % 31U;
      const std::size_t mentions =
          universe_size == 0U
              ? 0U
              : static_cast<std::size_t>(rng() % (universe_size * 2U + 1U));
      for (std::size_t j = 0U; j < mentions; ++j) {
        set.elements.push_back(
            static_cast<std::size_t>(rng() % universe_size));
      }
      sets.push_back(std::move(set));
    }

    const auto optimum =
        weighted_set_cover_test_detail::exhaustive_optimum(universe_size, sets);
    const auto result = greedy_weighted_set_cover(universe_size, sets);
    REQUIRE_EQ(result.has_value(), optimum.has_value());
    if (!result.has_value()) {
      continue;
    }

    weighted_set_cover_test_detail::replay_result(universe_size, sets, *result);
    REQUIRE(optimum.has_value());
    if (universe_size == 0U) {
      REQUIRE_EQ(result->total_cost, 0U);
      REQUIRE_EQ(*optimum, 0U);
      continue;
    }

    const std::uint64_t denominator =
        weighted_set_cover_test_detail::lcm_one_to_n(universe_size);
    const std::uint64_t numerator =
        weighted_set_cover_test_detail::harmonic_numerator(universe_size,
                                                           denominator);
    REQUIRE(result->total_cost * denominator <= (*optimum) * numerator);
  }
}

TEST_CASE(weighted_set_cover_repeated_execution_is_deterministic) {
  using algorithms::approximation::WeightedCoverSet;
  using algorithms::approximation::greedy_weighted_set_cover;

  const std::vector<WeightedCoverSet> sets{{5U, {0U, 2U, 4U}},
                                           {3U, {0U, 1U}},
                                           {3U, {2U, 3U}},
                                           {2U, {4U}},
                                           {4U, {1U, 3U, 4U}}};
  const auto first = greedy_weighted_set_cover(5U, sets);
  const auto second = greedy_weighted_set_cover(5U, sets);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE_EQ(*first, *second);
}
