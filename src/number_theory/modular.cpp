#include "algorithms/number_theory/modular.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace algorithms::number_theory {
namespace {

std::uint64_t add_mod(std::uint64_t a, std::uint64_t b,
                      std::uint64_t modulus) noexcept {
  // Preconditions: modulus > 0 and a,b < modulus. This avoids a+b overflow.
  if (a >= modulus - b) {
    return a - (modulus - b);
  }
  return a + b;
}

bool is_composite_witness(std::uint64_t base, std::uint64_t odd_part,
                          unsigned int powers_of_two, std::uint64_t value) {
  base %= value;
  if (base == 0U) {
    return false;
  }

  std::uint64_t x = power_mod(base, odd_part, value);
  if (x == 1U || x == value - 1U) {
    return false;
  }

  for (unsigned int round = 1U; round < powers_of_two; ++round) {
    x = multiply_mod(x, x, value);
    if (x == value - 1U) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::uint64_t gcd(std::uint64_t a, std::uint64_t b) noexcept {
  while (b != 0U) {
    const std::uint64_t remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

std::uint64_t multiply_mod(std::uint64_t a, std::uint64_t b,
                           std::uint64_t modulus) {
  if (modulus == 0U) {
    throw std::invalid_argument("modulus must be non-zero");
  }

  a %= modulus;
  b %= modulus;
  std::uint64_t result = 0U;
  while (b != 0U) {
    if ((b & 1U) != 0U) {
      result = add_mod(result, a, modulus);
    }
    b >>= 1U;
    if (b != 0U) {
      a = add_mod(a, a, modulus);
    }
  }
  return result;
}

std::uint64_t power_mod(std::uint64_t base, std::uint64_t exponent,
                        std::uint64_t modulus) {
  if (modulus == 0U) {
    throw std::invalid_argument("modulus must be non-zero");
  }
  if (modulus == 1U) {
    return 0U;
  }

  std::uint64_t result = 1U;
  base %= modulus;
  while (exponent != 0U) {
    if ((exponent & 1U) != 0U) {
      result = multiply_mod(result, base, modulus);
    }
    exponent >>= 1U;
    if (exponent != 0U) {
      base = multiply_mod(base, base, modulus);
    }
  }
  return result;
}

bool is_prime(std::uint64_t value) {
  if (value < 2U) {
    return false;
  }

  constexpr std::array<std::uint64_t, 12> small_primes{
      2U, 3U, 5U, 7U, 11U, 13U, 17U, 19U, 23U, 29U, 31U, 37U};
  for (const std::uint64_t prime : small_primes) {
    if (value % prime == 0U) {
      return value == prime;
    }
  }

  std::uint64_t odd_part = value - 1U;
  unsigned int powers_of_two = 0U;
  while ((odd_part & 1U) == 0U) {
    odd_part >>= 1U;
    ++powers_of_two;
  }

  constexpr std::array<std::uint64_t, 7> witnesses{
      2U, 325U, 9375U, 28178U, 450775U, 9780504U, 1795265022U};
  for (const std::uint64_t witness : witnesses) {
    if (is_composite_witness(witness, odd_part, powers_of_two, value)) {
      return false;
    }
  }
  return true;
}

}  // namespace algorithms::number_theory
