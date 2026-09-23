#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Mutable ordered sequence stored in a hierarchy of aligned gapped segments.
//
// The representation keeps a power-of-two slot array. Occupied slots preserve
// logical sequence order. Insertions identify the highest over-dense segment on
// the mutation path, then redistribute one enclosing segment; erasures compact
// sparse paths and shrink the root when it becomes globally under-filled.
//
// This is an exact packed-memory-array baseline. It deliberately does not claim
// the classical optimal amortized PMA bounds: rank lookup is a linear scan over
// physical slots, and density thresholds are fixed rather than level-varying.
class PackedMemoryArraySequence {
 public:
  using Value = std::int64_t;

  PackedMemoryArraySequence()
      : slots_(kMinimumCapacity) {}

  explicit PackedMemoryArraySequence(std::span<const Value> values) {
    initialize_from(values);
  }

  explicit PackedMemoryArraySequence(const std::vector<Value>& values)
      : PackedMemoryArraySequence(std::span<const Value>(values)) {}

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t capacity() const noexcept {
    return slots_.size();
  }

  [[nodiscard]] Value at(const std::size_t rank) const {
    return *slots_[physical_index(rank)];
  }

  void insert(const std::size_t rank, const Value value) {
    if (rank > size_) {
      throw std::out_of_range(
          "packed-memory-array insertion rank out of range");
    }

    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error(
          "packed-memory-array logical size exhausted");
    }

    ensure_root_can_accept_one();

    if (size_ == 0U) {
      std::vector<Value> only{value};
      redistribute_segment(0U, capacity(), only);
      size_ = 1U;
      return;
    }

    const std::size_t anchor =
        rank < size_ ? physical_index(rank)
                     : physical_index(size_ - 1U);

    std::size_t highest_violating_width = 0U;
    for (std::size_t width = kLeafWidth;; width *= 2U) {
      const std::size_t begin = aligned_begin(anchor, width);
      const std::size_t occupancy = count_window(begin, width);
      if (occupancy + 1U > upper_limit(width)) {
        highest_violating_width = width;
      }
      if (width == capacity()) {
        break;
      }
    }

    if (highest_violating_width == capacity()) {
      throw std::logic_error(
          "packed-memory-array root density precheck failed");
    }

    const std::size_t width =
        highest_violating_width == 0U
            ? kLeafWidth
            : highest_violating_width * 2U;
    const std::size_t begin = aligned_begin(anchor, width);
    const std::size_t before = count_prefix(begin);
    if (rank < before) {
      throw std::logic_error(
          "packed-memory-array insertion prefix invariant violated");
    }

    std::vector<Value> values = collect_window(begin, width);
    const std::size_t local_rank = rank - before;
    if (local_rank > values.size()) {
      throw std::logic_error(
          "packed-memory-array insertion window invariant violated");
    }

    values.insert(
        values.begin() + static_cast<std::ptrdiff_t>(local_rank),
        value);
    redistribute_segment(begin, width, values);
    ++size_;

