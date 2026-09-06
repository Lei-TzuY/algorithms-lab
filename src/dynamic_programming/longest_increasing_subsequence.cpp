#include "algorithms/dynamic_programming/longest_increasing_subsequence.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace algorithms::dynamic_programming {
namespace {

LongestIncreasingSubsequenceResult reconstruct(
    std::size_t end, const std::vector<std::optional<std::size_t>>& predecessor) {
  LongestIncreasingSubsequenceResult result;
  std::optional<std::size_t> current = end;
  while (current.has_value()) {
    result.indices.push_back(*current);
    current = predecessor[*current];
  }
  std::reverse(result.indices.begin(), result.indices.end());
  return result;
}

}  // namespace

LongestIncreasingSubsequenceResult longest_increasing_subsequence_quadratic(
    const std::vector<std::int64_t>& values) {
  if (values.empty()) {
    return {};
  }

  std::vector<std::size_t> length(values.size(), 1);
  std::vector<std::optional<std::size_t>> predecessor(values.size());

  std::size_t best_end = 0;
  for (std::size_t current = 0; current < values.size(); ++current) {
    for (std::size_t previous = 0; previous < current; ++previous) {
      if (values[previous] >= values[current]) {
        continue;
      }
      const std::size_t candidate = length[previous] + 1;
      if (candidate > length[current]) {
        length[current] = candidate;
        predecessor[current] = previous;
      }
    }
    if (length[current] > length[best_end]) {
      best_end = current;
    }
  }

  return reconstruct(best_end, predecessor);
}

LongestIncreasingSubsequenceResult longest_increasing_subsequence_nlogn(
    const std::vector<std::int64_t>& values) {
  if (values.empty()) {
    return {};
  }

  std::vector<std::size_t> tails;
  tails.reserve(values.size());
  std::vector<std::optional<std::size_t>> predecessor(values.size());

  for (std::size_t current = 0; current < values.size(); ++current) {
    std::size_t low = 0;
    std::size_t high = tails.size();
    while (low < high) {
      const std::size_t middle = low + (high - low) / 2;
      if (values[tails[middle]] < values[current]) {
        low = middle + 1;
      } else {
        high = middle;
      }
    }

    const std::size_t position = low;
    if (position > 0) {
      predecessor[current] = tails[position - 1];
    }

    if (position == tails.size()) {
      tails.push_back(current);
    } else if (values[current] < values[tails[position]]) {
      // Equal values deliberately keep the earlier retained index, making tie
      // behavior deterministic without changing the minimum tail value.
      tails[position] = current;
    }
  }

  return reconstruct(tails.back(), predecessor);
}

}  // namespace algorithms::dynamic_programming
