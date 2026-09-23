#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Bounded Raman-Raman-Rao-style bitvector.
//
// Every block contains at most 15 bits and is encoded by:
//   * class  = number of one bits;
//   * offset = combinatorial rank among bit patterns of the same class.
//
// Sixteen encoded blocks form one superblock. Superblock prefix counters bound
// rank work to at most fifteen complete blocks plus one decoded partial block.
// select uses a superblock prefix search followed by a bounded block scan.
//
// This implementation exposes exact access/rank/select semantics. It does not
// claim an entropy-optimal C++ object layout or benchmark-backed succinctness.
class RrrBitVector15 {
 public:
  using Bit = std::uint8_t;

  static constexpr std::size_t kBlockBits = 15U;
  static constexpr std::size_t kBlocksPerSuperblock = 16U;

  RrrBitVector15() { build(std::span<const Bit>{}); }

  explicit RrrBitVector15(const std::span<const Bit> bits) {
    build(bits);
  }

  explicit RrrBitVector15(const std::vector<Bit>& bits)
      : RrrBitVector15(std::span<const Bit>(bits)) {}

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t ones() const noexcept { return ones_; }
  [[nodiscard]] std::size_t zeros() const noexcept {
    return size_ - ones_;
  }
  [[nodiscard]] std::size_t block_count() const noexcept {
    return blocks_.size();
  }
  [[nodiscard]] std::size_t superblock_count() const noexcept {
    return superblock_ones_.empty() ? 0U : superblock_ones_.size() - 1U;
  }

