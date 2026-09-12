#include "algorithms/number_theory/polynomial_interpolation.hpp"

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::number_theory {
namespace {

std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                      std::uint64_t modulus) noexcept {
  if (a >= modulus - b) {
    return a - (modulus - b);
  }
  return a + b;
}

std::uint64_t sub_mod(std::uint64_t a, std::uint64_t b,
                      std::uint64_t modulus) noexcept {
  if (a >= b) {
    return a - b;
  }
  return modulus - (b - a);
}

void validate_prime(std::uint64_t modulus) {
  if (!is_prime(modulus)) {
    throw std::invalid_argument("interpolation modulus must be prime");
  }
}

}  // namespace

std::uint64_t evaluate_polynomial_mod(
    std::span<const std::uint64_t> coefficients, std::uint64_t x,
    std::uint64_t prime_modulus) {
  validate_prime(prime_modulus);
  x %= prime_modulus;

  std::uint64_t result = 0U;
  for (auto it = coefficients.rbegin(); it != coefficients.rend(); ++it) {
    result = add_mod(multiply_mod(result, x, prime_modulus),
                     *it % prime_modulus, prime_modulus);
  }
  return result;
}

std::vector<std::uint64_t> interpolate_prime_field(
    std::span<const FieldSample> samples, std::uint64_t prime_modulus) {
  validate_prime(prime_modulus);
  if (samples.empty()) {
    return {};
  }
  if (samples.size() > prime_modulus) {
    throw std::invalid_argument(
        "too many samples for distinct field x coordinates");
  }

  std::vector<std::uint64_t> xs(samples.size());
  std::vector<std::uint64_t> divided(samples.size());
  for (std::size_t index = 0; index < samples.size(); ++index) {
    xs[index] = samples[index].x % prime_modulus;
    divided[index] = samples[index].y % prime_modulus;
  }

  std::vector<std::uint64_t> sorted_xs = xs;
  std::sort(sorted_xs.begin(), sorted_xs.end());
  if (std::adjacent_find(sorted_xs.begin(), sorted_xs.end()) !=
      sorted_xs.end()) {
    throw std::invalid_argument(
        "sample x coordinates must be distinct modulo the prime field");
  }

  // In-place Newton divided differences. After order d, divided[i] is the
  // coefficient of the degree-d Newton basis for samples i-d..i.
  for (std::size_t order = 1; order < samples.size(); ++order) {
    for (std::size_t index = samples.size(); index-- > order;) {
      const std::uint64_t numerator =
          sub_mod(divided[index], divided[index - 1U], prime_modulus);
      const std::uint64_t denominator =
          sub_mod(xs[index], xs[index - order], prime_modulus);
      const std::uint64_t inverse =
          power_mod(denominator, prime_modulus - 2U, prime_modulus);
      divided[index] = multiply_mod(numerator, inverse, prime_modulus);
    }
  }

  // Horner-like conversion from Newton basis to ordinary monomial
  // coefficients, low degree first.
  std::vector<std::uint64_t> coefficients{divided.back()};
  for (std::size_t index = samples.size() - 1U; index-- > 0U;) {
    std::vector<std::uint64_t> next(coefficients.size() + 1U, 0U);
    for (std::size_t degree = 0; degree < coefficients.size(); ++degree) {
      next[degree + 1U] =
          add_mod(next[degree + 1U], coefficients[degree], prime_modulus);
      const std::uint64_t scaled =
          multiply_mod(coefficients[degree], xs[index], prime_modulus);
      next[degree] = sub_mod(next[degree], scaled, prime_modulus);
    }
    next[0] = add_mod(next[0], divided[index], prime_modulus);
    coefficients = std::move(next);
  }
  return coefficients;
}

}  // namespace algorithms::number_theory
