#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/number_theory/factorization.hpp"
#include "algorithms/number_theory/modular.hpp"

namespace {
using algorithms::number_theory::FactorizationResult;
using algorithms::number_theory::factorize_uint64;
using algorithms::number_theory::is_prime;

std::vector<std::uint64_t> trial_factorization(std::uint64_t value) {
  std::vector<std::uint64_t> factors;
  for (std::uint64_t divisor = 2U; divisor <= value / divisor; ++divisor) {
    while (value % divisor == 0U) {
      factors.push_back(divisor);
      value /= divisor;
    }
  }
  if (value > 1U) {
    factors.push_back(value);
  }
  return factors;
}

void verify_exact_factorization(std::uint64_t value,
                                const FactorizationResult& result) {
  REQUIRE(std::is_sorted(result.prime_factors.begin(),
                         result.prime_factors.end()));
  std::uint64_t remaining = value;
  for (const std::uint64_t factor : result.prime_factors) {
    REQUIRE(is_prime(factor));
    REQUIRE(factor > 1U);
    REQUIRE_EQ(remaining % factor, 0U);
    remaining /= factor;
  }
  REQUIRE_EQ(remaining, 1U);
}
}  // namespace

TEST_CASE(factorization_trivial_prime_and_power_cases) {
  REQUIRE_THROWS_AS(factorize_uint64(0U), std::invalid_argument);
  REQUIRE(factorize_uint64(1U).prime_factors.empty());
  REQUIRE_EQ(factorize_uint64(2U).prime_factors,
             std::vector<std::uint64_t>{2U});
  REQUIRE_EQ(factorize_uint64(97U).prime_factors,
             std::vector<std::uint64_t>{97U});
  REQUIRE_EQ(factorize_uint64(1ULL << 63U).prime_factors,
             std::vector<std::uint64_t>(63U, 2U));
}

TEST_CASE(factorization_full_width_known_vectors) {
  constexpr std::uint64_t max_value = std::numeric_limits<std::uint64_t>::max();
  const std::vector<std::uint64_t> max_expected{3U, 5U, 17U, 257U, 641U,
                                                65537U, 6700417U};
  REQUIRE_EQ(factorize_uint64(max_value, 7U).prime_factors, max_expected);

  constexpr std::uint64_t semiprime = 18446743979220271189ULL;
  const std::vector<std::uint64_t> semiprime_expected{4294967279ULL,
                                                       4294967291ULL};
  REQUIRE_EQ(factorize_uint64(semiprime, 123U).prime_factors,
             semiprime_expected);

  constexpr std::uint64_t repeated_large_prime = 1000000014000000049ULL;
  REQUIRE_EQ(factorize_uint64(repeated_large_prime, 99U).prime_factors,
             (std::vector<std::uint64_t>{1000000007ULL, 1000000007ULL}));

  constexpr std::uint64_t large_prime = 18446744073709551557ULL;
  REQUIRE_EQ(factorize_uint64(large_prime).prime_factors,
             std::vector<std::uint64_t>{large_prime});
}

TEST_CASE(factorization_seed_replay_and_seed_independent_result) {
  constexpr std::uint64_t value = 1000000016000000063ULL;
  const FactorizationResult first = factorize_uint64(value, 0x12345678ULL);
  const FactorizationResult replay = factorize_uint64(value, 0x12345678ULL);
  const FactorizationResult other_seed = factorize_uint64(value, 0xCAFEBABEULL);
  REQUIRE_EQ(first, replay);
  REQUIRE_EQ(first.prime_factors, other_seed.prime_factors);
  REQUIRE(first.rho_polynomials_tried > 0U);
  REQUIRE(other_seed.rho_polynomials_tried > 0U);
  verify_exact_factorization(value, first);
  verify_exact_factorization(value, other_seed);
}

TEST_CASE(factorization_randomized_differential_against_trial_division) {
  std::mt19937_64 random(0xFAC70A17ULL);
  for (std::size_t trial = 0; trial < 2500U; ++trial) {
    const std::uint64_t value = 1U + (random() % 10000000U);
    const std::uint64_t seed = random();
    const FactorizationResult actual = factorize_uint64(value, seed);
    const std::vector<std::uint64_t> expected = trial_factorization(value);
    REQUIRE_EQ(actual.prime_factors, expected);
    verify_exact_factorization(value, actual);
  }
}
