#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>

namespace algorithms::searching {

// Finds the first element equivalent to key in a sorted range.
//
// Preconditions:
// - values is sorted according to compare.
//
// Invariant:
// - The first equivalent element, if it exists, is always inside [low, high).
// - Every index < low is strictly less than key.
//
// Complexity: O(log n) comparisons and O(1) extra space.
template <typename T, typename Compare = std::less<T>>
[[nodiscard]] std::optional<std::size_t> binary_search_index(
    std::span<const T> values, const T& key, Compare compare = Compare{}) {
  std::size_t low = 0;
  std::size_t high = values.size();

  while (low < high) {
    const std::size_t middle = low + (high - low) / 2;
    if (compare(values[middle], key)) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }

  if (low < values.size() && !compare(key, values[low]) &&
      !compare(values[low], key)) {
    return low;
  }
  return std::nullopt;
}

}  // namespace algorithms::searching
