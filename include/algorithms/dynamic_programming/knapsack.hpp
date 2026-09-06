#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::dynamic_programming {

struct KnapsackItem {
  std::size_t weight;
  std::int64_t value;

  friend bool operator==(const KnapsackItem&, const KnapsackItem&) = default;
};

struct ZeroOneKnapsackResult {
  std::int64_t best_value{0};
  std::size_t total_weight{0};
  std::vector<std::size_t> selected_indices;
};

struct UnboundedKnapsackResult {
  std::int64_t best_value{0};
  std::size_t total_weight{0};
  std::vector<std::size_t> item_counts;
};

// 0/1 knapsack: each item may be selected at most once. Zero-weight items are
// permitted because the 0/1 constraint keeps their multiplicity finite.
//
// State invariant: dp[i][c] is the maximum representable total value achievable
// with the first i items and total weight at most c. The recurrence compares
// excluding item i-1 with including it from row i-1, which enforces one-use.
// Ties deterministically prefer exclusion during reconstruction.
//
// Time: O(n * capacity), space: O(n * capacity). This is pseudo-polynomial in
// the numeric capacity, not polynomial in its bit length.
[[nodiscard]] ZeroOneKnapsackResult solve_zero_one_knapsack(
    const std::vector<KnapsackItem>& items, std::size_t capacity);

// Unbounded knapsack: each item type may be selected any non-negative number of
// times. Every item must have strictly positive weight; zero-weight items are
// rejected because positive-value ones make the optimum unbounded and even
// non-positive ones break the strictly decreasing reconstruction measure.
//
// State invariant: dp[i][c] is the maximum representable total value achievable
// with the first i item types and total weight at most c. Inclusion stays on row
// i (dp[i][c-weight]), which is exactly what permits repeated use.
// Ties deterministically prefer exclusion during reconstruction.
//
// Time: O(n * capacity), space: O(n * capacity), pseudo-polynomial in capacity.
[[nodiscard]] UnboundedKnapsackResult solve_unbounded_knapsack(
    const std::vector<KnapsackItem>& items, std::size_t capacity);

}  // namespace algorithms::dynamic_programming
