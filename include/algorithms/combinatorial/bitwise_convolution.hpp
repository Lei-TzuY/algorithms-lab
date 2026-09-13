#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {

enum class BitwiseConvolutionKind : std::uint8_t { bit_or, bit_and, bit_xor };

namespace bitwise_convolution_detail {

inline std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                             std::uint64_t modulus) noexcept {
  return a >= modulus - b ? a - (modulus - b) : a + b;
}

inline std::uint64_t subtract_mod(std::uint64_t a, std::uint64_t b,
                                  std::uint64_t modulus) noexcept {
  return a >= b ? a - b : modulus - (b - a);
}

inline void or_transform(std::vector<std::uint64_t>& values,
                         std::uint64_t modulus, bool inverse) {
  const std::size_t n = values.size();
  for (std::size_t bit = 1U; bit < n; bit <<= 1U) {
    for (std::size_t mask = 0U; mask < n; ++mask) {
      if ((mask & bit) == 0U) {
        continue;
      }
      const std::size_t without = mask ^ bit;
      values[mask] = inverse ? subtract_mod(values[mask], values[without], modulus)
                             : add_mod(values[mask], values[without], modulus);
    }
  }
}

inline void and_transform(std::vector<std::uint64_t>& values,
                          std::uint64_t modulus, bool inverse) {
  const std::size_t n = values.size();
  for (std::size_t bit = 1U; bit < n; bit <<= 1U) {
    for (std::size_t mask = 0U; mask < n; ++mask) {
      if ((mask & bit) != 0U) {
        continue;
      }
      const std::size_t with = mask | bit;
      values[mask] = inverse ? subtract_mod(values[mask], values[with], modulus)
                             : add_mod(values[mask], values[with], modulus);
    }
  }
}

inline void xor_transform(std::vector<std::uint64_t>& values,
                          std::uint64_t modulus, bool inverse) {
  const std::size_t n = values.size();
  for (std::size_t block = 1U; block < n; block <<= 1U) {
    const std::size_t width = block << 1U;
    for (std::size_t start = 0U; start < n; start += width) {
      for (std::size_t offset = 0U; offset < block; ++offset) {
        const std::size_t left_index = start + offset;
        const std::size_t right_index = left_index + block;
        const std::uint64_t left = values[left_index];
        const std::uint64_t right = values[right_index];
        values[left_index] = add_mod(left, right, modulus);
        values[right_index] = subtract_mod(left, right, modulus);
      }
    }
  }
  if (!inverse) {
    return;
  }

  // n is a power of two and modulus is odd, so n is invertible mod modulus.
  // inv(2) = (modulus + 1) / 2, written without overflowing UINT64_MAX.
  const std::uint64_t inverse_two = (modulus >> 1U) + 1U;
  std::uint64_t inverse_n = 1U;
  const std::size_t bits = static_cast<std::size_t>(std::bit_width(n)) - 1U;
  for (std::size_t index = 0U; index < bits; ++index) {
    inverse_n = algorithms::number_theory::multiply_mod(
        inverse_n, inverse_two, modulus);
  }
  for (std::uint64_t& value : values) {
    value = algorithms::number_theory::multiply_mod(value, inverse_n, modulus);
  }
}

}  // namespace bitwise_convolution_detail

// Computes modular bitwise convolution over equal non-empty power-of-two tables:
// OR:  h[s] = sum_{a|b=s} f[a]g[b]
// AND: h[s] = sum_{a&b=s} f[a]g[b]
// XOR: h[s] = sum_{a^b=s} f[a]g[b]
//
// OR/AND support every modulus >= 2. XOR additionally requires an odd modulus
// so the power-of-two table length is invertible. For N table entries, the
// transform layer uses O(N log N) modular additions/subtractions, plus O(N)
// overflow-safe modular multiplications for pointwise products (and XOR scaling).
[[nodiscard]] inline std::vector<std::uint64_t> bitwise_convolution_mod(
    std::span<const std::uint64_t> first,
    std::span<const std::uint64_t> second, std::uint64_t modulus,
    BitwiseConvolutionKind kind) {
  if (first.size() != second.size()) {
    throw std::invalid_argument("bitwise-convolution tables must have equal size");
  }
  if (first.empty() || !std::has_single_bit(first.size())) {
    throw std::invalid_argument(
        "bitwise-convolution table size must be a non-zero power of two");
  }
  if (modulus < 2U) {
    throw std::invalid_argument("bitwise-convolution modulus must be at least 2");
  }
  if (kind == BitwiseConvolutionKind::bit_xor && (modulus & 1U) == 0U) {
    throw std::invalid_argument("XOR convolution requires an odd modulus");
  }

  std::vector<std::uint64_t> transformed_first(first.size());
  std::vector<std::uint64_t> transformed_second(second.size());
  for (std::size_t index = 0U; index < first.size(); ++index) {
    transformed_first[index] = first[index] % modulus;
    transformed_second[index] = second[index] % modulus;
  }

  const auto transform = [kind, modulus](std::vector<std::uint64_t>& values,
                                         bool inverse) {
    switch (kind) {
      case BitwiseConvolutionKind::bit_or:
        bitwise_convolution_detail::or_transform(values, modulus, inverse);
        return;
      case BitwiseConvolutionKind::bit_and:
        bitwise_convolution_detail::and_transform(values, modulus, inverse);
        return;
      case BitwiseConvolutionKind::bit_xor:
        bitwise_convolution_detail::xor_transform(values, modulus, inverse);
        return;
    }
    throw std::invalid_argument("unknown bitwise convolution kind");
  };

  transform(transformed_first, false);
  transform(transformed_second, false);
  for (std::size_t index = 0U; index < transformed_first.size(); ++index) {
    transformed_first[index] = algorithms::number_theory::multiply_mod(
        transformed_first[index], transformed_second[index], modulus);
  }
  transform(transformed_first, true);
  return transformed_first;
}

}  // namespace algorithms::combinatorial
