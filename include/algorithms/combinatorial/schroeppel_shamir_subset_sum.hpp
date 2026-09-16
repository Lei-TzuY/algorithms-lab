#pragma once

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct SubsetSumWitness {
  std::vector<std::size_t> indices;
  std::int64_t sum{};

  friend bool operator==(const SubsetSumWitness&, const SubsetSumWitness&) =
      default;
};

namespace schroeppel_shamir_detail {

constexpr std::size_t kMaxElements = 40U;

inline std::uint64_t negative_magnitude(std::int64_t value) {
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

inline std::uint64_t checked_add_u64(std::uint64_t lhs, std::uint64_t rhs) {
  if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
    throw std::overflow_error("subset-sum representability bound overflow");
  }
  return lhs + rhs;
}

inline std::int64_t checked_add_i64(std::int64_t lhs, std::int64_t rhs) {
  if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
      (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)) {
    throw std::overflow_error("subset sum is not representable");
  }
  return lhs + rhs;
}

inline void validate_representability(std::span<const std::int64_t> values) {
  if (values.size() > kMaxElements) {
    throw std::length_error("Schroeppel-Shamir supports at most 40 elements");
  }

  std::uint64_t positive = 0U;
  std::uint64_t negative = 0U;
  for (const std::int64_t value : values) {
    if (value > 0) {
      positive = checked_add_u64(positive, static_cast<std::uint64_t>(value));
    } else if (value < 0) {
      negative = checked_add_u64(negative, negative_magnitude(value));
    }
  }
  if (positive > static_cast<std::uint64_t>(
                     std::numeric_limits<std::int64_t>::max())) {
    throw std::overflow_error("positive subset sums are not representable");
  }
  constexpr std::uint64_t kMinMagnitude = std::uint64_t{1} << 63U;
  if (negative > kMinMagnitude) {
    throw std::overflow_error("negative subset sums are not representable");
  }
}

struct QuarterSum {
  std::int64_t sum{};
  std::uint16_t mask{};
};

inline std::vector<QuarterSum> enumerate_quarter(
    std::span<const std::int64_t> values, std::size_t offset,
    std::size_t count) {
  const std::size_t combinations = std::size_t{1} << count;
  std::vector<QuarterSum> result;
  result.reserve(combinations);
  for (std::size_t mask = 0; mask < combinations; ++mask) {
    std::int64_t sum = 0;
    for (std::size_t bit = 0; bit < count; ++bit) {
      if ((mask & (std::size_t{1} << bit)) != 0U) {
        sum = checked_add_i64(sum, values[offset + bit]);
      }
    }
    result.push_back(
        QuarterSum{sum, static_cast<std::uint16_t>(mask)});
  }
  std::sort(result.begin(), result.end(), [](const QuarterSum& lhs,
                                             const QuarterSum& rhs) {
    if (lhs.sum != rhs.sum) {
      return lhs.sum < rhs.sum;
    }
    return lhs.mask < rhs.mask;
  });
  return result;
}

struct PairCursor {
  std::int64_t sum{};
  std::size_t first{};
  std::size_t second{};
};

struct PairCursorMinCompare {
  bool operator()(const PairCursor& lhs, const PairCursor& rhs) const {
    if (lhs.sum != rhs.sum) {
      return lhs.sum < rhs.sum;
    }
    if (lhs.first != rhs.first) {
      return lhs.first < rhs.first;
    }
    return lhs.second < rhs.second;
  }
};

struct PairCursorMaxCompare {
  bool operator()(const PairCursor& lhs, const PairCursor& rhs) const {
    if (lhs.sum != rhs.sum) {
      return lhs.sum > rhs.sum;
    }
    if (lhs.first != rhs.first) {
      return lhs.first < rhs.first;
    }
    return lhs.second > rhs.second;
  }
};

template <typename Compare>
using PairHeap = algorithms::data_structures::BinaryHeap<PairCursor, Compare>;

class AscendingPairStream {
 public:
  AscendingPairStream(const std::vector<QuarterSum>& first,
                      const std::vector<QuarterSum>& second)
      : first_(first), second_(second) {
    if (second_.empty()) {
      return;
    }
    for (std::size_t index = 0; index < first_.size(); ++index) {
      heap_.push(PairCursor{checked_add_i64(first_[index].sum, second_[0].sum),
                            index, 0U});
    }
  }

