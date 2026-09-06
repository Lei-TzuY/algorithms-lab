#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/dynamic_programming/knapsack.hpp"

namespace {

using algorithms::dynamic_programming::KnapsackItem;
using algorithms::dynamic_programming::UnboundedKnapsackResult;
using algorithms::dynamic_programming::ZeroOneKnapsackResult;

std::int64_t brute_force_zero_one(const std::vector<KnapsackItem>& items,
                                  std::size_t capacity) {
  REQUIRE(items.size() <= 20);
  const std::uint64_t limit = std::uint64_t{1} << items.size();
  std::int64_t best = 0;
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    std::size_t weight = 0;
    std::int64_t value = 0;
    bool feasible = true;
    for (std::size_t index = 0; index < items.size(); ++index) {
      if ((mask & (std::uint64_t{1} << index)) == 0U) {
        continue;
      }
      if (items[index].weight > capacity - weight) {
        feasible = false;
        break;
      }
      weight += items[index].weight;
      value += items[index].value;
    }
    if (feasible) {
      best = std::max(best, value);
    }
  }
  return best;
}

std::int64_t unbounded_oracle_from_capacity(
    const std::vector<KnapsackItem>& items, std::size_t capacity,
    std::vector<std::optional<std::int64_t>>& memo) {
  if (memo[capacity].has_value()) {
    return *memo[capacity];
  }
  std::int64_t best = 0;
  for (const KnapsackItem& item : items) {
    if (item.weight <= capacity) {
      best = std::max(best, item.value + unbounded_oracle_from_capacity(
                                             items, capacity - item.weight, memo));
    }
  }
  memo[capacity] = best;
  return best;
}

std::int64_t unbounded_oracle(const std::vector<KnapsackItem>& items,
                              std::size_t capacity) {
  std::vector<std::optional<std::int64_t>> memo(capacity + 1);
  memo[0] = 0;
  return unbounded_oracle_from_capacity(items, capacity, memo);
}

void require_valid_zero_one(const std::vector<KnapsackItem>& items,
                            std::size_t capacity,
                            const ZeroOneKnapsackResult& result) {
  std::vector<bool> seen(items.size(), false);
  std::size_t weight = 0;
  std::int64_t value = 0;
  for (const std::size_t index : result.selected_indices) {
    REQUIRE(index < items.size());
    REQUIRE(!seen[index]);
    seen[index] = true;
    REQUIRE(items[index].weight <= capacity - weight);
    weight += items[index].weight;
    value += items[index].value;
  }
  REQUIRE(weight <= capacity);
  REQUIRE_EQ(weight, result.total_weight);
  REQUIRE_EQ(value, result.best_value);
}

void require_valid_unbounded(const std::vector<KnapsackItem>& items,
                             std::size_t capacity,
                             const UnboundedKnapsackResult& result) {
  REQUIRE_EQ(result.item_counts.size(), items.size());
  std::size_t weight = 0;
  std::int64_t value = 0;
  for (std::size_t index = 0; index < items.size(); ++index) {
    for (std::size_t copy = 0; copy < result.item_counts[index]; ++copy) {
      REQUIRE(items[index].weight <= capacity - weight);
      weight += items[index].weight;
      value += items[index].value;
    }
  }
  REQUIRE(weight <= capacity);
  REQUIRE_EQ(weight, result.total_weight);
  REQUIRE_EQ(value, result.best_value);
}

}  // namespace

TEST_CASE(zero_one_knapsack_handles_classic_empty_zero_capacity_and_zero_weight) {
  const std::vector<KnapsackItem> classic{{10, 60}, {20, 100}, {30, 120}};
  const auto result =
      algorithms::dynamic_programming::solve_zero_one_knapsack(classic, 50);
  REQUIRE_EQ(result.best_value, std::int64_t{220});
  REQUIRE_EQ(result.total_weight, std::size_t{50});
  REQUIRE_EQ(result.selected_indices, (std::vector<std::size_t>{1, 2}));
  require_valid_zero_one(classic, 50, result);

  const std::vector<KnapsackItem> empty;
  const auto empty_result =
      algorithms::dynamic_programming::solve_zero_one_knapsack(empty, 10);
  REQUIRE_EQ(empty_result.best_value, std::int64_t{0});
  REQUIRE(empty_result.selected_indices.empty());

  const std::vector<KnapsackItem> zero_weight{{0, 5}, {2, 4}, {0, -100}};
  const auto zero_weight_result =
      algorithms::dynamic_programming::solve_zero_one_knapsack(zero_weight, 2);
  REQUIRE_EQ(zero_weight_result.best_value, std::int64_t{9});
  REQUIRE_EQ(zero_weight_result.total_weight, std::size_t{2});
  require_valid_zero_one(zero_weight, 2, zero_weight_result);
}

