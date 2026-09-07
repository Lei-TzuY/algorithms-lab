#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::data_structures {

class PackedRankSelectBitVector {
 public:
  explicit PackedRankSelectBitVector(std::span<const std::uint8_t> bits);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] bool bit(std::size_t index) const;

  // Number of one/zero bits in the half-open prefix [0, end).
  [[nodiscard]] std::size_t rank1(std::size_t end) const;
  [[nodiscard]] std::size_t rank0(std::size_t end) const;

  // Position of the zero-based ordinal-th one/zero bit, or nullopt if absent.
  [[nodiscard]] std::optional<std::size_t> select1(std::size_t ordinal) const noexcept;
  [[nodiscard]] std::optional<std::size_t> select0(std::size_t ordinal) const noexcept;

  [[nodiscard]] std::size_t one_count() const noexcept;
  [[nodiscard]] std::size_t zero_count() const noexcept;
  [[nodiscard]] std::size_t packed_word_count() const noexcept;
  [[nodiscard]] std::size_t rank_index_entries() const noexcept;

  // Logical bytes occupied by the packed words and rank-prefix entries only.
  // This intentionally excludes vector object/allocator/capacity overhead.
  [[nodiscard]] std::size_t logical_payload_bytes() const noexcept;

 private:
  static constexpr std::size_t kWordBits = 64U;

  [[nodiscard]] std::size_t zeros_before_word(std::size_t word_index) const noexcept;
  [[nodiscard]] std::uint64_t valid_mask_for_word(std::size_t word_index) const noexcept;
  [[nodiscard]] std::optional<std::size_t> select_in_word(
      std::uint64_t word, std::size_t word_index, std::size_t ordinal_in_word) const noexcept;

  std::size_t size_ = 0U;
  std::vector<std::uint64_t> words_;
  std::vector<std::size_t> prefix_ones_;
  std::size_t logical_payload_bytes_ = 0U;
};

}  // namespace algorithms::data_structures