  [[nodiscard]] bool access(const std::size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("RRR bitvector index out of range");
    }
    const std::size_t block = index / kBlockBits;
    const std::size_t offset = index % kBlockBits;
    const std::uint16_t decoded = decode(blocks_[block]);
    return ((decoded >> offset) & std::uint16_t{1}) != 0U;
  }

  // Number of one bits in [0,end).
  [[nodiscard]] std::size_t rank1(const std::size_t end) const {
    if (end > size_) {
      throw std::out_of_range("RRR bitvector rank endpoint out of range");
    }
    if (end == 0U) {
      return 0U;
    }
    if (end == size_ && end % kBlockBits == 0U) {
      return ones_;
    }

    const std::size_t block = end / kBlockBits;
    const std::size_t in_block = end % kBlockBits;
    const std::size_t superblock = block / kBlocksPerSuperblock;

    std::size_t total = superblock_ones_[superblock];
    const std::size_t first_block =
        superblock * kBlocksPerSuperblock;
    for (std::size_t current = first_block; current < block; ++current) {
      total += blocks_[current].ones;
    }

    if (in_block != 0U) {
      const std::uint16_t decoded = decode(blocks_[block]);
      const std::uint16_t mask =
          static_cast<std::uint16_t>((std::uint32_t{1} << in_block) - 1U);
      total += static_cast<std::size_t>(std::popcount(
          static_cast<std::uint16_t>(decoded & mask)));
    }
    return total;
  }

  // Number of zero bits in [0,end).
  [[nodiscard]] std::size_t rank0(const std::size_t end) const {
    if (end > size_) {
      throw std::out_of_range("RRR bitvector rank endpoint out of range");
    }
    return end - rank1(end);
  }

  // Zero-based ordinal of a one bit.
  [[nodiscard]] std::optional<std::size_t> select1(
      const std::size_t ordinal) const {
    if (ordinal >= ones_) {
      return std::nullopt;
    }

    const auto after = std::upper_bound(
        superblock_ones_.begin(), superblock_ones_.end(), ordinal);
    const std::size_t superblock =
        static_cast<std::size_t>(after - superblock_ones_.begin() - 1);
    std::size_t remaining = ordinal - superblock_ones_[superblock];

    const std::size_t first =
        superblock * kBlocksPerSuperblock;
    const std::size_t last =
        std::min(blocks_.size(), first + kBlocksPerSuperblock);

    for (std::size_t block = first; block < last; ++block) {
      const std::size_t count = blocks_[block].ones;
      if (remaining < count) {
        return block * kBlockBits +
               select_in_mask(decode(blocks_[block]),
                              blocks_[block].length, true, remaining);
      }
      remaining -= count;
    }
    throw std::logic_error("RRR one-select prefix invariant violated");
  }

  // Zero-based ordinal of a zero bit.
  [[nodiscard]] std::optional<std::size_t> select0(
      const std::size_t ordinal) const {
    if (ordinal >= zeros()) {
      return std::nullopt;
    }

    const auto after = std::upper_bound(
        superblock_zeros_.begin(), superblock_zeros_.end(), ordinal);
    const std::size_t superblock =
        static_cast<std::size_t>(after - superblock_zeros_.begin() - 1);
    std::size_t remaining = ordinal - superblock_zeros_[superblock];

    const std::size_t first =
        superblock * kBlocksPerSuperblock;
    const std::size_t last =
        std::min(blocks_.size(), first + kBlocksPerSuperblock);

    for (std::size_t block = first; block < last; ++block) {
      const std::size_t count =
          static_cast<std::size_t>(blocks_[block].length) -
          blocks_[block].ones;
      if (remaining < count) {
        return block * kBlockBits +
               select_in_mask(decode(blocks_[block]),
                              blocks_[block].length, false, remaining);
      }
      remaining -= count;
    }
    throw std::logic_error("RRR zero-select prefix invariant violated");
  }

  // Expensive representation replay. Excluded from query complexity claims.
  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      const std::size_t expected_blocks =
          size_ / kBlockBits +
          ((size_ % kBlockBits) != 0U ? 1U : 0U);
      if (blocks_.size() != expected_blocks) {
        return false;
      }

      const std::size_t expected_superblocks =
          expected_blocks / kBlocksPerSuperblock +
          ((expected_blocks % kBlocksPerSuperblock) != 0U ? 1U : 0U);
      if (superblock_ones_.size() != expected_superblocks + 1U ||
          superblock_zeros_.size() != expected_superblocks + 1U ||
          superblock_ones_.front() != 0U ||
          superblock_zeros_.front() != 0U) {
        return false;
      }

      std::size_t recomputed_ones = 0U;
      std::size_t recomputed_zeros = 0U;
      std::size_t superblock = 0U;

      for (std::size_t block = 0U; block < blocks_.size(); ++block) {
        if (block % kBlocksPerSuperblock == 0U) {
          if (superblock_ones_[superblock] != recomputed_ones ||
              superblock_zeros_[superblock] != recomputed_zeros) {
            return false;
          }
          ++superblock;
        }

        const std::size_t begin = block * kBlockBits;
        const std::size_t expected_length =
            std::min(kBlockBits, size_ - begin);
        const EncodedBlock& encoded = blocks_[block];

        if (encoded.length != expected_length ||
            encoded.ones > encoded.length ||
            encoded.offset >= choose(encoded.length, encoded.ones)) {
          return false;
        }

        const std::uint16_t decoded = decode(encoded);
        const std::uint16_t valid_mask =
            encoded.length == kBlockBits
                ? static_cast<std::uint16_t>((std::uint32_t{1}
                                              << kBlockBits) - 1U)
                : static_cast<std::uint16_t>(
                      (std::uint32_t{1} << encoded.length) - 1U);
        if ((decoded & static_cast<std::uint16_t>(~valid_mask)) != 0U ||
            static_cast<std::size_t>(std::popcount(decoded)) !=
                encoded.ones ||
            rank_mask(decoded, encoded.length, encoded.ones) !=
                encoded.offset) {
          return false;
        }

        recomputed_ones += encoded.ones;
        recomputed_zeros +=
            static_cast<std::size_t>(encoded.length) - encoded.ones;
      }

      if (superblock_ones_.back() != recomputed_ones ||
          superblock_zeros_.back() != recomputed_zeros ||
          recomputed_ones != ones_ ||
          recomputed_ones + recomputed_zeros != size_) {
        return false;
      }
      return true;
    } catch (...) {
      return false;
    }
  }

 private:
  struct EncodedBlock {
    std::uint8_t length{};
    std::uint8_t ones{};
    std::uint16_t offset{};
  };

  std::size_t size_{};
  std::size_t ones_{};
  std::vector<EncodedBlock> blocks_;
  std::vector<std::size_t> superblock_ones_;
  std::vector<std::size_t> superblock_zeros_;

  [[nodiscard]] static constexpr std::uint32_t choose(
      const std::size_t n, const std::size_t k) noexcept {
    if (k > n) {
      return 0U;
    }
    const std::size_t reduced = std::min(k, n - k);
    std::uint32_t result = 1U;
    for (std::size_t i = 1U; i <= reduced; ++i) {
      result = static_cast<std::uint32_t>(
          (result * static_cast<std::uint32_t>(n - reduced + i)) /
          static_cast<std::uint32_t>(i));
    }
    return result;
  }

  [[nodiscard]] static std::uint16_t rank_mask(
      const std::uint16_t mask, const std::size_t length,
      const std::size_t expected_ones) {
    std::size_t seen = 0U;
    std::uint32_t rank = 0U;
    for (std::size_t position = 0U; position < length; ++position) {
      if (((mask >> position) & std::uint16_t{1}) == 0U) {
        continue;
      }
      ++seen;
      rank += choose(position, seen);
    }
    if (seen != expected_ones ||
        rank >= choose(length, expected_ones) ||
        rank > std::numeric_limits<std::uint16_t>::max()) {
      throw std::logic_error("RRR combinatorial rank invariant violated");
    }
    return static_cast<std::uint16_t>(rank);
  }

  [[nodiscard]] static std::uint16_t decode(
      const EncodedBlock& encoded) {
    if (encoded.length == 0U || encoded.length > kBlockBits ||
        encoded.ones > encoded.length ||
        encoded.offset >= choose(encoded.length, encoded.ones)) {
      throw std::logic_error("RRR encoded block invalid");
    }

    std::uint32_t remaining = encoded.offset;
    std::size_t upper = encoded.length;
    std::uint16_t mask = 0U;

    for (std::size_t i = encoded.ones; i > 0U; --i) {
      std::size_t position = upper - 1U;
      while (choose(position, i) > remaining) {
        if (position == 0U) {
          throw std::logic_error("RRR combinatorial decode underflow");
        }
        --position;
      }
      mask = static_cast<std::uint16_t>(
          mask | static_cast<std::uint16_t>(std::uint16_t{1} << position));
      remaining -= choose(position, i);
      upper = position;
    }

    if (remaining != 0U) {
      throw std::logic_error("RRR combinatorial decode remainder");
    }
    return mask;
  }

  [[nodiscard]] static std::size_t select_in_mask(
      const std::uint16_t mask, const std::size_t length,
      const bool bit_value, std::size_t ordinal) {
    for (std::size_t position = 0U; position < length; ++position) {
      const bool current =
          ((mask >> position) & std::uint16_t{1}) != 0U;
      if (current == bit_value) {
        if (ordinal == 0U) {
          return position;
        }
        --ordinal;
      }
    }
    throw std::logic_error("RRR decoded select ordinal invalid");
  }

  void build(const std::span<const Bit> bits) {
    size_ = bits.size();
    ones_ = 0U;
    blocks_.clear();
    superblock_ones_.clear();
    superblock_zeros_.clear();

    const std::size_t block_count =
        size_ / kBlockBits +
        ((size_ % kBlockBits) != 0U ? 1U : 0U);
    blocks_.reserve(block_count);

    std::size_t prefix_ones = 0U;
    std::size_t prefix_zeros = 0U;
    superblock_ones_.push_back(0U);
    superblock_zeros_.push_back(0U);
    if (block_count == 0U) {
      return;
    }

    for (std::size_t block = 0U; block < block_count; ++block) {
      if (block != 0U && block % kBlocksPerSuperblock == 0U) {
        superblock_ones_.push_back(prefix_ones);
        superblock_zeros_.push_back(prefix_zeros);
      }

      const std::size_t begin = block * kBlockBits;
      const std::size_t length =
          std::min(kBlockBits, size_ - begin);
      std::uint16_t mask = 0U;
      std::size_t count = 0U;

      for (std::size_t offset = 0U; offset < length; ++offset) {
        const Bit bit = bits[begin + offset];
        if (bit > 1U) {
          throw std::invalid_argument("RRR input bit must be zero or one");
        }
        if (bit != 0U) {
          mask = static_cast<std::uint16_t>(
              mask | static_cast<std::uint16_t>(
                         std::uint16_t{1} << offset));
          ++count;
        }
      }

      EncodedBlock encoded;
      encoded.length = static_cast<std::uint8_t>(length);
      encoded.ones = static_cast<std::uint8_t>(count);
      encoded.offset = rank_mask(mask, length, count);
      blocks_.push_back(encoded);

      prefix_ones += count;
      prefix_zeros += length - count;
    }

    ones_ = prefix_ones;
    superblock_ones_.push_back(prefix_ones);
    superblock_zeros_.push_back(prefix_zeros);
  }
};

}  // namespace algorithms::data_structures
