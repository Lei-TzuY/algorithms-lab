#include "algorithms/number_theory/factorization.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "algorithms/number_theory/modular.hpp"

namespace algorithms::number_theory {
namespace {

class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

  std::uint64_t next() noexcept {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state_;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
  }

  std::uint64_t bounded(std::uint64_t bound) noexcept {
    if (bound == 0U) {
      return next();
    }
    const std::uint64_t threshold = (0U - bound) % bound;
    for (;;) {
      const std::uint64_t sample = next();
      if (sample >= threshold) {
        return sample % bound;
      }
    }
  }

 private:
  std::uint64_t state_;
};

std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                      std::uint64_t modulus) noexcept {
  if (a >= modulus - b) {
    return a - (modulus - b);
  }
  return a + b;
}

std::uint64_t polynomial_step(std::uint64_t value, std::uint64_t constant,
                              std::uint64_t modulus) {
  return add_mod(multiply_mod(value, value, modulus), constant, modulus);
}

std::uint64_t absolute_difference(std::uint64_t a, std::uint64_t b) noexcept {
  return a >= b ? a - b : b - a;
}

std::uint64_t pollard_rho_factor(std::uint64_t value, SplitMix64& random,
                                 std::size_t& polynomials_tried) {
  if ((value & 1U) == 0U) {
    return 2U;
  }
  if (value % 3U == 0U) {
    return 3U;
  }

  constexpr std::size_t iteration_budget = 1U << 20U;
  for (;;) {
    ++polynomials_tried;
    const std::uint64_t constant = 1U + random.bounded(value - 1U);
    std::uint64_t tortoise = 2U + random.bounded(value - 3U);
    std::uint64_t hare = tortoise;
    std::uint64_t divisor = 1U;

    for (std::size_t iteration = 0;
         iteration < iteration_budget && divisor == 1U; ++iteration) {
      tortoise = polynomial_step(tortoise, constant, value);
      hare = polynomial_step(polynomial_step(hare, constant, value), constant,
                             value);
      divisor = gcd(absolute_difference(tortoise, hare), value);
    }

    if (divisor > 1U && divisor < value) {
      return divisor;
    }
  }
}

void factor_recursive(std::uint64_t value, SplitMix64& random,
                      std::size_t& polynomials_tried,
                      std::vector<std::uint64_t>& factors) {
  if (value == 1U) {
    return;
  }
  if (is_prime(value)) {
    factors.push_back(value);
    return;
  }

  const std::uint64_t divisor =
      pollard_rho_factor(value, random, polynomials_tried);
  factor_recursive(divisor, random, polynomials_tried, factors);
  factor_recursive(value / divisor, random, polynomials_tried, factors);
}

}  // namespace

FactorizationResult factorize_uint64(std::uint64_t value, std::uint64_t seed) {
  if (value == 0U) {
    throw std::invalid_argument("zero has no prime factorization");
  }

  FactorizationResult result;
  result.seed = seed;
  if (value == 1U) {
    return result;
  }

  SplitMix64 random(seed);
  factor_recursive(value, random, result.rho_polynomials_tried,
                   result.prime_factors);
  std::sort(result.prime_factors.begin(), result.prime_factors.end());
  return result;
}

}  // namespace algorithms::number_theory
