#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace algorithms::strings {

struct MyersEditDistance64Result {
  std::size_t distance{0};
  std::uint64_t positive_vertical{0};
  std::uint64_t negative_vertical{0};
};

// Exact unit-cost Levenshtein distance using Myers' single-word bit-vector
// recurrence. The pattern is the bit-parallel dimension and is therefore
// limited to 64 bytes; the text length is unrestricted by the algorithm.
// Arbitrary byte values are supported.
//
// After consuming text[0,j), bit i (0 <= i < pattern.size()) of
// positive_vertical is set exactly when D[i+1,j]-D[i,j] == +1; the matching
// bit of negative_vertical is set exactly when that difference is -1. If both
// are clear, the difference is zero. Here D is the ordinary Levenshtein DP.
// The public distance is D[pattern.size(), text.size()].
//
// Time: O(pattern.size() + text.size() + 256) fixed-word operations.
// Auxiliary storage: O(256) machine words. This is a 64-bit bounded baseline,
// not a multiword Myers implementation.
[[nodiscard]] inline MyersEditDistance64Result myers_levenshtein_distance_64(
    std::string_view pattern, std::string_view text) {
  if (pattern.size() > 64U) {
    throw std::length_error("Myers bit-vector pattern exceeds 64 bytes");
  }

  if (pattern.empty()) {
    return MyersEditDistance64Result{text.size(), 0U, 0U};
  }

  std::array<std::uint64_t, 256> equality_masks{};
  for (std::size_t index = 0; index < pattern.size(); ++index) {
    const auto byte = static_cast<unsigned char>(pattern[index]);
    equality_masks[byte] |= (std::uint64_t{1} << index);
  }

  const std::uint64_t valid_mask =
      pattern.size() == 64U
          ? ~std::uint64_t{0}
          : ((std::uint64_t{1} << pattern.size()) - std::uint64_t{1});
  const std::uint64_t high_bit =
      std::uint64_t{1} << (pattern.size() - 1U);

  std::uint64_t positive_vertical = valid_mask;
  std::uint64_t negative_vertical = 0U;
  std::size_t score = pattern.size();

  for (const char raw_character : text) {
    const auto byte = static_cast<unsigned char>(raw_character);
    const std::uint64_t equal = equality_masks[byte];
    const std::uint64_t vertical = equal | negative_vertical;

    // Unsigned wraparound here is the intended fixed-width carry behavior of
    // the Myers recurrence, not arithmetic overflow of a numeric result.
    const std::uint64_t horizontal =
        ((((equal & positive_vertical) + positive_vertical) ^
          positive_vertical) |
         equal) &
        valid_mask;

    std::uint64_t positive_horizontal =
        negative_vertical | ~(horizontal | positive_vertical);
    std::uint64_t negative_horizontal = positive_vertical & horizontal;
    positive_horizontal &= valid_mask;
    negative_horizontal &= valid_mask;

    if ((positive_horizontal & high_bit) != 0U) {
      ++score;
    } else if ((negative_horizontal & high_bit) != 0U) {
      if (score == 0U) {
        throw std::logic_error("invalid Myers score underflow");
      }
      --score;
    }

    positive_horizontal = ((positive_horizontal << 1U) | 1U) & valid_mask;
    negative_horizontal = (negative_horizontal << 1U) & valid_mask;

    positive_vertical =
        (negative_horizontal | ~(vertical | positive_horizontal)) & valid_mask;
    negative_vertical = positive_horizontal & vertical;
  }

  return MyersEditDistance64Result{score, positive_vertical,
                                   negative_vertical};
}

}  // namespace algorithms::strings
