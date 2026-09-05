#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace algorithms::sorting {
namespace detail {

template <typename T, typename Compare>
void merge_sort_impl(std::vector<T>& values, std::vector<T>& buffer,
                     std::size_t begin, std::size_t end, Compare compare) {
  if (end - begin <= 1) {
    return;
  }

  const std::size_t middle = begin + (end - begin) / 2;
  merge_sort_impl(values, buffer, begin, middle, compare);
  merge_sort_impl(values, buffer, middle, end, compare);

  std::size_t left = begin;
  std::size_t right = middle;
  std::size_t out = begin;

  while (left < middle && right < end) {
    if (compare(values[right], values[left])) {
      buffer[out++] = values[right++];
    } else {
      buffer[out++] = values[left++];
    }
  }
  while (left < middle) {
    buffer[out++] = values[left++];
  }
  while (right < end) {
    buffer[out++] = values[right++];
  }
  for (std::size_t index = begin; index < end; ++index) {
    values[index] = buffer[index];
  }
}

}  // namespace detail

// Stable divide-and-conquer sort.
//
// Invariant during merge: buffer[begin, out) is the sorted merge of the
// already-consumed prefixes of the two sorted halves.
//
// Complexity: O(n log n) time, O(n) auxiliary space.
template <typename T, typename Compare = std::less<T>>
void merge_sort(std::vector<T>& values, Compare compare = Compare{}) {
  if (values.size() <= 1) {
    return;
  }
  std::vector<T> buffer(values);
  detail::merge_sort_impl(values, buffer, 0, values.size(), compare);
}

}  // namespace algorithms::sorting
