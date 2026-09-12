#pragma once

#include "algorithms/data_structures/packed_rank_select.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

class EliasFanoMonotoneSequence {
 public:
  explicit EliasFanoMonotoneSequence(std::span<const std::uint64_t> values) {
    if (values.size() > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) {
      throw std::length_error("Elias-Fano sequence length exceeds uint64 domain");
    }
    size_ = values.size();
    for (std::size_t i = 1U; i < size_; ++i) {
      if (values[i] < values[i - 1U]) {
        throw std::invalid_argument("Elias-Fano input must be nondecreasing");
      }
    }

    if (size_ == 0U) {
      const std::vector<std::uint8_t> empty_bits;
      high_bits_.emplace(std::span<const std::uint8_t>{empty_bits});
      logical_payload_bytes_ = high_bits_->logical_payload_bytes();
      return;
    }

    max_value_ = values.back();
    const auto count64 = static_cast<std::uint64_t>(size_);
    const std::uint64_t ratio = max_value_ / count64;
    low_bit_width_ = ratio == 0U
                         ? 0U
                         : static_cast<std::size_t>(std::bit_width(ratio)) - 1U;

    const std::uint64_t upper_max64 = max_value_ >> low_bit_width_;
    if (upper_max64 > static_cast<std::uint64_t>(
                          std::numeric_limits<std::size_t>::max() - size_)) {
      throw std::length_error("Elias-Fano high bitvector size overflow");
    }
    const auto upper_max = static_cast<std::size_t>(upper_max64);
    high_bit_count_ = size_ + upper_max;

    std::vector<std::uint8_t> high_bits(high_bit_count_, 0U);
    for (std::size_t i = 0U; i < size_; ++i) {
      const auto upper = static_cast<std::size_t>(values[i] >> low_bit_width_);
      high_bits[upper + i] = 1U;
    }
    high_bits_.emplace(std::span<const std::uint8_t>{high_bits});

    total_low_bits_ = checked_mul(size_, low_bit_width_);
    const std::size_t low_word_count =
        total_low_bits_ / 64U + (total_low_bits_ % 64U != 0U ? 1U : 0U);
    low_words_.assign(low_word_count, 0U);

    if (low_bit_width_ != 0U) {
      const std::uint64_t mask = low_mask();
      for (std::size_t i = 0U; i < size_; ++i) {
        const std::uint64_t low = values[i] & mask;
        const std::size_t bit_offset = i * low_bit_width_;
        const std::size_t word = bit_offset / 64U;
        const std::size_t shift = bit_offset % 64U;
        low_words_[word] |= low << shift;
        if (shift + low_bit_width_ > 64U) {
          low_words_[word + 1U] |= low >> (64U - shift);
        }
      }
    }

    encoded_bit_count_ = checked_add(high_bit_count_, total_low_bits_);
    logical_payload_bytes_ = checked_add(
        high_bits_->logical_payload_bytes(),
        checked_mul(low_words_.size(), sizeof(std::uint64_t)));
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t low_bit_width() const noexcept { return low_bit_width_; }
  [[nodiscard]] std::size_t high_bitvector_size() const noexcept { return high_bit_count_; }
  [[nodiscard]] std::size_t encoded_bit_count() const noexcept { return encoded_bit_count_; }
  [[nodiscard]] std::size_t logical_payload_bytes() const noexcept { return logical_payload_bytes_; }

  [[nodiscard]] std::uint64_t at(std::size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("Elias-Fano index out of range");
    }
    const auto position = high_bits_->select1(index);
    if (!position.has_value() || *position < index) {
      throw std::logic_error("Elias-Fano high-bit invariant violated");
    }
    const std::uint64_t upper = static_cast<std::uint64_t>(*position - index);
    return (upper << low_bit_width_) | low_value(index);
  }

  [[nodiscard]] std::size_t lower_bound_index(std::uint64_t value) const {
    std::size_t low = 0U;
    std::size_t high = size_;
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (at(mid) < value) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  [[nodiscard]] std::optional<std::size_t> predecessor_index(
      std::uint64_t value) const {
    const std::size_t upper = upper_bound_index(value);
    if (upper == 0U) {
      return std::nullopt;
    }
    return upper - 1U;
  }

  [[nodiscard]] bool contains(std::uint64_t value) const {
    const std::size_t index = lower_bound_index(value);
    return index < size_ && at(index) == value;
  }

  [[nodiscard]] bool valid_structure() const {
    try {
      if (!high_bits_.has_value() || high_bits_->one_count() != size_ ||
          high_bits_->size() != high_bit_count_) {
        return false;
      }
      if (size_ == 0U) {
        return high_bit_count_ == 0U && low_bit_width_ == 0U &&
               low_words_.empty() && encoded_bit_count_ == 0U;
      }
      const std::uint64_t upper_max64 = max_value_ >> low_bit_width_;
      if (upper_max64 > static_cast<std::uint64_t>(
                            std::numeric_limits<std::size_t>::max() - size_)) {
        return false;
      }
      if (high_bit_count_ != size_ + static_cast<std::size_t>(upper_max64) ||
          total_low_bits_ != size_ * low_bit_width_) {
        return false;
      }
      const std::size_t expected_words =
          total_low_bits_ / 64U + (total_low_bits_ % 64U != 0U ? 1U : 0U);
      if (low_words_.size() != expected_words ||
          encoded_bit_count_ != high_bit_count_ + total_low_bits_) {
        return false;
      }
      std::uint64_t previous = at(0U);
      for (std::size_t i = 1U; i < size_; ++i) {
        const std::uint64_t current = at(i);
        if (current < previous) {
          return false;
        }
        previous = current;
      }
      return previous == max_value_;
    } catch (...) {
      return false;
    }
  }

 private:
  static std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
    if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
      throw std::length_error("Elias-Fano size overflow");
    }
    return lhs + rhs;
  }

  static std::size_t checked_mul(std::size_t lhs, std::size_t rhs) {
    if (lhs != 0U && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
      throw std::length_error("Elias-Fano size overflow");
    }
    return lhs * rhs;
  }

  [[nodiscard]] std::uint64_t low_mask() const noexcept {
    return low_bit_width_ == 0U
               ? 0U
               : (std::uint64_t{1} << low_bit_width_) - 1U;
  }

  [[nodiscard]] std::uint64_t low_value(std::size_t index) const noexcept {
    if (low_bit_width_ == 0U) {
      return 0U;
    }
    const std::size_t bit_offset = index * low_bit_width_;
    const std::size_t word = bit_offset / 64U;
    const std::size_t shift = bit_offset % 64U;
    std::uint64_t value = low_words_[word] >> shift;
    if (shift + low_bit_width_ > 64U) {
      value |= low_words_[word + 1U] << (64U - shift);
    }
    return value & low_mask();
  }

  [[nodiscard]] std::size_t upper_bound_index(std::uint64_t value) const {
    std::size_t low = 0U;
    std::size_t high = size_;
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (at(mid) <= value) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  std::size_t size_ = 0U;
  std::uint64_t max_value_ = 0U;
  std::size_t low_bit_width_ = 0U;
  std::size_t high_bit_count_ = 0U;
  std::size_t total_low_bits_ = 0U;
  std::size_t encoded_bit_count_ = 0U;
  std::size_t logical_payload_bytes_ = 0U;
  std::vector<std::uint64_t> low_words_;
  std::optional<PackedRankSelectBitVector> high_bits_;
};

}  // namespace algorithms::data_structures
