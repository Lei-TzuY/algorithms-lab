#pragma once

#include "algorithms/data_structures/binary_heap.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

namespace algorithms::optimization {

class SlopeTrick {
 public:
  using Value = std::int64_t;

  struct MinimizerInterval {
    // null lower means -infinity; null upper means +infinity.
    std::optional<Value> lower;
    std::optional<Value> upper;

    friend bool operator==(const MinimizerInterval&,
                           const MinimizerInterval&) = default;
  };

  [[nodiscard]] Value minimum_value() const noexcept { return minimum_; }

  [[nodiscard]] MinimizerInterval minimizer_interval() const {
    return MinimizerInterval{
        left_.empty() ? std::nullopt
                      : std::optional<Value>(left_.top()),
        right_.empty() ? std::nullopt
                       : std::optional<Value>(right_.top())};
  }

  [[nodiscard]] std::size_t left_breakpoint_count() const noexcept {
    return left_.size();
  }

  [[nodiscard]] std::size_t right_breakpoint_count() const noexcept {
    return right_.size();
  }

  [[nodiscard]] std::size_t breakpoint_count() const noexcept {
    return left_.size() + right_.size();
  }

  // Adds a constant to the represented convex function.
  // Throws before changing state if the exact minimum would leave int64_t.
  void add_constant(Value delta) { minimum_ = checked_add(minimum_, delta); }

  // Adds max(x - a, 0).
  void add_x_minus_a(Value a) {
    const bool rebalance = !left_.empty() && left_.top() > a;
    const Value new_minimum =
        rebalance ? add_positive_difference(minimum_, left_.top(), a)
                  : minimum_;
    add_x_minus_a_structure(a);
    minimum_ = new_minimum;
  }

  // Adds max(a - x, 0).
  void add_a_minus_x(Value a) {
    const bool rebalance = !right_.empty() && right_.top() < a;
    const Value new_minimum =
        rebalance ? add_positive_difference(minimum_, a, right_.top())
                  : minimum_;
    add_a_minus_x_structure(a);
    minimum_ = new_minimum;
  }

  // Adds |x-a|. Arithmetic overflow is checked before either heap is changed.
  void add_abs(Value a) {
    Value new_minimum = minimum_;
    if (!left_.empty() && left_.top() > a) {
      new_minimum = add_positive_difference(minimum_, left_.top(), a);
    } else if (!right_.empty() && right_.top() < a) {
      new_minimum = add_positive_difference(minimum_, a, right_.top());
    }
    add_x_minus_a_structure(a);
    add_a_minus_x_structure(a);
    minimum_ = new_minimum;
  }

  // Replaces f(x) by min_{y <= x} f(y).
  void prefix_min() { right_ = MinHeap{}; }

  // Replaces f(x) by min_{y >= x} f(y).
  void suffix_min() { left_ = MaxHeap{}; }

  [[nodiscard]] bool valid_structure() const {
    if (!left_.valid_heap_property() || !right_.valid_heap_property()) {
      return false;
    }
    return left_.empty() || right_.empty() || left_.top() <= right_.top();
  }

 private:
  using MaxHeap = algorithms::data_structures::BinaryHeap<Value, std::greater<Value>>;
  using MinHeap = algorithms::data_structures::BinaryHeap<Value, std::less<Value>>;

  static Value checked_add(Value first, Value second) {
    constexpr Value kMax = std::numeric_limits<Value>::max();
    constexpr Value kMin = std::numeric_limits<Value>::min();
    if ((second > 0 && first > kMax - second) ||
        (second < 0 && first < kMin - second)) {
      throw std::overflow_error("slope-trick value overflow");
    }
    return static_cast<Value>(first + second);
  }

  // Returns base + (high-low) exactly when the mathematical result fits in
  // int64_t. The positive difference itself may be as large as UINT64_MAX.
  static Value add_positive_difference(Value base, Value high, Value low) {
    if (high <= low) {
      return base;
    }

    const auto high_bits = static_cast<std::uint64_t>(high);
    const auto low_bits = static_cast<std::uint64_t>(low);
    const std::uint64_t difference = high_bits - low_bits;
    constexpr Value kMax = std::numeric_limits<Value>::max();
    constexpr Value kMin = std::numeric_limits<Value>::min();

    if (base >= 0) {
      const auto room = static_cast<std::uint64_t>(kMax - base);
      if (difference > room) {
        throw std::overflow_error("slope-trick value overflow");
      }
      return static_cast<Value>(base + static_cast<Value>(difference));
    }

    const std::uint64_t magnitude =
        static_cast<std::uint64_t>(-(base + 1)) + 1U;
    if (difference >= magnitude) {
      const std::uint64_t nonnegative = difference - magnitude;
      if (nonnegative > static_cast<std::uint64_t>(kMax)) {
        throw std::overflow_error("slope-trick value overflow");
      }
      return static_cast<Value>(nonnegative);
    }

    const std::uint64_t negative_magnitude = magnitude - difference;
    const std::uint64_t minimum_magnitude =
        static_cast<std::uint64_t>(kMax) + 1U;
    if (negative_magnitude == minimum_magnitude) {
      return kMin;
    }
    return static_cast<Value>(-static_cast<Value>(negative_magnitude));
  }

  void add_x_minus_a_structure(Value a) {
    if (!left_.empty() && left_.top() > a) {
      const Value old_left = left_.top();
      left_.push(a);
      right_.push(old_left);
      static_cast<void>(left_.pop());
    } else {
      right_.push(a);
    }
  }

  void add_a_minus_x_structure(Value a) {
    if (!right_.empty() && right_.top() < a) {
      const Value old_right = right_.top();
      right_.push(a);
      left_.push(old_right);
      static_cast<void>(right_.pop());
    } else {
      left_.push(a);
    }
  }

  Value minimum_{0};
  MaxHeap left_;
  MinHeap right_;
};

}  // namespace algorithms::optimization