TEST_CASE(zero_one_knapsack_prefers_empty_over_negative_value_and_is_deterministic_on_ties) {
  const std::vector<KnapsackItem> negative{{1, -5}, {2, -1}};
  const auto negative_result =
      algorithms::dynamic_programming::solve_zero_one_knapsack(negative, 3);
  REQUIRE_EQ(negative_result.best_value, std::int64_t{0});
  REQUIRE(negative_result.selected_indices.empty());

  const std::vector<KnapsackItem> ties{{2, 7}, {2, 7}};
  const auto tie_result =
      algorithms::dynamic_programming::solve_zero_one_knapsack(ties, 2);
  REQUIRE_EQ(tie_result.selected_indices,
             (std::vector<std::size_t>{0}));
}

TEST_CASE(zero_one_knapsack_matches_exhaustive_randomized_oracle) {
  std::mt19937_64 rng(0x01BADC0DEULL);
  std::uniform_int_distribution<int> count_distribution(0, 12);
  std::uniform_int_distribution<int> capacity_distribution(0, 20);
  std::uniform_int_distribution<int> weight_distribution(0, 8);
  std::uniform_int_distribution<int> value_distribution(-8, 25);

  for (std::size_t trial = 0; trial < 320; ++trial) {
    const std::size_t count =
        static_cast<std::size_t>(count_distribution(rng));
    const std::size_t capacity =
        static_cast<std::size_t>(capacity_distribution(rng));
    std::vector<KnapsackItem> items;
    items.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      items.push_back(KnapsackItem{
          static_cast<std::size_t>(weight_distribution(rng)),
          static_cast<std::int64_t>(value_distribution(rng))});
    }

    const auto result =
        algorithms::dynamic_programming::solve_zero_one_knapsack(items,
                                                                  capacity);
    REQUIRE_EQ(result.best_value, brute_force_zero_one(items, capacity));
    require_valid_zero_one(items, capacity, result);
  }
}

TEST_CASE(unbounded_knapsack_handles_classic_repetition_and_negative_values) {
  const std::vector<KnapsackItem> classic{{10, 60}, {20, 100}, {30, 120}};
  const auto result =
      algorithms::dynamic_programming::solve_unbounded_knapsack(classic, 50);
  REQUIRE_EQ(result.best_value, std::int64_t{300});
  REQUIRE_EQ(result.total_weight, std::size_t{50});
  REQUIRE_EQ(result.item_counts[0], std::size_t{5});
  require_valid_unbounded(classic, 50, result);

  const std::vector<KnapsackItem> negative{{1, -5}, {3, -1}};
  const auto negative_result =
      algorithms::dynamic_programming::solve_unbounded_knapsack(negative, 9);
  REQUIRE_EQ(negative_result.best_value, std::int64_t{0});
  REQUIRE_EQ(negative_result.total_weight, std::size_t{0});
  require_valid_unbounded(negative, 9, negative_result);
}

TEST_CASE(unbounded_knapsack_rejects_zero_weight_items) {
  const std::vector<KnapsackItem> positive{{0, 1}};
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::solve_unbounded_knapsack(positive, 10),
      std::invalid_argument);

  const std::vector<KnapsackItem> non_positive{{0, 0}, {1, 2}};
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::solve_unbounded_knapsack(non_positive,
                                                                 10),
      std::invalid_argument);
}

TEST_CASE(unbounded_knapsack_matches_independent_capacity_recursion_oracle) {
  std::mt19937_64 rng(0x0BADC0FFEEULL);
  std::uniform_int_distribution<int> count_distribution(0, 7);
  std::uniform_int_distribution<int> capacity_distribution(0, 24);
  std::uniform_int_distribution<int> weight_distribution(1, 8);
  std::uniform_int_distribution<int> value_distribution(-8, 25);

  for (std::size_t trial = 0; trial < 360; ++trial) {
    const std::size_t count =
        static_cast<std::size_t>(count_distribution(rng));
    const std::size_t capacity =
        static_cast<std::size_t>(capacity_distribution(rng));
    std::vector<KnapsackItem> items;
    items.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      items.push_back(KnapsackItem{
          static_cast<std::size_t>(weight_distribution(rng)),
          static_cast<std::int64_t>(value_distribution(rng))});
    }

    const auto result =
        algorithms::dynamic_programming::solve_unbounded_knapsack(items,
                                                                   capacity);
    REQUIRE_EQ(result.best_value, unbounded_oracle(items, capacity));
    require_valid_unbounded(items, capacity, result);
  }
}

TEST_CASE(knapsack_detects_value_overflow) {
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  const std::vector<KnapsackItem> zero_one{{1, maximum}, {1, maximum}};
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::solve_zero_one_knapsack(zero_one, 2),
      std::overflow_error);

  const std::vector<KnapsackItem> unbounded{{1, maximum}};
  REQUIRE_THROWS_AS(
      algorithms::dynamic_programming::solve_unbounded_knapsack(unbounded, 2),
      std::overflow_error);
}
