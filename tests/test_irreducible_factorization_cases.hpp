#pragma once

#include "algorithms/polynomials/irreducible_factorization.hpp"
#include "test_equal_degree_factorization_cases.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace irreducible_factorization_test_detail {
using algorithms::polynomials::IrreduciblePolynomialFactor;
using algorithms::polynomials::PrimeFieldPolynomial;
using distinct_degree_test_detail::irreducibles;
using equal_degree_test_detail::multiply_reconstruction;
using equal_degree_test_detail::scale_reconstruction;

[[nodiscard]] inline PrimeFieldPolynomial power_reconstruction(
    PrimeFieldPolynomial factor, std::uint64_t multiplicity,
    std::uint64_t prime) {
  PrimeFieldPolynomial result{1U};
  while (multiplicity != 0U) {
    if ((multiplicity & 1U) != 0U) {
      result = multiply_reconstruction(result, factor, prime);
    }
    multiplicity >>= 1U;
    if (multiplicity != 0U) {
      factor = multiply_reconstruction(factor, factor, prime);
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial reconstruct(
    const std::vector<IrreduciblePolynomialFactor>& factors,
    std::uint64_t unit, std::uint64_t prime) {
  PrimeFieldPolynomial product{1U};
  for (const auto& entry : factors) {
    product = multiply_reconstruction(
        product, power_reconstruction(entry.factor, entry.multiplicity, prime),
        prime);
  }
  return scale_reconstruction(std::move(product), unit, prime);
}

inline void sort_expected(std::vector<IrreduciblePolynomialFactor>& factors) {
  std::sort(factors.begin(), factors.end(),
            [](const auto& first, const auto& second) {
              if (first.factor != second.factor) {
                return first.factor < second.factor;
              }
              return first.multiplicity < second.multiplicity;
            });
}
}  // namespace irreducible_factorization_test_detail

TEST_CASE(prime_field_irreducible_factorization_validation_constants_and_budget) {
  using namespace irreducible_factorization_test_detail;
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          {}, 5U, 1U, 32U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          {1U, 1U}, 4U, 1U, 32U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          {1U, 5U}, 5U, 1U, 32U),
      std::invalid_argument);

  const auto constant =
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          {3U}, 5U, 17U, 0U);
  REQUIRE(constant.complete);
  REQUIRE_EQ(constant.unit, 3U);
  REQUIRE(constant.factors.empty());
  REQUIRE_EQ(constant.seed, 17U);
  REQUIRE_EQ(constant.trials_executed, 0U);

  const PrimeFieldPolynomial two_linears =
      multiply_reconstruction({4U, 1U}, {3U, 1U}, 5U);
  const auto exhausted =
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          two_linears, 5U, 7U, 0U);
  REQUIRE(!exhausted.complete);
  REQUIRE(exhausted.factors.empty());
  REQUIRE_EQ(exhausted.unit, 1U);
  REQUIRE_EQ(exhausted.seed, 7U);
  REQUIRE_EQ(exhausted.trials_executed, 0U);
}

TEST_CASE(prime_field_irreducible_factorization_multiplicity_and_replay) {
  using namespace irreducible_factorization_test_detail;
  const PrimeFieldPolynomial linear{1U, 1U};
  const PrimeFieldPolynomial quadratic{1U, 1U, 1U};
  PrimeFieldPolynomial input{1U};
  input = multiply_reconstruction(
      input, power_reconstruction(linear, 3U, 2U), 2U);
  input = multiply_reconstruction(
      input, power_reconstruction(quadratic, 4U, 2U), 2U);

  const auto first =
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          input, 2U, 0xFAC70123ULL, 1024U);
  const auto second =
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          input, 2U, 0xFAC70123ULL, 1024U);
  REQUIRE(first.complete);
  REQUIRE_EQ(first.unit, 1U);
  REQUIRE_EQ(first.factors, second.factors);
  REQUIRE_EQ(first.trials_executed, second.trials_executed);
  REQUIRE_EQ(reconstruct(first.factors, first.unit, 2U), input);

  std::vector<IrreduciblePolynomialFactor> expected{
      {linear, 3U}, {quadratic, 4U}};
  sort_expected(expected);
  REQUIRE_EQ(first.factors, expected);
}

TEST_CASE(prime_field_irreducible_factorization_nonmonic_and_full_width_prime) {
  using namespace irreducible_factorization_test_detail;
  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const PrimeFieldPolynomial first{prime - 1U, 1U};
  const PrimeFieldPolynomial second{prime - 2U, 1U};
  PrimeFieldPolynomial monic = multiply_reconstruction(first, second, prime);
  monic = multiply_reconstruction(monic, second, prime);
  const std::uint64_t unit = prime - 7U;
  const PrimeFieldPolynomial input =
      scale_reconstruction(monic, unit, prime);

  const auto result =
      algorithms::polynomials::polynomial_irreducible_factorization_mod(
          input, prime, 0xF011FAC7ULL, 64U);
  REQUIRE(result.complete);
  REQUIRE_EQ(result.unit, unit);
  REQUIRE_EQ(reconstruct(result.factors, result.unit, prime), input);
  std::vector<IrreduciblePolynomialFactor> expected{{first, 1U},
                                                     {second, 2U}};
  sort_expected(expected);
  REQUIRE_EQ(result.factors, expected);
}

TEST_CASE(prime_field_irreducible_factorization_random_independent_oracle) {
  using namespace irreducible_factorization_test_detail;
  std::mt19937_64 rng(0xFACADE1234ULL);
  for (const std::uint64_t prime : {2U, 3U, 5U}) {
    const auto by_degree = irreducibles(prime, 3U);
    std::vector<PrimeFieldPolynomial> pool;
    for (std::size_t degree = 1U; degree < by_degree.size(); ++degree) {
      for (const auto& factor : by_degree[degree]) {
        pool.push_back(factor);
      }
    }
    REQUIRE(pool.size() >= 3U);

    for (std::size_t trial = 0U; trial < 40U; ++trial) {
      std::shuffle(pool.begin(), pool.end(), rng);
      const std::size_t count =
          1U + static_cast<std::size_t>(rng() % std::min<std::size_t>(3U, pool.size()));
      PrimeFieldPolynomial monic{1U};
      std::vector<IrreduciblePolynomialFactor> expected;
      for (std::size_t index = 0U; index < count; ++index) {
        const std::uint64_t multiplicity = 1U + (rng() % 3U);
        expected.push_back(IrreduciblePolynomialFactor{pool[index], multiplicity});
        monic = multiply_reconstruction(
            monic, power_reconstruction(pool[index], multiplicity, prime), prime);
      }
      const std::uint64_t unit = 1U + (rng() % (prime - 1U));
      const PrimeFieldPolynomial input =
          scale_reconstruction(monic, unit, prime);
      sort_expected(expected);

      const std::uint64_t seed = rng();
      const auto result =
          algorithms::polynomials::polynomial_irreducible_factorization_mod(
              input, prime, seed, 2048U);
      REQUIRE(result.complete);
      REQUIRE_EQ(result.unit, unit);
      REQUIRE_EQ(result.seed, seed);
      REQUIRE_EQ(result.factors, expected);
      REQUIRE_EQ(reconstruct(result.factors, result.unit, prime), input);
    }
  }
}
