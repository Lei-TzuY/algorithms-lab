#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::strings {

struct SuffixArrayResult {
  // Starting offsets of all non-empty suffixes in lexicographic order.
  std::vector<std::size_t> order;
  // Inverse permutation: rank[start] is start's position in order.
  std::vector<std::size_t> rank;
  // lcp[0] = 0; lcp[k] = LCP(order[k - 1], order[k]) for k > 0.
  std::vector<std::size_t> lcp;
};

// Prefix-doubling suffix array with rank-pair sorting, followed by Kasai LCP.
// Bytes are ordered by their unsigned 0..255 values.
// Construction is O(n log^2 n) because each of O(log n) doubling rounds uses
// comparison sorting. Storage is O(n); Kasai's LCP pass is O(n).
[[nodiscard]] inline SuffixArrayResult build_suffix_array(
    std::string_view input) {
  const std::size_t size = input.size();
  SuffixArrayResult result;
  result.order.resize(size);
  result.rank.resize(size);
  result.lcp.assign(size, 0U);
  std::iota(result.order.begin(), result.order.end(), std::size_t{0});

  if (size == 0U) {
    return result;
  }

  // Rank zero is reserved for the implicit end-of-string sentinel. Real
  // one-byte ranks are 1..256 so a shorter equal-prefix suffix sorts first.
  std::vector<std::size_t> classes(size, 0U);
  std::vector<std::size_t> next_classes(size, 0U);
  for (std::size_t index = 0U; index < size; ++index) {
    classes[index] =
        static_cast<std::size_t>(static_cast<unsigned char>(input[index])) +
        std::size_t{1};
  }

  std::size_t span = 1U;
  while (span < size) {
    const auto key = [&](std::size_t start) {
      const std::size_t second =
          span < size - start ? classes[start + span] : std::size_t{0};
      return std::pair<std::size_t, std::size_t>{classes[start], second};
    };

    std::sort(result.order.begin(), result.order.end(),
              [&](std::size_t left, std::size_t right) {
                return key(left) < key(right);
              });

    std::size_t class_count = 1U;
    next_classes[result.order[0]] = class_count;
    for (std::size_t position = 1U; position < size; ++position) {
      const std::size_t previous = result.order[position - 1U];
      const std::size_t current = result.order[position];
      if (key(previous) != key(current)) {
        ++class_count;
      }
      next_classes[current] = class_count;
    }
    classes.swap(next_classes);

    if (class_count == size) {
      break;
    }
    if (span > size / 2U) {
      break;
    }
    span *= 2U;
  }

  for (std::size_t position = 0U; position < size; ++position) {
    result.rank[result.order[position]] = position;
  }

  std::size_t shared = 0U;
  for (std::size_t start = 0U; start < size; ++start) {
    const std::size_t position = result.rank[start];
    if (position == 0U) {
      shared = 0U;
      continue;
    }

    const std::size_t previous = result.order[position - 1U];
    while (shared < size - start && shared < size - previous &&
           input[start + shared] == input[previous + shared]) {
      ++shared;
    }
    result.lcp[position] = shared;
    if (shared > 0U) {
      --shared;
    }
  }

  return result;
}

}  // namespace algorithms::strings