    // Fixed thresholds make the proof simple, but rounding at a local
    // redistribution boundary can conservatively fall back to a whole-root
    // rebalance rather than weakening the density invariant.
    if (!upper_density_valid()) {
      rebuild_capacity(capacity());
    }
  }

  [[nodiscard]] Value erase(const std::size_t rank) {
    if (rank >= size_) {
      throw std::out_of_range(
          "packed-memory-array erase rank out of range");
    }

    const std::size_t position = physical_index(rank);
    const Value removed = *slots_[position];
    slots_[position].reset();
    --size_;

    if (size_ == 0U) {
      slots_.assign(kMinimumCapacity, std::nullopt);
      return removed;
    }

    std::size_t target_capacity = capacity();
    while (target_capacity > kMinimumCapacity &&
           size_ < lower_trigger(target_capacity)) {
      const std::size_t candidate = target_capacity / 2U;
      if (candidate < kMinimumCapacity ||
          size_ > upper_limit(candidate)) {
        break;
      }
      target_capacity = candidate;
    }

    if (target_capacity != capacity()) {
      rebuild_capacity(target_capacity);
      return removed;
    }

    std::size_t highest_sparse_width = 0U;
    for (std::size_t width = kLeafWidth;; width *= 2U) {
      const std::size_t begin = aligned_begin(position, width);
      if (count_window(begin, width) < lower_trigger(width)) {
        highest_sparse_width = width;
      }
      if (width == capacity()) {
        break;
      }
    }

    std::size_t width = kLeafWidth;
    if (highest_sparse_width != 0U) {
      width = highest_sparse_width == capacity()
                  ? capacity()
                  : highest_sparse_width * 2U;
    }
    const std::size_t begin = aligned_begin(position, width);
    redistribute_segment(begin, width, collect_window(begin, width));

    if (!upper_density_valid()) {
      rebuild_capacity(capacity());
    }
    return removed;
  }

  [[nodiscard]] std::vector<Value> to_vector() const {
    std::vector<Value> values;
    values.reserve(size_);
    for (const auto& slot : slots_) {
      if (slot.has_value()) {
        values.push_back(*slot);
      }
    }
    return values;
  }

  // Expensive structural diagnostic. It verifies exact cardinality, slot-array
  // shape, and the hierarchical upper-density invariant. It is intentionally
  // excluded from operation-complexity claims.
  [[nodiscard]] bool valid_structure() const noexcept {
    if (slots_.size() < kMinimumCapacity ||
        !std::has_single_bit(slots_.size()) ||
        slots_.size() % kLeafWidth != 0U) {
      return false;
    }

    std::size_t occupied = 0U;
    for (const auto& slot : slots_) {
      occupied += slot.has_value() ? 1U : 0U;
    }
    if (occupied != size_ || size_ > upper_limit(capacity())) {
      return false;
    }
    return upper_density_valid();
  }

 private:
  static constexpr std::size_t kLeafWidth = 8U;
  static constexpr std::size_t kMinimumCapacity = 16U;

  std::vector<std::optional<Value>> slots_;
  std::size_t size_{};

  [[nodiscard]] static std::size_t upper_limit(
      const std::size_t width) noexcept {
    const std::size_t quarter = width / 4U;
    const std::size_t remainder = width % 4U;
    const std::size_t ceil_quarter =
        quarter + (remainder == 0U ? 0U : 1U);
    return width - ceil_quarter;
  }

  [[nodiscard]] static std::size_t lower_trigger(
      const std::size_t width) noexcept {
    const std::size_t quarter = width / 4U;
    return quarter + (width % 4U == 0U ? 0U : 1U);
  }

  [[nodiscard]] static std::size_t aligned_begin(
      const std::size_t position,
      const std::size_t width) noexcept {
    return (position / width) * width;
  }

  void initialize_from(const std::span<const Value> values) {
    size_ = values.size();
    std::size_t target = kMinimumCapacity;
    while (size_ > upper_limit(target)) {
      if (target > std::numeric_limits<std::size_t>::max() / 2U) {
        throw std::length_error(
            "packed-memory-array initial sequence is too large");
      }
      target *= 2U;
    }

    slots_.assign(target, std::nullopt);
    std::vector<Value> copy(values.begin(), values.end());
    place_evenly(0U, target, copy);
  }

  [[nodiscard]] std::size_t physical_index(
      const std::size_t rank) const {
    if (rank >= size_) {
      throw std::out_of_range(
          "packed-memory-array rank out of range");
    }

    std::size_t seen = 0U;
    for (std::size_t index = 0U; index < slots_.size(); ++index) {
      if (!slots_[index].has_value()) {
        continue;
      }
      if (seen == rank) {
        return index;
      }
      ++seen;
    }
    throw std::logic_error(
        "packed-memory-array cardinality invariant violated");
  }

  [[nodiscard]] std::size_t count_prefix(
      const std::size_t end) const noexcept {
    std::size_t count = 0U;
    for (std::size_t index = 0U; index < end; ++index) {
      count += slots_[index].has_value() ? 1U : 0U;
    }
    return count;
  }

  [[nodiscard]] std::size_t count_window(
      const std::size_t begin,
      const std::size_t width) const noexcept {
    std::size_t count = 0U;
    for (std::size_t index = begin;
         index < begin + width; ++index) {
      count += slots_[index].has_value() ? 1U : 0U;
    }
    return count;
  }

  [[nodiscard]] std::vector<Value> collect_window(
      const std::size_t begin,
      const std::size_t width) const {
    std::vector<Value> values;
    values.reserve(count_window(begin, width));
    for (std::size_t index = begin;
         index < begin + width; ++index) {
      if (slots_[index].has_value()) {
        values.push_back(*slots_[index]);
      }
    }
    return values;
  }

  void ensure_root_can_accept_one() {
    if (size_ + 1U <= upper_limit(capacity())) {
      return;
    }
    grow_for(size_ + 1U);
  }

  void grow_for(const std::size_t required_size) {
    std::size_t target = capacity();
    while (required_size > upper_limit(target)) {
      if (target > std::numeric_limits<std::size_t>::max() / 2U) {
        throw std::length_error(
            "packed-memory-array capacity exhausted");
      }
      target *= 2U;
    }
    if (target != capacity()) {
      rebuild_capacity(target);
    }
  }

  void rebuild_capacity(const std::size_t new_capacity) {
    if (new_capacity < kMinimumCapacity ||
        !std::has_single_bit(new_capacity) ||
        new_capacity % kLeafWidth != 0U ||
        size_ > upper_limit(new_capacity)) {
      throw std::logic_error(
          "packed-memory-array rebuild capacity invalid");
    }

    const std::vector<Value> values = to_vector();
    slots_.assign(new_capacity, std::nullopt);
    place_evenly(0U, new_capacity, values);
  }

  void redistribute_segment(
      const std::size_t begin,
      const std::size_t width,
      const std::vector<Value>& values) {
    if (begin > capacity() || width > capacity() - begin ||
        values.size() > upper_limit(width)) {
      throw std::logic_error(
          "packed-memory-array redistribution bounds invalid");
    }

    for (std::size_t index = begin;
         index < begin + width; ++index) {
      slots_[index].reset();
    }
    place_evenly(begin, width, values);
  }

  void place_evenly(
      const std::size_t begin,
      const std::size_t width,
      const std::vector<Value>& values) {
    const std::size_t count = values.size();
    if (count == 0U) {
      return;
    }
    if (count >= width) {
      throw std::logic_error(
          "packed-memory-array segment has no gap");
    }

    const std::size_t step = width / count;
    const std::size_t remainder = width % count;
    std::size_t offset = step / 2U;
    std::size_t error = 0U;

    for (std::size_t index = 0U; index < count; ++index) {
      if (offset >= width ||
          slots_[begin + offset].has_value()) {
        throw std::logic_error(
            "packed-memory-array even placement invariant violated");
      }
      slots_[begin + offset] = values[index];

      if (index + 1U == count) {
        continue;
      }

      offset += step;
      error += remainder;
      if (error >= count) {
        ++offset;
        error -= count;
      }
    }
  }

  [[nodiscard]] bool upper_density_valid() const noexcept {
    for (std::size_t width = kLeafWidth;; width *= 2U) {
      for (std::size_t begin = 0U;
           begin < capacity(); begin += width) {
        if (count_window(begin, width) > upper_limit(width)) {
          return false;
        }
      }
      if (width == capacity()) {
        break;
      }
    }
    return true;
  }
};

}  // namespace algorithms::data_structures
