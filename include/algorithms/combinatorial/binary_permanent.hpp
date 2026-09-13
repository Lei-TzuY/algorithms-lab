#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {

inline constexpr std::size_t kMaxExactBinaryPermanentDimension = 20;

struct BinaryPermanentResult {
  std::uint64_t permanent{};
  std::size_t dimension{};
  std::uint64_t subsets_evaluated{};

  friend bool operator==(const BinaryPermanentResult&,
                         const BinaryPermanentResult&) = default;
};

namespace binary_permanent_detail {

inline constexpr std::uint64_t kMod1 = 2013265921ULL;
inline constexpr std::uint64_t kMod2 = 1811939329ULL;
inline constexpr std::uint64_t kMod1InverseMod2 = 1811939320ULL;
inline constexpr std::uint64_t kCombinedModulus = kMod1 * kMod2;
inline constexpr std::uint64_t kFactorial20 = 2432902008176640000ULL;

static_assert(std::gcd(kMod1, kMod2) == 1ULL);
static_assert(((kMod1 % kMod2) * kMod1InverseMod2) % kMod2 == 1ULL);
static_assert(kCombinedModulus > kFactorial20);

[[nodiscard]] constexpr std::uint64_t factorial_bound(std::size_t n) {
  std::uint64_t result = 1;
  for (std::size_t value = 2; value <= n; ++value) {
    result *= static_cast<std::uint64_t>(value);
  }
  return result;
}

[[nodiscard]] constexpr std::uint64_t add_mod(std::uint64_t left,
                                               std::uint64_t right,
                                               std::uint64_t modulus) {
  const std::uint64_t sum = left + right;
  return sum >= modulus ? sum - modulus : sum;
}

[[nodiscard]] constexpr std::uint64_t subtract_mod(std::uint64_t left,
                                                    std::uint64_t right,
                                                    std::uint64_t modulus) {
  return left >= right ? left - right : modulus - (right - left);
}

[[nodiscard]] inline std::uint64_t reconstruct_crt(std::uint64_t residue1,
                                                    std::uint64_t residue2) {
  const std::uint64_t residue1_mod2 = residue1 % kMod2;
  const std::uint64_t delta =
      residue2 >= residue1_mod2 ? residue2 - residue1_mod2
                               : kMod2 - (residue1_mod2 - residue2);
  const std::uint64_t multiplier =
      (delta * kMod1InverseMod2) % kMod2;
  return residue1 + kMod1 * multiplier;
}

}  // namespace binary_permanent_detail

// Exact permanent of an n-by-n 0/1 matrix represented by row bitmasks.
// Bit j of row i is matrix entry A[i][j]. The bounded contract n <= 20
// permits unique CRT reconstruction because permanent(A) <= n! <= 20! and the
// product of the two coprime moduli used internally is larger than 20!.
[[nodiscard]] inline BinaryPermanentResult binary_matrix_permanent(
    std::span<const std::uint64_t> row_masks) {
  using namespace binary_permanent_detail;

  const std::size_t n = row_masks.size();
  if (n > kMaxExactBinaryPermanentDimension) {
    throw std::length_error("binary permanent dimension exceeds exact bound");
  }
  if (n == 0) {
    return BinaryPermanentResult{1, 0, 1};
  }

  const std::uint64_t allowed_mask = (std::uint64_t{1} << n) - 1ULL;
  std::uint64_t covered_columns = 0;
  bool has_zero_row = false;
  for (const std::uint64_t row_mask : row_masks) {
    if ((row_mask & ~allowed_mask) != 0ULL) {
      throw std::invalid_argument("binary permanent row mask exceeds dimension");
    }
    has_zero_row = has_zero_row || row_mask == 0ULL;
    covered_columns |= row_mask;
  }
  if (has_zero_row || covered_columns != allowed_mask) {
    return BinaryPermanentResult{0, n, 0};
  }

  std::vector<std::size_t> row_sums(n, 0);
  std::uint64_t residue1 = 0;
  std::uint64_t residue2 = 0;
  const std::uint64_t total_masks = std::uint64_t{1} << n;
  std::uint64_t previous_gray = 0;

  for (std::uint64_t step = 1; step < total_masks; ++step) {
    const std::uint64_t gray = step ^ (step >> 1U);
    const std::uint64_t changed = gray ^ previous_gray;
    const std::size_t column =
        static_cast<std::size_t>(std::countr_zero(changed));
    const bool added = (gray & changed) != 0ULL;

    for (std::size_t row = 0; row < n; ++row) {
      if ((row_masks[row] & (std::uint64_t{1} << column)) == 0ULL) {
        continue;
      }
      if (added) {
        ++row_sums[row];
      } else {
        if (row_sums[row] == 0) {
          throw std::logic_error("binary permanent Gray update underflow");
        }
        --row_sums[row];
      }
    }

    std::uint64_t product1 = 1;
    std::uint64_t product2 = 1;
    for (const std::size_t row_sum : row_sums) {
      product1 = (product1 * static_cast<std::uint64_t>(row_sum)) % kMod1;
      product2 = (product2 * static_cast<std::uint64_t>(row_sum)) % kMod2;
    }

    const std::size_t subset_size =
        static_cast<std::size_t>(std::popcount(gray));
    const bool positive = ((n - subset_size) % 2U) == 0U;
    if (positive) {
      residue1 = add_mod(residue1, product1, kMod1);
      residue2 = add_mod(residue2, product2, kMod2);
    } else {
      residue1 = subtract_mod(residue1, product1, kMod1);
      residue2 = subtract_mod(residue2, product2, kMod2);
    }

    previous_gray = gray;
  }

  const std::uint64_t permanent = reconstruct_crt(residue1, residue2);
  if (permanent > factorial_bound(n)) {
    throw std::logic_error("binary permanent CRT reconstruction exceeds n! bound");
  }
  return BinaryPermanentResult{permanent, n, total_masks - 1ULL};
}

}  // namespace algorithms::combinatorial
