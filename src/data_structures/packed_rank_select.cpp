#include "algorithms/data_structures/packed_rank_select.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace algorithms::data_structures {
namespace {

std::size_t checked_add(std::size_t lhs, std::size_t rhs) {
  if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
    throw std::length_error("packed rank/select size overflow");
  }
  return lhs + rhs;
}

std::size_t checked_mul(std::size_t lhs, std::size_t rhs) {
  if (lhs != 0U && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
    throw std::length_error("packed rank/select size overflow");
  }
  return lhs * rhs;
}

}  // namespace

PackedRankSelectBitVector::PackedRankSelectBitVector(
    std::span<const std::uint8_t> bits)
    : size_(bits.size()) {
  const std::size_t word_count =
      size_ / kWordBits + (size_ % kWordBits != 0U ? 1U : 0U);
  if (word_count == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("packed rank/select index size overflow");
  }

  words_.assign(word_count, 0U);
  prefix_ones_.assign(word_count + 1U, 0U);

  for (std::size_t index = 0U; index < size_; ++index) {
    if (bits[index] > 1U) {
      throw std::invalid_argument("packed rank/select bits must be 0 or 1");
    }
    if (bits[index] != 0U) {
      words_[index / kWordBits] |=
          std::uint64_t{1} << (index % kWordBits);
    }
  }

  for (std::size_t word = 0U; word < word_count; ++word) {
    prefix_ones_[word + 1U] =
        prefix_ones_[word] +
        static_cast<std::size_t>(std::popcount(words_[word]));
  }

  logical_payload_bytes_ = checked_add(
      checked_mul(words_.size(), sizeof(std::uint64_t)),
      checked_mul(prefix_ones_.size(), sizeof(std::size_t)));
}

std::size_t PackedRankSelectBitVector::size() const noexcept { return size_; }

bool PackedRankSelectBitVector::empty() const noexcept { return size_ == 0U; }

bool PackedRankSelectBitVector::bit(std::size_t index) const {
  if (index >= size_) {
    throw std::out_of_range("packed rank/select bit index out of range");
  }
  return ((words_[index / kWordBits] >> (index % kWordBits)) &
          std::uint64_t{1}) != 0U;
}

std::size_t PackedRankSelectBitVector::rank1(std::size_t end) const {
  if (end > size_) {
    throw std::out_of_range("packed rank/select rank end out of range");
  }
  if (end == 0U) {
    return 0U;
  }

  const std::size_t full_words = end / kWordBits;
  const std::size_t remainder = end % kWordBits;
  std::size_t result = prefix_ones_[full_words];
  if (remainder != 0U) {
    const std::uint64_t mask = (std::uint64_t{1} << remainder) - 1U;
    result += static_cast<std::size_t>(
        std::popcount(words_[full_words] & mask));
  }
  return result;
}

std::size_t PackedRankSelectBitVector::rank0(std::size_t end) const {
  return end - rank1(end);
}

std::optional<std::size_t> PackedRankSelectBitVector::select1(
    std::size_t ordinal) const noexcept {
  if (prefix_ones_.empty() || ordinal >= one_count()) {
    return std::nullopt;
  }

  const auto it =
      std::upper_bound(prefix_ones_.begin(), prefix_ones_.end(), ordinal);
  const std::size_t word_index =
      static_cast<std::size_t>(it - prefix_ones_.begin() - 1);
  const std::size_t ordinal_in_word =
      ordinal - prefix_ones_[word_index];
  return select_in_word(words_[word_index], word_index, ordinal_in_word);
}

std::optional<std::size_t> PackedRankSelectBitVector::select0(
    std::size_t ordinal) const noexcept {
  if (ordinal >= zero_count()) {
    return std::nullopt;
  }

  std::size_t low = 0U;
  std::size_t high = words_.size();
  while (low < high) {
    const std::size_t mid = low + (high - low) / 2U;
    const std::size_t zeros_through_mid = zeros_before_word(mid + 1U);
    if (zeros_through_mid > ordinal) {
      high = mid;
    } else {
      low = mid + 1U;
    }
  }

  const std::size_t word_index = low;
  const std::size_t ordinal_in_word =
      ordinal - zeros_before_word(word_index);
  const std::uint64_t zero_word =
      (~words_[word_index]) & valid_mask_for_word(word_index);
  return select_in_word(zero_word, word_index, ordinal_in_word);
}

std::size_t PackedRankSelectBitVector::one_count() const noexcept {
  return prefix_ones_.empty() ? 0U : prefix_ones_.back();
}

std::size_t PackedRankSelectBitVector::zero_count() const noexcept {
  return size_ - one_count();
}

std::size_t PackedRankSelectBitVector::packed_word_count() const noexcept {
  return words_.size();
}

std::size_t PackedRankSelectBitVector::rank_index_entries() const noexcept {
  return prefix_ones_.size();
}

std::size_t PackedRankSelectBitVector::logical_payload_bytes() const noexcept {
  return logical_payload_bytes_;
}

std::size_t PackedRankSelectBitVector::zeros_before_word(
    std::size_t word_index) const noexcept {
  const std::size_t bits_before =
      word_index == words_.size() ? size_ : word_index * kWordBits;
  return bits_before - prefix_ones_[word_index];
}

std::uint64_t PackedRankSelectBitVector::valid_mask_for_word(
    std::size_t word_index) const noexcept {
  if (word_index + 1U < words_.size() || size_ % kWordBits == 0U) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  const std::size_t valid_bits = size_ % kWordBits;
  return (std::uint64_t{1} << valid_bits) - 1U;
}

std::optional<std::size_t> PackedRankSelectBitVector::select_in_word(
    std::uint64_t word, std::size_t word_index,
    std::size_t ordinal_in_word) const noexcept {
  for (std::size_t skipped = 0U; skipped < ordinal_in_word; ++skipped) {
    word &= word - 1U;
  }
  if (word == 0U) {
    return std::nullopt;
  }
  const std::size_t bit_offset =
      static_cast<std::size_t>(std::countr_zero(word));
  const std::size_t position = word_index * kWordBits + bit_offset;
  return position < size_ ? std::optional<std::size_t>{position}
                          : std::nullopt;
}

}  // namespace algorithms::data_structures
