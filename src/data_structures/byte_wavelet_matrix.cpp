#include "algorithms/data_structures/byte_wavelet_matrix.hpp"

#include <limits>
#include <stdexcept>

namespace algorithms::data_structures {
namespace {

std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
  if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
    throw std::length_error("byte wavelet matrix payload size overflow");
  }
  return lhs + rhs;
}

}  // namespace

ByteWaveletMatrix::ByteWaveletMatrix(std::span<const std::uint8_t> values)
    : size_(values.size()), logical_payload_bytes_(sizeof(zero_counts_)) {
  levels_.reserve(kLevels);
  std::vector<std::uint8_t> current(values.begin(), values.end());
  std::vector<std::uint8_t> next(size_);
  std::vector<std::uint8_t> bits(size_);

  for (std::size_t level = 0U; level < kLevels; ++level) {
    const std::size_t shift = (kLevels - 1U) - level;
    std::size_t zero_count = 0U;
    for (std::size_t index = 0U; index < size_; ++index) {
      const std::uint32_t promoted = static_cast<std::uint32_t>(current[index]);
      const std::uint8_t bit = static_cast<std::uint8_t>(
          (promoted >> shift) & std::uint32_t{1});
      bits[index] = bit;
      zero_count += bit == 0U ? 1U : 0U;
    }

    levels_.emplace_back(std::span<const std::uint8_t>{bits});
    zero_counts_[level] = zero_count;
    logical_payload_bytes_ = checked_add(
        logical_payload_bytes_, levels_.back().logical_payload_bytes());

    std::size_t zero_position = 0U;
    std::size_t one_position = zero_count;
    for (std::size_t index = 0U; index < size_; ++index) {
      if (bits[index] == 0U) {
        next[zero_position++] = current[index];
      } else {
        next[one_position++] = current[index];
      }
    }
    current.swap(next);
  }
}

std::size_t ByteWaveletMatrix::size() const noexcept { return size_; }

bool ByteWaveletMatrix::empty() const noexcept { return size_ == 0U; }

std::uint8_t ByteWaveletMatrix::access(std::size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("byte wavelet matrix access index out of range");
  }

  std::uint8_t value = 0U;
  std::size_t position = index;
  for (std::size_t level = 0U; level < kLevels; ++level) {
    const bool bit = levels_[level].bit(position);
    if (bit) {
      const std::size_t shift = (kLevels - 1U) - level;
      value = static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(value) | (std::uint32_t{1} << shift));
      position = zero_counts_[level] + levels_[level].rank1(position);
    } else {
      position = levels_[level].rank0(position);
    }
  }
  return value;
}

ByteWaveletMatrix::Interval ByteWaveletMatrix::descend(
    std::uint8_t value, std::size_t begin, std::size_t end) const {
  for (std::size_t level = 0U; level < kLevels; ++level) {
    const std::size_t shift = (kLevels - 1U) - level;
    const std::uint32_t promoted = static_cast<std::uint32_t>(value);
    const bool bit = ((promoted >> shift) & std::uint32_t{1}) != 0U;
    if (bit) {
      begin = zero_counts_[level] + levels_[level].rank1(begin);
      end = zero_counts_[level] + levels_[level].rank1(end);
    } else {
      begin = levels_[level].rank0(begin);
      end = levels_[level].rank0(end);
    }
  }
  return Interval{begin, end};
}

std::size_t ByteWaveletMatrix::rank(std::uint8_t value, std::size_t end) const {
  if (end > size_) {
    throw std::out_of_range("byte wavelet matrix rank end out of range");
  }
  const Interval interval = descend(value, 0U, end);
  return interval.end - interval.begin;
}

std::size_t ByteWaveletMatrix::rank(std::uint8_t value, std::size_t begin,
                                    std::size_t end) const {
  validate_range(begin, end);
  const Interval interval = descend(value, begin, end);
  return interval.end - interval.begin;
}

std::optional<std::size_t> ByteWaveletMatrix::select(
    std::uint8_t value, std::size_t ordinal) const {
  const Interval interval = descend(value, 0U, size_);
  if (ordinal >= interval.end - interval.begin) {
    return std::nullopt;
  }

  std::size_t position = interval.begin + ordinal;
  for (std::size_t reverse = kLevels; reverse > 0U; --reverse) {
    const std::size_t level = reverse - 1U;
    const std::size_t shift = (kLevels - 1U) - level;
    const std::uint32_t promoted = static_cast<std::uint32_t>(value);
    const bool bit = ((promoted >> shift) & std::uint32_t{1}) != 0U;
    const std::optional<std::size_t> previous =
        bit ? levels_[level].select1(position - zero_counts_[level])
            : levels_[level].select0(position);
    if (!previous.has_value()) {
      throw std::logic_error(
          "byte wavelet matrix inverse mapping invariant violated");
    }
    position = *previous;
  }
  return position;
}

std::uint8_t ByteWaveletMatrix::kth_smallest(
    std::size_t begin, std::size_t end, std::size_t ordinal) const {
  validate_range(begin, end);
  if (ordinal >= end - begin) {
    throw std::out_of_range("byte wavelet matrix quantile ordinal out of range");
  }

  std::uint8_t value = 0U;
  for (std::size_t level = 0U; level < kLevels; ++level) {
    const std::size_t zeros_before_begin = levels_[level].rank0(begin);
    const std::size_t zeros_before_end = levels_[level].rank0(end);
    const std::size_t zeros_in_range = zeros_before_end - zeros_before_begin;
    if (ordinal < zeros_in_range) {
      begin = zeros_before_begin;
      end = zeros_before_end;
    } else {
      ordinal -= zeros_in_range;
      const std::size_t shift = (kLevels - 1U) - level;
      value = static_cast<std::uint8_t>(
          static_cast<std::uint32_t>(value) | (std::uint32_t{1} << shift));
      begin = zero_counts_[level] + levels_[level].rank1(begin);
      end = zero_counts_[level] + levels_[level].rank1(end);
    }
  }
  return value;
}

std::size_t ByteWaveletMatrix::logical_payload_bytes() const noexcept {
  return logical_payload_bytes_;
}

void ByteWaveletMatrix::validate_range(std::size_t begin,
                                       std::size_t end) const {
  if (begin > end) {
    throw std::invalid_argument("byte wavelet matrix range begin exceeds end");
  }
  if (end > size_) {
    throw std::out_of_range("byte wavelet matrix range end out of range");
  }
}

}  // namespace algorithms::data_structures
