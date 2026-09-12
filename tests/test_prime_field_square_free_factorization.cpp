#include "algorithms/polynomials/square_free_factorization.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::polynomials::PrimeFieldPolynomial;
using algorithms::polynomials::SquareFreeFactorization;
using algorithms::polynomials::polynomial_derivative_mod;
using algorithms::polynomials::polynomial_square_free_factorization_mod;

namespace {

void trim(PrimeFieldPolynomial& polynomial) {
  while (!polynomial.empty() && polynomial.back() == 0U) polynomial.pop_back();
}

PrimeFieldPolynomial small_multiply(const PrimeFieldPolynomial& first,
                                    const PrimeFieldPolynomial& second,
                                    std::uint64_t prime) {
  if (first.empty() || second.empty()) return {};
  PrimeFieldPolynomial result(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0; left < first.size(); ++left) {
    for (std::size_t right = 0; right < second.size(); ++right) {
      result[left + right] =
          (result[left + right] + (first[left] * second[right]) % prime) % prime;
    }
  }
  trim(result);
  return result;
}

PrimeFieldPolynomial small_scale(PrimeFieldPolynomial polynomial,
                                 std::uint64_t scalar,
                                 std::uint64_t prime) {
  for (std::uint64_t& coefficient : polynomial) {
    coefficient = (coefficient * scalar) % prime;
  }
  trim(polynomial);
  return polynomial;
}

PrimeFieldPolynomial small_power(PrimeFieldPolynomial base,
                                 std::uint64_t exponent,
                                 std::uint64_t prime) {
  PrimeFieldPolynomial result{1U};
  while (exponent != 0U) {
    if ((exponent & 1U) != 0U) result = small_multiply(result, base, prime);
    exponent >>= 1U;
    if (exponent != 0U) base = small_multiply(base, base, prime);
  }
  return result;
}

PrimeFieldPolynomial reconstruct(const SquareFreeFactorization& decomposition,
                                 std::uint64_t prime) {
  PrimeFieldPolynomial result{decomposition.unit};
  for (const auto& factor : decomposition.factors) {
    result = small_multiply(
        result, small_power(factor.factor, factor.multiplicity, prime), prime);
  }
  return result;
}

bool monic_divides_naive(PrimeFieldPolynomial divisor,
                         PrimeFieldPolynomial dividend,
                         std::uint64_t prime) {
  trim(divisor);
  trim(dividend);
  if (divisor.empty()) return false;
  if (dividend.empty()) return true;
  if (divisor.back() != 1U || divisor.size() > dividend.size()) return false;
  while (!dividend.empty() && dividend.size() >= divisor.size()) {
    const std::size_t shift = dividend.size() - divisor.size();
    const std::uint64_t leading = dividend.back();
    for (std::size_t index = 0; index < divisor.size(); ++index) {
      const std::uint64_t term = (leading * divisor[index]) % prime;
      dividend[shift + index] = dividend[shift + index] >= term
                                    ? dividend[shift + index] - term
                                    : prime - (term - dividend[shift + index]);
    }
    trim(dividend);
  }
  return dividend.empty();
}

std::vector<PrimeFieldPolynomial> enumerate_monic_nonconstant(
    std::size_t maximum_degree, std::uint64_t prime) {
  std::vector<PrimeFieldPolynomial> result;
  for (std::size_t degree = 1U; degree <= maximum_degree; ++degree) {
    std::uint64_t count = 1U;
    for (std::size_t index = 0; index < degree; ++index) count *= prime;
    for (std::uint64_t code = 0U; code < count; ++code) {
      std::uint64_t cursor = code;
      PrimeFieldPolynomial candidate(degree + 1U, 0U);
      candidate[degree] = 1U;
      for (std::size_t index = 0; index < degree; ++index) {
        candidate[index] = cursor % prime;
        cursor /= prime;
      }
      result.push_back(std::move(candidate));
    }
  }
  return result;
}

bool brute_square_free(const PrimeFieldPolynomial& polynomial,
                       std::uint64_t prime) {
  if (polynomial.size() <= 1U) return true;
  const auto candidates =
      enumerate_monic_nonconstant((polynomial.size() - 1U) / 2U, prime);
  for (const auto& candidate : candidates) {
    if (monic_divides_naive(small_multiply(candidate, candidate, prime),
                            polynomial, prime)) {
      return false;
    }
  }
  return true;
}

bool brute_coprime(const PrimeFieldPolynomial& first,
                   const PrimeFieldPolynomial& second,
                   std::uint64_t prime) {
  if (first.size() <= 1U || second.size() <= 1U) return true;
  const auto candidates = enumerate_monic_nonconstant(
      std::min(first.size(), second.size()) - 1U, prime);
  for (const auto& candidate : candidates) {
    if (monic_divides_naive(candidate, first, prime) &&
        monic_divides_naive(candidate, second, prime)) {
      return false;
    }
  }
  return true;
}

void require_valid_decomposition(const PrimeFieldPolynomial& input,
                                 std::uint64_t prime) {
  const auto decomposition =
      polynomial_square_free_factorization_mod(input, prime);
  REQUIRE(decomposition.unit > 0U);
  REQUIRE(decomposition.unit < prime);
  REQUIRE_EQ(reconstruct(decomposition, prime), input);

  std::uint64_t previous_multiplicity = 0U;
  for (std::size_t index = 0; index < decomposition.factors.size(); ++index) {
    const auto& factor = decomposition.factors[index];
    REQUIRE(factor.multiplicity > previous_multiplicity);
    previous_multiplicity = factor.multiplicity;
    REQUIRE(factor.factor.size() > 1U);
    REQUIRE_EQ(factor.factor.back(), 1U);
    REQUIRE(brute_square_free(factor.factor, prime));
    for (std::size_t earlier = 0; earlier < index; ++earlier) {
      REQUIRE(brute_coprime(factor.factor,
                            decomposition.factors[earlier].factor, prime));
    }
  }
}

PrimeFieldPolynomial random_nonzero_polynomial(std::mt19937_64& rng,
                                               std::uint64_t prime,
                                               std::size_t maximum_size) {
  const std::size_t size =
      static_cast<std::size_t>(rng() % (maximum_size + 1U));
  PrimeFieldPolynomial result(size, 0U);
  for (std::uint64_t& coefficient : result) coefficient = rng() % prime;
  trim(result);
  if (result.empty()) result = {1U};
  return result;
}

}  // namespace

