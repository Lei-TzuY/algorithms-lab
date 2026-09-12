#pragma once

#include "algorithms/linear_algebra/finite_field_gaussian_elimination.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "algorithms/number_theory/polynomial_interpolation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct ReedSolomonDecodeResult {
  std::vector<std::uint64_t> message_coefficients;
  std::vector<std::uint64_t> corrected_codeword;
  std::vector<std::size_t> error_positions;
};

namespace {

std::uint64_t subtract_mod(std::uint64_t lhs, std::uint64_t rhs,
                           std::uint64_t modulus) noexcept {
  return lhs >= rhs ? lhs - rhs : modulus - (rhs - lhs);
}

std::uint64_t negate_mod(std::uint64_t value,
                         std::uint64_t modulus) noexcept {
  return value == 0U ? 0U : modulus - value;
}

std::vector<std::uint64_t> normalized_distinct_points(
    std::span<const std::uint64_t> points, std::uint64_t prime_modulus) {
  if (!number_theory::is_prime(prime_modulus)) {
    throw std::invalid_argument("Reed-Solomon modulus must be prime");
  }
  if (static_cast<std::uint64_t>(points.size()) > prime_modulus) {
    throw std::invalid_argument("too many Reed-Solomon evaluation points");
  }
  std::vector<std::uint64_t> normalized(points.begin(), points.end());
  for (auto& point : normalized) {
    point %= prime_modulus;
  }
  auto sorted = normalized;
  std::sort(sorted.begin(), sorted.end());
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
    throw std::invalid_argument(
        "Reed-Solomon evaluation points must be distinct modulo p");
  }
  return normalized;
}

std::optional<std::vector<std::uint64_t>> divide_by_monic_exact(
    std::vector<std::uint64_t> numerator,
    std::span<const std::uint64_t> monic_divisor, std::size_t quotient_size,
    std::uint64_t modulus) {
  if (monic_divisor.empty() || monic_divisor.back() != 1U) {
    throw std::logic_error("Reed-Solomon error locator must be monic");
  }
  const std::size_t divisor_degree = monic_divisor.size() - 1U;
  if (numerator.size() != quotient_size + divisor_degree) {
    throw std::logic_error("Reed-Solomon polynomial division shape mismatch");
  }

  std::vector<std::uint64_t> quotient(quotient_size, 0U);
  for (std::size_t degree = numerator.size(); degree-- > divisor_degree;) {
    const std::size_t quotient_degree = degree - divisor_degree;
    const std::uint64_t factor = numerator[degree];
    quotient[quotient_degree] = factor;
    if (factor == 0U) {
      continue;
    }
    for (std::size_t j = 0; j < monic_divisor.size(); ++j) {
      const std::size_t target = quotient_degree + j;
      const std::uint64_t product = number_theory::multiply_mod(
          factor, monic_divisor[j], modulus);
      numerator[target] = subtract_mod(numerator[target], product, modulus);
    }
  }
  if (std::any_of(numerator.begin(), numerator.end(),
                  [](std::uint64_t value) { return value != 0U; })) {
    return std::nullopt;
  }
  return quotient;
}

}  // namespace

// Evaluation-form Reed-Solomon encoding over F_p. Message coefficients are
// low-degree first; the message dimension and code length must both be positive,
// n >= k, and evaluation points must be distinct modulo p.
[[nodiscard]] inline std::vector<std::uint64_t> reed_solomon_encode(
    std::span<const std::uint64_t> message_coefficients,
    std::span<const std::uint64_t> evaluation_points,
    std::uint64_t prime_modulus) {
  if (message_coefficients.empty()) {
    throw std::invalid_argument("Reed-Solomon message dimension must be positive");
  }
  if (evaluation_points.size() < message_coefficients.size()) {
    throw std::invalid_argument("Reed-Solomon code length must be at least k");
  }
  const auto points =
      normalized_distinct_points(evaluation_points, prime_modulus);
  std::vector<std::uint64_t> codeword;
  codeword.reserve(points.size());
  for (const std::uint64_t point : points) {
    codeword.push_back(number_theory::evaluate_polynomial_mod(
        message_coefficients, point, prime_modulus));
  }
  return codeword;
}

// Berlekamp-Welch unique decoding. Requires k > 0 and n >= k + 2t.
// Returns the unique codeword within Hamming radius t when one exists; otherwise
// returns nullopt. No claim is made that every corruption affecting >t symbols
// is detectable, because such a word may lie within radius t of another codeword.
[[nodiscard]] inline std::optional<ReedSolomonDecodeResult> reed_solomon_decode(
    std::span<const std::uint64_t> received,
    std::span<const std::uint64_t> evaluation_points,
    std::size_t message_length, std::size_t max_errors,
    std::uint64_t prime_modulus) {
  if (message_length == 0U) {
    throw std::invalid_argument("Reed-Solomon message dimension must be positive");
  }
  if (received.size() != evaluation_points.size()) {
    throw std::invalid_argument("Reed-Solomon received/point sizes differ");
  }
  const std::size_t n = received.size();
  if (message_length > n || max_errors > (n - message_length) / 2U) {
    throw std::invalid_argument(
        "Reed-Solomon requires n >= k + 2*max_errors");
  }
  const auto points =
      normalized_distinct_points(evaluation_points, prime_modulus);

  const std::size_t q_count = message_length + max_errors;
  const std::size_t variable_count = q_count + max_errors;
  std::vector<std::vector<std::uint64_t>> coefficients(
      n, std::vector<std::uint64_t>(variable_count, 0U));
  std::vector<std::uint64_t> rhs(n, 0U);

  for (std::size_t row = 0; row < n; ++row) {
    const std::uint64_t x = points[row];
    const std::uint64_t y = received[row] % prime_modulus;

    std::uint64_t power = 1U;
    for (std::size_t degree = 0; degree < q_count; ++degree) {
      coefficients[row][degree] = power;
      power = number_theory::multiply_mod(power, x, prime_modulus);
    }

    power = 1U;
    for (std::size_t degree = 0; degree < max_errors; ++degree) {
      const std::uint64_t scaled =
          number_theory::multiply_mod(y, power, prime_modulus);
      coefficients[row][q_count + degree] =
          negate_mod(scaled, prime_modulus);
      power = number_theory::multiply_mod(power, x, prime_modulus);
    }
    rhs[row] = number_theory::multiply_mod(y, power, prime_modulus);
  }

  const auto system = linear_algebra::solve_linear_system_mod_prime(
      variable_count, coefficients, rhs, prime_modulus);
  if (!system.consistent) {
    return std::nullopt;
  }

  std::vector<std::uint64_t> q(q_count, 0U);
  std::copy_n(system.particular_solution.begin(), q_count, q.begin());
  std::vector<std::uint64_t> locator(max_errors + 1U, 0U);
  for (std::size_t degree = 0; degree < max_errors; ++degree) {
    locator[degree] = system.particular_solution[q_count + degree];
  }
  locator[max_errors] = 1U;

  auto message = divide_by_monic_exact(q, locator, message_length, prime_modulus);
  if (!message.has_value()) {
    return std::nullopt;
  }
  const auto corrected = reed_solomon_encode(*message, points, prime_modulus);
  std::vector<std::size_t> error_positions;
  for (std::size_t index = 0; index < n; ++index) {
    if (corrected[index] != received[index] % prime_modulus) {
      error_positions.push_back(index);
    }
  }
  if (error_positions.size() > max_errors) {
    return std::nullopt;
  }
  return ReedSolomonDecodeResult{std::move(*message), corrected,
                                 std::move(error_positions)};
}

}  // namespace algorithms::coding
