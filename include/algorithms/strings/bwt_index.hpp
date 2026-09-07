#pragma once

#include "algorithms/data_structures/byte_wavelet_matrix.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace algorithms::strings {

class BwtByteIndex {
 public:
  explicit BwtByteIndex(std::string_view text);

  [[nodiscard]] std::size_t text_size() const noexcept;
  [[nodiscard]] std::size_t row_count() const noexcept;

  // Exact substring search over arbitrary bytes. The empty pattern matches
  // every boundary position 0..text_size(), matching the repository's KMP
  // convention.
  [[nodiscard]] std::size_t count(std::string_view pattern) const;
  [[nodiscard]] std::vector<std::size_t> locate(std::string_view pattern) const;

 private:
  struct SearchRange {
    std::size_t begin;
    std::size_t end;
  };

  struct BuildState {
    std::size_t text_size = 0U;
    std::size_t sentinel_row = 0U;
    std::array<std::size_t, 256U> cumulative{};
    std::vector<std::size_t> row_positions;
    std::vector<std::uint8_t> bwt_bytes;
  };

  explicit BwtByteIndex(BuildState state);
  [[nodiscard]] static BuildState build(std::string_view text);
  [[nodiscard]] SearchRange backward_search(std::string_view pattern) const;
  [[nodiscard]] std::size_t occurrence(std::uint8_t value,
                                       std::size_t row_end) const;

  std::size_t text_size_ = 0U;
  std::size_t sentinel_row_ = 0U;
  std::array<std::size_t, 256U> cumulative_{};
  std::vector<std::size_t> row_positions_;
  algorithms::data_structures::ByteWaveletMatrix bwt_;
};

}  // namespace algorithms::strings
