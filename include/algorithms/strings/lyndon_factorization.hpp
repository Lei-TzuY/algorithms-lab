#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace algorithms::strings {

struct LyndonFactor {
  std::size_t begin;
  std::size_t end;

  friend bool operator==(const LyndonFactor&, const LyndonFactor&) = default;
};

// Return the Chen-Fox-Lyndon factorization of text as contiguous half-open
// ranges. Lexicographic order is byte order 0..255, independent of whether
// plain char is signed on the host platform.
[[nodiscard]] inline std::vector<LyndonFactor> duval_lyndon_factorization(
    std::string_view text) {
  std::vector<LyndonFactor> factors;
  std::size_t begin = 0U;

  while (begin < text.size()) {
    std::size_t scan = begin + 1U;
    std::size_t compare = begin;

    while (scan < text.size()) {
      const auto left = static_cast<unsigned char>(text[compare]);
      const auto right = static_cast<unsigned char>(text[scan]);
      if (left > right) {
        break;
      }
      if (left < right) {
        compare = begin;
      } else {
        ++compare;
      }
      ++scan;
    }

    const std::size_t factor_length = scan - compare;
    while (begin <= compare) {
      factors.push_back(LyndonFactor{begin, begin + factor_length});
      begin += factor_length;
    }
  }

  return factors;
}

}  // namespace algorithms::strings
