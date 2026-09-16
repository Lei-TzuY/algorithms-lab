#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::searching {
namespace detail {

inline void insertion_sort_selection_range(std::vector<std::int64_t>& values,
                                           std::size_t begin,
                                           std::size_t end) {
  if (end - begin < 2U) {
    return;
  }
  for (std::size_t index = begin + 1U; index < end; ++index) {
    const std::int64_t value = values[index];
    std::size_t position = index;
    while (position > begin && value < values[position - 1U]) {
      values[position] = values[position - 1U];
      --position;
    }
    values[position] = value;
  }
}

inline std::int64_t select_kth_in_working_range(
    std::vector<std::int64_t>& values, std::size_t begin, std::size_t end,
    std::size_t kth_index) {
  while (true) {
    const std::size_t length = end - begin;
    if (length <= 5U) {
      insertion_sort_selection_range(values, begin, end);
      return values[kth_index];
    }

    std::size_t group_count = 0U;
    for (std::size_t group_begin = begin; group_begin < end;) {
      const std::size_t remaining = end - group_begin;
      const std::size_t group_length = std::min<std::size_t>(5U, remaining);
      const std::size_t group_end = group_begin + group_length;
      insertion_sort_selection_range(values, group_begin, group_end);
      const std::size_t median_index = group_begin + group_length / 2U;
      std::swap(values[begin + group_count], values[median_index]);
      ++group_count;
      group_begin = group_end;
    }

    const std::size_t medians_end = begin + group_count;
    const std::size_t median_rank = begin + group_count / 2U;
    const std::int64_t pivot =
        select_kth_in_working_range(values, begin, medians_end, median_rank);

    std::size_t less_end = begin;
    std::size_t scan = begin;
    std::size_t greater_begin = end;
    while (scan < greater_begin) {
      if (values[scan] < pivot) {
        std::swap(values[less_end], values[scan]);
        ++less_end;
        ++scan;
      } else if (pivot < values[scan]) {
        --greater_begin;
        std::swap(values[scan], values[greater_begin]);
      } else {
        ++scan;
      }
    }

    if (kth_index < less_end) {
      end = less_end;
    } else if (kth_index >= greater_begin) {
      begin = greater_begin;
    } else {
      return pivot;
    }
  }
}

}  // namespace detail

// Return the zero-based kth order statistic without mutating the input snapshot.
//
// The implementation is deterministic BFPRT / median-of-medians selection:
// groups of at most five are sorted from first principles, their medians are
// recursively selected as a pivot, and the active range is three-way
// partitioned around that pivot. Standard-library sorting/selection routines
// are not used by the algorithm under study.
//
// Worst-case time: O(n). Auxiliary storage: O(n) for the working copy, plus
// O(log n) recursion depth for median-of-medians pivot selection.
inline std::int64_t deterministic_select_kth(
    std::span<const std::int64_t> values, std::size_t kth_index) {
  if (kth_index >= values.size()) {
    throw std::out_of_range("selection rank out of range");
  }

  std::vector<std::int64_t> working(values.begin(), values.end());
  return detail::select_kth_in_working_range(working, 0U, working.size(),
                                             kth_index);
}

}  // namespace algorithms::searching
