#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {
namespace subset_convolution_detail {

inline std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                             std::uint64_t modulus) noexcept {
  // Preconditions: modulus >= 2 and a,b < modulus.
  if (a >= modulus - b) {
    return a - (modulus - b);
  }
  return a + b;
}

inline std::uint64_t subtract_mod(std::uint64_t a, std::uint64_t b,
                                  std::uint64_t modulus) noexcept {
  // Preconditions: modulus >= 2 and a,b < modulus.
  if (a >= b) {
    return a - b;
  }
  return modulus - (b - a);
}

inline std::size_t ranked_index(std::size_t rank, std::size_t mask,
                                std::size_t table_size) noexcept {
  return rank * table_size + mask;
}

inline void subset_zeta_transform(std::vector<std::uint64_t>& ranked,
                                  std::size_t rank_count,
                                  std::size_t table_size,
                                  std::uint64_t modulus) {
  for (std::size_t bit = 1U; bit < table_size; bit <<= 1U) {
    for (std::size_t mask = 0U; mask < table_size; ++mask) {
      if ((mask & bit) == 0U) {
        continue;
      }
      const std::size_t without = mask ^ bit;
      for (std::size_t rank = 0U; rank < rank_count; ++rank) {
        const std::size_t here = ranked_index(rank, mask, table_size);
        const std::size_t previous =
            ranked_index(rank, without, table_size);
        ranked[here] = add_mod(ranked[here], ranked[previous], modulus);
      }
    }
  }
}

inline void subset_mobius_transform(std::vector<std::uint64_t>& ranked,
                                    std::size_t rank_count,
                                    std::size_t table_size,
                                    std::uint64_t modulus) {
  for (std::size_t bit = 1U; bit < table_size; bit <<= 1U) {
    for (std::size_t mask = 0U; mask < table_size; ++mask) {
      if ((mask & bit) == 0U) {
        continue;
      }
      const std::size_t without = mask ^ bit;
      for (std::size_t rank = 0U; rank < rank_count; ++rank) {
        const std::size_t here = ranked_index(rank, mask, table_size);
        const std::size_t previous =
            ranked_index(rank, without, table_size);
        ranked[here] =
            subtract_mod(ranked[here], ranked[previous], modulus);
      }
    }
  }
}

}  // namespace subset_convolution_detail

// Computes h[S] = sum_{T subseteq S} f[T] * g[S \\ T] (mod modulus).
// Input tables use the usual bit-mask subset order and must have equal,
// non-zero, power-of-two length. modulus may be composite, but must be >= 2.
//
// This is the ranked-zeta subset-convolution algorithm. For N = 2^n table
// entries it uses O(n^2 N) modular ring operations and O(n N) workspace.
[[nodiscard]] inline std::vector<std::uint64_t> subset_convolution_mod(
    std::span<const std::uint64_t> first,
    std::span<const std::uint64_t> second, std::uint64_t modulus) {
  if (first.size() != second.size()) {
    throw std::invalid_argument("subset-convolution tables must have equal size");
  }
  if (first.empty() || !std::has_single_bit(first.size())) {
    throw std::invalid_argument(
        "subset-convolution table size must be a non-zero power of two");
  }
  if (modulus < 2U) {
    throw std::invalid_argument("subset-convolution modulus must be at least 2");
  }

  const std::size_t table_size = first.size();
  const std::size_t bit_count =
      static_cast<std::size_t>(std::bit_width(table_size)) - 1U;
  const std::size_t rank_count = bit_count + 1U;
  if (rank_count > std::numeric_limits<std::size_t>::max() / table_size) {
    throw std::length_error("subset-convolution ranked workspace overflows size_t");
  }
  const std::size_t workspace_size = rank_count * table_size;

  std::vector<std::uint64_t> ranked_first(workspace_size, 0U);
  std::vector<std::uint64_t> ranked_second(workspace_size, 0U);
  std::vector<std::uint64_t> ranked_product(workspace_size, 0U);

  for (std::size_t mask = 0U; mask < table_size; ++mask) {
    const std::size_t rank = static_cast<std::size_t>(std::popcount(mask));
    ranked_first[subset_convolution_detail::ranked_index(
        rank, mask, table_size)] = first[mask] % modulus;
    ranked_second[subset_convolution_detail::ranked_index(
        rank, mask, table_size)] = second[mask] % modulus;
  }

  subset_convolution_detail::subset_zeta_transform(
      ranked_first, rank_count, table_size, modulus);
  subset_convolution_detail::subset_zeta_transform(
      ranked_second, rank_count, table_size, modulus);

  for (std::size_t mask = 0U; mask < table_size; ++mask) {
    for (std::size_t rank = 0U; rank < rank_count; ++rank) {
      std::uint64_t sum = 0U;
      for (std::size_t left_rank = 0U; left_rank <= rank; ++left_rank) {
        const std::size_t right_rank = rank - left_rank;
        const std::uint64_t left =
            ranked_first[subset_convolution_detail::ranked_index(
                left_rank, mask, table_size)];
        const std::uint64_t right =
            ranked_second[subset_convolution_detail::ranked_index(
                right_rank, mask, table_size)];
        const std::uint64_t product =
            algorithms::number_theory::multiply_mod(left, right, modulus);
        sum = subset_convolution_detail::add_mod(sum, product, modulus);
      }
      ranked_product[subset_convolution_detail::ranked_index(
          rank, mask, table_size)] = sum;
    }
  }

  subset_convolution_detail::subset_mobius_transform(
      ranked_product, rank_count, table_size, modulus);

  std::vector<std::uint64_t> result(table_size, 0U);
  for (std::size_t mask = 0U; mask < table_size; ++mask) {
    const std::size_t rank = static_cast<std::size_t>(std::popcount(mask));
    result[mask] = ranked_product[subset_convolution_detail::ranked_index(
        rank, mask, table_size)];
  }
  return result;
}

}  // namespace algorithms::combinatorial
