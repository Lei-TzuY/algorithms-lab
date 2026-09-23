#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {

namespace detail {

[[nodiscard]] inline std::int64_t charpoly_checked_add(
    const std::int64_t lhs, const std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if ((rhs > 0 && lhs > kMax - rhs) ||
      (rhs < 0 && lhs < kMin - rhs)) {
    throw std::overflow_error(
        "characteristic polynomial addition overflow");
  }
  return lhs + rhs;
}

[[nodiscard]] inline std::int64_t charpoly_checked_mul(
    const std::int64_t lhs, const std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();

  if (lhs == 0 || rhs == 0) {
    return 0;
  }

  if (lhs > 0) {
    if (rhs > 0) {
      if (lhs > kMax / rhs) {
        throw std::overflow_error(
            "characteristic polynomial multiplication overflow");
      }
    } else if (rhs < kMin / lhs) {
      throw std::overflow_error(
          "characteristic polynomial multiplication overflow");
    }
  } else {
    if (rhs > 0) {
      if (lhs < kMin / rhs) {
        throw std::overflow_error(
            "characteristic polynomial multiplication overflow");
      }
    } else if (lhs < kMax / rhs) {
      throw std::overflow_error(
          "characteristic polynomial multiplication overflow");
    }
  }

  return lhs * rhs;
}

[[nodiscard]] inline std::int64_t charpoly_checked_negate(
    const std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min()) {
    throw std::overflow_error(
        "characteristic polynomial negation overflow");
  }
  return -value;
}

[[nodiscard]] inline std::int64_t charpoly_exact_divide(
    const std::int64_t numerator, const std::int64_t denominator) {
  if (denominator <= 0) {
    throw std::logic_error(
        "characteristic polynomial divisor must be positive");
  }
  if (numerator % denominator != 0) {
    throw std::logic_error(
        "Faddeev-LeVerrier division was not exact");
  }
  return numerator / denominator;
}

using CharacteristicPolynomialMatrix =
    std::vector<std::vector<std::int64_t>>;

[[nodiscard]] inline CharacteristicPolynomialMatrix charpoly_multiply(
    const CharacteristicPolynomialMatrix& left,
    const CharacteristicPolynomialMatrix& right) {
  const std::size_t n = left.size();
  CharacteristicPolynomialMatrix product(
      n, std::vector<std::int64_t>(n, 0));

  for (std::size_t row = 0U; row < n; ++row) {
    for (std::size_t column = 0U; column < n; ++column) {
      std::int64_t sum = 0;
      for (std::size_t inner = 0U; inner < n; ++inner) {
        const std::int64_t term =
            charpoly_checked_mul(left[row][inner],
                                 right[inner][column]);
        sum = charpoly_checked_add(sum, term);
      }
      product[row][column] = sum;
    }
  }

  return product;
}

}  // namespace detail

// Returns the exact monic characteristic polynomial det(lambda I - A) of a
// square signed-64-bit integer matrix.
//
// Coefficients are returned in descending degree order:
//   [1, c1, ..., cn]
// represents
//   lambda^n + c1*lambda^(n-1) + ... + cn.
//
// The implementation uses the Faddeev-LeVerrier recurrence. Every matrix
// multiplication, trace accumulation, diagonal update, exact division, and
// coefficient negation is checked in signed 64-bit arithmetic. If this bounded
// exact arithmetic contract is exceeded, std::overflow_error is thrown rather
// than silently wrapping. A mathematically impossible non-exact recurrence
// division throws std::logic_error.
//
// The empty 0x0 matrix has characteristic polynomial 1.
[[nodiscard]] inline std::vector<std::int64_t>
integer_characteristic_polynomial(
    const std::vector<std::vector<std::int64_t>>& matrix) {
  const std::size_t n = matrix.size();
  for (const auto& row : matrix) {
    if (row.size() != n) {
      throw std::invalid_argument(
          "characteristic polynomial matrix must be square");
    }
  }

  if (n == std::numeric_limits<std::size_t>::max() ||
      n > static_cast<std::size_t>(
              std::numeric_limits<std::int64_t>::max())) {
    throw std::length_error(
        "characteristic polynomial matrix dimension is too large");
  }

  std::vector<std::int64_t> coefficients(n + 1U, 0);
  coefficients[0U] = 1;
  if (n == 0U) {
    return coefficients;
  }

  detail::CharacteristicPolynomialMatrix previous(
      n, std::vector<std::int64_t>(n, 0));
  for (std::size_t index = 0U; index < n; ++index) {
    previous[index][index] = 1;
  }

  for (std::size_t step = 1U; step <= n; ++step) {
    auto product = detail::charpoly_multiply(matrix, previous);

    std::int64_t trace = 0;
    for (std::size_t index = 0U; index < n; ++index) {
      trace = detail::charpoly_checked_add(
          trace, product[index][index]);
    }

    const auto divisor = static_cast<std::int64_t>(step);
    const std::int64_t quotient =
        detail::charpoly_exact_divide(trace, divisor);
    const std::int64_t coefficient =
        detail::charpoly_checked_negate(quotient);
    coefficients[step] = coefficient;

    if (step == n) {
      break;
    }

    for (std::size_t index = 0U; index < n; ++index) {
      product[index][index] = detail::charpoly_checked_add(
          product[index][index], coefficient);
    }
    previous = std::move(product);
  }

  return coefficients;
}

}  // namespace algorithms::linear_algebra
