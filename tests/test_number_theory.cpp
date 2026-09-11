#include "algorithms/number_theory/discrete_logarithm.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>

using algorithms::number_theory::baby_step_giant_step_discrete_log;
using algorithms::number_theory::gcd;
using algorithms::number_theory::is_prime;
using algorithms::number_theory::multiply_mod;
using algorithms::number_theory::power_mod;

namespace {

bool trial_prime(std::uint64_t value) {
  if (value < 2U) {
    return false;
  }
  if ((value & 1U) == 0U) {
    return value == 2U;
  }
  for (std::uint64_t divisor = 3U; divisor <= value / divisor;
       divisor += 2U) {
    if (value % divisor == 0U) {
      return false;
    }
  }
  return true;
}

std::uint64_t small_power_mod(std::uint64_t base, std::uint64_t exponent,
                              std::uint64_t modulus) {
  if (modulus == 0U) {
    throw std::invalid_argument("zero modulus");
  }
  if (modulus == 1U) {
    return 0U;
  }
  std::uint64_t result = 1U;
  base %= modulus;
  while (exponent != 0U) {
    if ((exponent & 1U) != 0U) {
      result = (result * base) % modulus;
    }
    exponent >>= 1U;
    if (exponent != 0U) {
      base = (base * base) % modulus;
    }
  }
  return result;
}

std::optional<std::uint64_t> brute_discrete_log(std::uint64_t base,
                                                std::uint64_t target,
                                                std::uint64_t prime) {
  base %= prime;
  target %= prime;
  std::uint64_t value = 1U;
  for (std::uint64_t exponent = 0U; exponent < prime - 1U; ++exponent) {
    if (value == target) {
      return exponent;
    }
    value = multiply_mod(value, base, prime);
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE(number_theory_gcd_and_modular_arithmetic_boundaries) {
  REQUIRE_EQ(gcd(0U, 0U), 0U);
  REQUIRE_EQ(gcd(0U, 42U), 42U);
  REQUIRE_EQ(gcd(48U, 18U), 6U);
  REQUIRE_EQ(multiply_mod(0U, 123U, 7U), 0U);
  REQUIRE_EQ(multiply_mod(123U, 456U, 1U), 0U);
  REQUIRE_EQ(power_mod(123U, 0U, 97U), 1U);
  REQUIRE_EQ(power_mod(123U, 456U, 1U), 0U);
  REQUIRE_THROWS_AS(multiply_mod(1U, 2U, 0U), std::invalid_argument);
  REQUIRE_THROWS_AS(power_mod(2U, 10U, 0U), std::invalid_argument);

  REQUIRE_EQ(multiply_mod(18'446'744'073'709'551'614ULL,
                          18'446'744'073'709'551'613ULL,
                          18'446'744'073'709'551'557ULL),
             3192ULL);
  REQUIRE_EQ(power_mod(18'446'744'073'709'551'614ULL, 1'234'567ULL,
                       18'446'744'073'709'551'557ULL),
             7'652'480'996'883'560'114ULL);
}

TEST_CASE(number_theory_primality_known_values_and_pseudoprimes) {
  REQUIRE(!is_prime(0U));
  REQUIRE(!is_prime(1U));
  REQUIRE(is_prime(2U));
  REQUIRE(is_prime(3U));
  REQUIRE(!is_prime(4U));
  REQUIRE(is_prime(37U));
  REQUIRE(!is_prime(341U));
  REQUIRE(!is_prime(561U));
  REQUIRE(!is_prime(1105U));
  REQUIRE(!is_prime(1729U));
  REQUIRE(!is_prime(3'215'031'751ULL));
  REQUIRE(is_prime(18'446'744'073'709'551'557ULL));
  REQUIRE(!is_prime(18'446'744'073'709'551'615ULL));
}

TEST_CASE(number_theory_randomized_against_independent_small_oracles) {
  std::mt19937_64 rng(0xA11CEBEEFULL);
  for (std::size_t trial = 0; trial < 5000; ++trial) {
    const std::uint64_t a = rng();
    const std::uint64_t b = rng();
    REQUIRE_EQ(gcd(a, b), std::gcd(a, b));
  }

  for (std::size_t trial = 0; trial < 5000; ++trial) {
    const std::uint64_t modulus = 1U + (rng() % 1'000'000U);
    const std::uint64_t a = rng() % modulus;
    const std::uint64_t b = rng() % modulus;
    REQUIRE_EQ(multiply_mod(a, b, modulus), (a * b) % modulus);
    const std::uint64_t exponent = rng() % 1000U;
    REQUIRE_EQ(power_mod(a, exponent, modulus),
               small_power_mod(a, exponent, modulus));
  }

  for (std::size_t trial = 0; trial < 20'000; ++trial) {
    const std::uint64_t value = rng() % 1'000'000U;
    REQUIRE_EQ(is_prime(value), trial_prime(value));
  }
}

TEST_CASE(number_theory_discrete_log_validation_and_group_boundaries) {
  REQUIRE_THROWS_AS(baby_step_giant_step_discrete_log(2U, 1U, 8U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(baby_step_giant_step_discrete_log(0U, 1U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(baby_step_giant_step_discrete_log(2U, 0U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(baby_step_giant_step_discrete_log(2U, 1U, 7U, 0U),
                    std::invalid_argument);
  REQUIRE_EQ(baby_step_giant_step_discrete_log(1U, 1U, 2U),
             std::optional<std::uint64_t>{0U});
}

TEST_CASE(number_theory_discrete_log_least_exponent_and_no_solution) {
  REQUIRE_EQ(baby_step_giant_step_discrete_log(2U, 1U, 7U),
             std::optional<std::uint64_t>{0U});
  REQUIRE_EQ(baby_step_giant_step_discrete_log(2U, 4U, 7U),
             std::optional<std::uint64_t>{2U});
  REQUIRE_EQ(baby_step_giant_step_discrete_log(2U, 3U, 7U), std::nullopt);
  REQUIRE_EQ(baby_step_giant_step_discrete_log(4U, 2U, 7U),
             std::optional<std::uint64_t>{2U});
}

TEST_CASE(number_theory_discrete_log_exhaustive_small_prime_differential) {
  constexpr std::array<std::uint64_t, 11> primes{
      2U, 3U, 5U, 7U, 11U, 13U, 17U, 19U, 23U, 29U, 31U};
  for (const std::uint64_t prime : primes) {
    for (std::uint64_t base = 1U; base < prime; ++base) {
      for (std::uint64_t target = 1U; target < prime; ++target) {
        REQUIRE_EQ(baby_step_giant_step_discrete_log(base, target, prime, 16U),
                   brute_discrete_log(base, target, prime));
      }
    }
  }
}

TEST_CASE(number_theory_discrete_log_large_prime_and_resource_budget) {
  constexpr std::uint64_t prime = 998244353U;
  constexpr std::uint64_t base = 3U;
  constexpr std::uint64_t exponent = 1'234'567U;
  const std::uint64_t target = power_mod(base, exponent, prime);
  REQUIRE_EQ(baby_step_giant_step_discrete_log(base, target, prime, 40'000U),
             std::optional<std::uint64_t>{exponent});
  REQUIRE_THROWS_AS(
      baby_step_giant_step_discrete_log(base, target, prime, 100U),
      std::length_error);
}
