#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// First-principles min-max heap over signed 64-bit values.
//
// Even tree levels are min levels and odd tree levels are max levels. The
// complete binary tree is stored in level order. This provides one
// double-ended priority queue with O(1) minimum/maximum lookup and O(log n)
// insertion / endpoint deletion.
class MinMaxHeap {
 public:
  using Value = std::int64_t;

  [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

  [[nodiscard]] Value minimum() const {
    if (empty()) {
      throw std::out_of_range("minimum of empty min-max heap");
    }
    return data_[0U];
  }

  [[nodiscard]] Value maximum() const {
    if (empty()) {
      throw std::out_of_range("maximum of empty min-max heap");
    }
    if (data_.size() == 1U) {
      return data_[0U];
    }
    if (data_.size() == 2U || data_[1U] >= data_[2U]) {
      return data_[1U];
    }
    return data_[2U];
  }

  void push(const Value value) {
    data_.push_back(value);
    std::size_t index = data_.size() - 1U;
    if (index == 0U) {
      return;
    }

    const std::size_t parent_index = parent(index);
    if (is_min_level(index)) {
      if (data_[index] > data_[parent_index]) {
        std::swap(data_[index], data_[parent_index]);
        bubble_up_max(parent_index);
      } else {
        bubble_up_min(index);
      }
    } else {
      if (data_[index] < data_[parent_index]) {
        std::swap(data_[index], data_[parent_index]);
        bubble_up_min(parent_index);
      } else {
        bubble_up_max(index);
      }
    }
  }

  Value pop_minimum() {
    if (empty()) {
      throw std::out_of_range("pop_minimum on empty min-max heap");
    }

    const Value result = data_[0U];
    const Value last = data_.back();
    data_.pop_back();
    if (!data_.empty()) {
      data_[0U] = last;
      trickle_down_min(0U);
    }
    return result;
  }

  Value pop_maximum() {
    if (empty()) {
      throw std::out_of_range("pop_maximum on empty min-max heap");
    }
    if (data_.size() == 1U) {
      const Value result = data_.back();
      data_.pop_back();
      return result;
    }

    const std::size_t index =
        data_.size() == 2U || data_[1U] >= data_[2U] ? 1U : 2U;
    const Value result = data_[index];
    const Value last = data_.back();
    data_.pop_back();
    if (index < data_.size()) {
      data_[index] = last;
      trickle_down_max(index);
    }
    return result;
  }

  [[nodiscard]] bool valid_invariants() const noexcept {
    for (std::size_t index = 0U; index < data_.size(); ++index) {
      const bool min_level = is_min_level(index);
      const CandidateSet candidates = children_and_grandchildren(index);
      for (std::size_t offset = 0U; offset < candidates.count; ++offset) {
        const std::size_t other = candidates.indices[offset];
        if ((min_level && data_[index] > data_[other]) ||
            (!min_level && data_[index] < data_[other])) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  struct CandidateSet {
    std::array<std::size_t, 6U> indices{};
    std::size_t count{};
  };

  std::vector<Value> data_;

  [[nodiscard]] static std::size_t parent(
      const std::size_t index) noexcept {
    return (index - 1U) / 2U;
  }

  [[nodiscard]] static std::size_t grandparent(
      const std::size_t index) noexcept {
    return parent(parent(index));
  }

  [[nodiscard]] static bool is_min_level(
      const std::size_t index) noexcept {
    std::size_t one_based = index + 1U;
    unsigned depth = 0U;
    while (one_based > 1U) {
      one_based >>= 1U;
      ++depth;
    }
    return (depth & 1U) == 0U;
  }

  void bubble_up_min(std::size_t index) {
    while (index >= 3U) {
      const std::size_t ancestor = grandparent(index);
      if (data_[index] >= data_[ancestor]) {
        return;
      }
      std::swap(data_[index], data_[ancestor]);
      index = ancestor;
    }
  }

  void bubble_up_max(std::size_t index) {
    while (index >= 3U) {
      const std::size_t ancestor = grandparent(index);
      if (data_[index] <= data_[ancestor]) {
        return;
      }
      std::swap(data_[index], data_[ancestor]);
      index = ancestor;
    }
  }

  [[nodiscard]] CandidateSet children_and_grandchildren(
      const std::size_t index) const noexcept {
    CandidateSet result;
    if (index > (std::numeric_limits<std::size_t>::max() - 2U) / 2U) {
      return result;
    }

    const std::size_t first_child = 2U * index + 1U;
    for (std::size_t child_offset = 0U; child_offset < 2U;
         ++child_offset) {
      const std::size_t child = first_child + child_offset;
      if (child >= data_.size()) {
        continue;
      }
      result.indices[result.count++] = child;

      if (child > (std::numeric_limits<std::size_t>::max() - 2U) / 2U) {
        continue;
      }
      const std::size_t first_grandchild = 2U * child + 1U;
      if (first_grandchild < data_.size()) {
        result.indices[result.count++] = first_grandchild;
      }
      if (first_grandchild + 1U < data_.size()) {
        result.indices[result.count++] = first_grandchild + 1U;
      }
    }
    return result;
  }

  [[nodiscard]] std::size_t smallest_descendant(
      const std::size_t index) const noexcept {
    const CandidateSet candidates = children_and_grandchildren(index);
    if (candidates.count == 0U) {
      return data_.size();
    }

    std::size_t best = candidates.indices[0U];
    for (std::size_t offset = 1U; offset < candidates.count; ++offset) {
      const std::size_t candidate = candidates.indices[offset];
      if (data_[candidate] < data_[best]) {
        best = candidate;
      }
    }
    return best;
  }

  [[nodiscard]] std::size_t largest_descendant(
      const std::size_t index) const noexcept {
    const CandidateSet candidates = children_and_grandchildren(index);
    if (candidates.count == 0U) {
      return data_.size();
    }

    std::size_t best = candidates.indices[0U];
    for (std::size_t offset = 1U; offset < candidates.count; ++offset) {
      const std::size_t candidate = candidates.indices[offset];
      if (data_[candidate] > data_[best]) {
        best = candidate;
      }
    }
    return best;
  }

  void trickle_down_min(std::size_t index) {
    while (true) {
      const std::size_t candidate = smallest_descendant(index);
      if (candidate == data_.size()) {
        return;
      }

      if (parent(candidate) != index) {
        if (data_[candidate] >= data_[index]) {
          return;
        }
        std::swap(data_[candidate], data_[index]);
        const std::size_t parent_index = parent(candidate);
        if (data_[candidate] > data_[parent_index]) {
          std::swap(data_[candidate], data_[parent_index]);
        }
        index = candidate;
      } else {
        if (data_[candidate] < data_[index]) {
          std::swap(data_[candidate], data_[index]);
        }
        return;
      }
    }
  }

  void trickle_down_max(std::size_t index) {
    while (true) {
      const std::size_t candidate = largest_descendant(index);
      if (candidate == data_.size()) {
        return;
      }

      if (parent(candidate) != index) {
        if (data_[candidate] <= data_[index]) {
          return;
        }
        std::swap(data_[candidate], data_[index]);
        const std::size_t parent_index = parent(candidate);
        if (data_[candidate] < data_[parent_index]) {
          std::swap(data_[candidate], data_[parent_index]);
        }
        index = candidate;
      } else {
        if (data_[candidate] > data_[index]) {
          std::swap(data_[candidate], data_[index]);
        }
        return;
      }
    }
  }
};

}  // namespace algorithms::data_structures