TEST_CASE(prime_field_polynomial_derivative_and_validation) {
  REQUIRE_EQ(polynomial_derivative_mod({1U, 2U, 3U, 4U}, 5U),
             (PrimeFieldPolynomial{2U, 1U, 2U}));
  REQUIRE(polynomial_derivative_mod({1U, 0U, 1U}, 2U).empty());
  REQUIRE_THROWS_AS(polynomial_square_free_factorization_mod({}, 5U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(polynomial_square_free_factorization_mod({1U}, 9U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(polynomial_square_free_factorization_mod({5U}, 5U),
                    std::invalid_argument);
}

TEST_CASE(prime_field_square_free_known_mixed_multiplicities) {
  constexpr std::uint64_t prime = 5U;
  const auto polynomial = small_scale(
      small_multiply(small_power({1U, 1U}, 2U, prime),
                     small_power({2U, 1U}, 3U, prime), prime),
      3U, prime);
  const auto decomposition =
      polynomial_square_free_factorization_mod(polynomial, prime);
  REQUIRE_EQ(decomposition.unit, 3U);
  REQUIRE_EQ(decomposition.factors.size(), 2U);
  REQUIRE_EQ(decomposition.factors[0U].multiplicity, 2U);
  REQUIRE_EQ(decomposition.factors[0U].factor,
             (PrimeFieldPolynomial{1U, 1U}));
  REQUIRE_EQ(decomposition.factors[1U].multiplicity, 3U);
  REQUIRE_EQ(decomposition.factors[1U].factor,
             (PrimeFieldPolynomial{2U, 1U}));
  require_valid_decomposition(polynomial, prime);
}

TEST_CASE(prime_field_square_free_characteristic_p_recursion) {
  const auto fourth_power = small_power({1U, 1U}, 4U, 2U);
  const auto binary = polynomial_square_free_factorization_mod(fourth_power, 2U);
  REQUIRE_EQ(binary.factors.size(), 1U);
  REQUIRE_EQ(binary.factors[0U].multiplicity, 4U);
  REQUIRE_EQ(binary.factors[0U].factor, (PrimeFieldPolynomial{1U, 1U}));

  const auto ternary = small_multiply(small_power({1U, 1U}, 3U, 3U),
                                      small_power({2U, 1U}, 6U, 3U), 3U);
  const auto decomposition =
      polynomial_square_free_factorization_mod(ternary, 3U);
  REQUIRE_EQ(decomposition.factors.size(), 2U);
  REQUIRE_EQ(decomposition.factors[0U].multiplicity, 3U);
  REQUIRE_EQ(decomposition.factors[1U].multiplicity, 6U);
  require_valid_decomposition(ternary, 3U);
}

TEST_CASE(prime_field_square_free_constant_and_random_exhaustive_properties) {
  const auto constant = polynomial_square_free_factorization_mod({4U}, 5U);
  REQUIRE_EQ(constant.unit, 4U);
  REQUIRE(constant.factors.empty());

  std::mt19937_64 rng(0x5AFA11ULL);
  constexpr std::uint64_t prime = 3U;
  for (std::size_t trial = 0; trial < 500U; ++trial) {
    require_valid_decomposition(random_nonzero_polynomial(rng, prime, 8U),
                                prime);
  }
}