  [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
  [[nodiscard]] const PairCursor& top() const { return heap_.top(); }

  PairCursor pop() {
    PairCursor current = heap_.pop();
    const std::size_t next = current.second + 1U;
    if (next < second_.size()) {
      heap_.push(PairCursor{
          checked_add_i64(first_[current.first].sum, second_[next].sum),
          current.first, next});
    }
    return current;
  }

 private:
  const std::vector<QuarterSum>& first_;
  const std::vector<QuarterSum>& second_;
  PairHeap<PairCursorMinCompare> heap_;
};

class DescendingPairStream {
 public:
  DescendingPairStream(const std::vector<QuarterSum>& first,
                       const std::vector<QuarterSum>& second)
      : first_(first), second_(second) {
    if (second_.empty()) {
      return;
    }
    const std::size_t last = second_.size() - 1U;
    for (std::size_t index = 0; index < first_.size(); ++index) {
      heap_.push(PairCursor{
          checked_add_i64(first_[index].sum, second_[last].sum), index, last});
    }
  }

  [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
  [[nodiscard]] const PairCursor& top() const { return heap_.top(); }

  PairCursor pop() {
    PairCursor current = heap_.pop();
    if (current.second > 0U) {
      const std::size_t next = current.second - 1U;
      heap_.push(PairCursor{
          checked_add_i64(first_[current.first].sum, second_[next].sum),
          current.first, next});
    }
    return current;
  }

 private:
  const std::vector<QuarterSum>& first_;
  const std::vector<QuarterSum>& second_;
  PairHeap<PairCursorMaxCompare> heap_;
};

inline void append_mask_indices(std::vector<std::size_t>& indices,
                                std::size_t offset, std::size_t count,
                                std::uint16_t mask) {
  for (std::size_t bit = 0; bit < count; ++bit) {
    if ((mask & (std::uint16_t{1} << bit)) != 0U) {
      indices.push_back(offset + bit);
    }
  }
}

}  // namespace schroeppel_shamir_detail

// Exact subset-sum decision + witness using the Schroeppel-Shamir four-way
// meet-in-the-middle decomposition. Pair sums are generated lazily with the
// repository BinaryHeap, so the O(2^(n/2)) Cartesian products are not stored.
//
// Direct implementation bound for n elements:
//   time  O(2^(n/2) log 2^(n/4)) in the worst case,
//   space O(2^(n/4)),
// plus the returned witness. Inputs are bounded to 40 elements.
inline std::optional<SubsetSumWitness> schroeppel_shamir_subset_sum(
    std::span<const std::int64_t> values, std::int64_t target) {
  using namespace schroeppel_shamir_detail;
  validate_representability(values);

  if (target == 0) {
    return SubsetSumWitness{{}, 0};
  }

  const std::size_t n = values.size();
  std::array<std::size_t, 4> counts{};
  const std::size_t base = n / 4U;
  const std::size_t remainder = n % 4U;
  for (std::size_t group = 0; group < 4U; ++group) {
    counts[group] = base + (group < remainder ? 1U : 0U);
  }

  std::array<std::size_t, 4> offsets{};
  for (std::size_t group = 1; group < 4U; ++group) {
    offsets[group] = offsets[group - 1U] + counts[group - 1U];
  }

  const auto a = enumerate_quarter(values, offsets[0], counts[0]);
  const auto b = enumerate_quarter(values, offsets[1], counts[1]);
  const auto c = enumerate_quarter(values, offsets[2], counts[2]);
  const auto d = enumerate_quarter(values, offsets[3], counts[3]);

  AscendingPairStream left(a, b);
  DescendingPairStream right(c, d);
  while (!left.empty() && !right.empty()) {
    const PairCursor left_cursor = left.top();
    const PairCursor right_cursor = right.top();
    const std::int64_t total = checked_add_i64(left_cursor.sum, right_cursor.sum);
    if (total == target) {
      std::vector<std::size_t> indices;
      indices.reserve(n);
      append_mask_indices(indices, offsets[0], counts[0],
                          a[left_cursor.first].mask);
      append_mask_indices(indices, offsets[1], counts[1],
                          b[left_cursor.second].mask);
      append_mask_indices(indices, offsets[2], counts[2],
                          c[right_cursor.first].mask);
      append_mask_indices(indices, offsets[3], counts[3],
                          d[right_cursor.second].mask);
      return SubsetSumWitness{std::move(indices), target};
    }
    if (total < target) {
      static_cast<void>(left.pop());
    } else {
      static_cast<void>(right.pop());
    }
  }
  return std::nullopt;
}

inline bool valid_subset_sum_witness(std::span<const std::int64_t> values,
                                     std::int64_t target,
                                     const SubsetSumWitness& witness) {
  if (witness.sum != target || !std::is_sorted(witness.indices.begin(),
                                                witness.indices.end())) {
    return false;
  }
  std::int64_t sum = 0;
  std::optional<std::size_t> previous;
  for (const std::size_t index : witness.indices) {
    if (index >= values.size() || (previous.has_value() && *previous == index)) {
      return false;
    }
    try {
      sum = schroeppel_shamir_detail::checked_add_i64(sum, values[index]);
    } catch (const std::overflow_error&) {
      return false;
    }
    previous = index;
  }
  return sum == target;
}

}  // namespace algorithms::combinatorial
