#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace algorithms::sorting {
namespace detail {

template <typename T, typename Compare>
std::size_t median_of_three_index(const std::vector<T>& values,
                                  std::size_t first, std::size_t middle,
                                  std::size_t last, Compare compare) {
  const auto before = [&](std::size_t a, std::size_t b) {
    return compare(values[a], values[b]);
  };

  if (before(first, middle)) {
    if (before(middle, last)) {
      return middle;
    }
    return before(first, last) ? last : first;
  }
  if (before(first, last)) {
    return first;
  }
  return before(middle, last) ? last : middle;
}

template <typename T, typename Compare>
void quick_sort_impl(std::vector<T>& values, std::size_t begin,
                     std::size_t end, Compare compare) {
  while (end - begin > 1) {
    const std::size_t middle = begin + (end - begin) / 2;
    const std::size_t pivot_index =
        median_of_three_index(values, begin, middle, end - 1, compare);
    const T pivot = values[pivot_index];

    // Three-way partition invariant:
    // [begin, less) < pivot, [less, scan) == pivot,
    // [scan, greater) unknown, [greater, end) > pivot.
    std::size_t less = begin;
    std::size_t scan = begin;
    std::size_t greater = end;
    while (scan < greater) {
      if (compare(values[scan], pivot)) {
        std::swap(values[less++], values[scan++]);
      } else if (compare(pivot, values[scan])) {
        std::swap(values[scan], values[--greater]);
      } else {
        ++scan;
      }
    }

    // Recurse on the smaller side and iterate on the larger side to keep
    // auxiliary stack usage O(log n) even for badly unbalanced partitions.
    if (less - begin < end - greater) {
      quick_sort_impl(values, begin, less, compare);
      begin = greater;
    } else {
      quick_sort_impl(values, greater, end, compare);
      end = less;
    }
  }
}

}  // namespace detail

// Quicksort with median-of-three pivot selection and three-way partitioning.
// Duplicate-heavy inputs avoid the classic two-way-partition degeneration.
//
// Average: O(n log n) time. Worst case: O(n^2) comparisons.
// Extra space: O(log n) stack due to smaller-side-first recursion.
template <typename T, typename Compare = std::less<T>>
void quick_sort(std::vector<T>& values, Compare compare = Compare{}) {
  detail::quick_sort_impl(values, 0, values.size(), compare);
}

}  // namespace algorithms::sorting
