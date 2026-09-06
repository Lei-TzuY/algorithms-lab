#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::dynamic_programming {

struct LongestIncreasingSubsequenceResult {
  std::vector<std::size_t> indices;
};

// Strictly increasing subsequence: equal values cannot both extend the witness.
//
// State invariant: length[i] is the maximum length of a strictly increasing
// subsequence ending exactly at i. predecessor[i], when present, is an earlier
// index realizing length[i] - 1.
//
// Time: O(n^2), space: O(n).
[[nodiscard]] LongestIncreasingSubsequenceResult longest_increasing_subsequence_quadratic(
    const std::vector<std::int64_t>& values);

// Strictly increasing subsequence using the tails formulation. Binary search is
// implemented directly rather than delegated to std::lower_bound.
//
// Invariant after processing a prefix: tails[k] names a retained index with
// the minimum tail value among discovered increasing subsequences of length
// k + 1; equal tail values preserve the earlier retained index. Tail values are
// strictly increasing. predecessor links preserve
// one concrete witness even as smaller tails replace larger ones.
//
// Time: O(n log n), space: O(n).
[[nodiscard]] LongestIncreasingSubsequenceResult longest_increasing_subsequence_nlogn(
    const std::vector<std::int64_t>& values);

}  // namespace algorithms::dynamic_programming
