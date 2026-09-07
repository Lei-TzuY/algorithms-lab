#pragma once

#include "algorithms/data_structures/packed_rank_select.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::data_structures {

class ByteWaveletMatrix {
 public:
  static constexpr std::size_t kLevels = 8U;

  explicit ByteWaveletMatrix(std::span<const std::uint8_t> values);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] std::uint8_t access(std::size_t index) const;

  [[nodiscard]] std::size_t rank(std::uint8_t value, std::size_t end) const;
  [[nodiscard]] std::size_t rank(std::uint8_t value, std::size_t begin,
                                 std::size_t end) const;
  [[nodiscard]] std::optional<std::size_t> select(std::uint8_t value,
                                                   std::size_t ordinal) const;

  [[nodiscard]] std::uint8_t kth_smallest(std::size_t begin, std::size_t end,
                                          std::size_t ordinal) const;

  [[nodiscard]] std::size_t logical_payload_bytes() const noexcept;

 private:
  struct Interval {
    std::size_t begin;
    std::size_t end;
  };

  [[nodiscard]] Interval descend(std::uint8_t value, std::size_t begin,
                                 std::size_t end) const;
  void validate_range(std::size_t begin, std::size_t end) const;

  std::size_t size_ = 0U;
  std::vector<PackedRankSelectBitVector> levels_;
  std::array<std::size_t, kLevels> zero_counts_{};
  std::size_t logical_payload_bytes_ = 0U;
};

}  // namespace algorithms::data_structures
