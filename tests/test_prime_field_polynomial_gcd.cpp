#include "algorithms/polynomials/prime_field_gcd.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::polynomials::PrimeFieldPolynomial;
using algorithms::polynomials::polynomial_divide_mod;
using algorithms::polynomials::polynomial_extended_gcd_mod;
using algorithms::polynomials::polynomial_gcd_mod;

namespace {

void trim(PrimeFieldPolynomial& value) {
  while (!value.empty() && value.back() == 0U) value.pop_back();
}

PrimeFieldPolynomial small_add(const PrimeFieldPolynomial& first,
                               const PrimeFieldPolynomial& second,
                               std::uint64_t prime) {
  PrimeFieldPolynomial result(std::max(first.size(), second.size()), 0U);
  for (std::size_t index = 0; index < result.size(); ++index) {
    const std::uint64_t left = index < first.size() ? first[index] : 0U;
    const std::uint64_t right = index < second.size() ? second[index] : 0U;
    result[index] = (left + right) % prime;
  }
  trim(result);
  return result;
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

PrimeFieldPolynomial monic(PrimeFieldPolynomial value, std::uint64_t prime) {
  trim(value);
  if (value.empty()) return {};
  std::uint64_t inverse = 0U;
  for (std::uint64_t candidate = 1U; candidate < prime; ++candidate) {
    if ((value.back() * candidate) % prime == 1U) {
      inverse = candidate;
      break;
    }
  }
  REQUIRE(inverse != 0U);
  for (std::uint64_t& coefficient : value) {
    coefficient = (coefficient * inverse) % prime;
  }
  trim(value);
  return value;
}

bool brute_divides(const PrimeFieldPolynomial& divisor,
                   const PrimeFieldPolynomial& dividend,
                   std::uint64_t prime) {
  if (divisor.empty()) return false;
  if (dividend.empty()) return true;
  if (divisor.size() > dividend.size()) return false;
  const std::size_t quotient_size = dividend.size() - divisor.size() + 1U;
  std::uint64_t state_count = 1U;
  for (std::size_t index = 0; index < quotient_size; ++index) {
    state_count *= prime;
  }
  for (std::uint64_t code = 0U; code < state_count; ++code) {
    std::uint64_t cursor = code;
    PrimeFieldPolynomial quotient(quotient_size, 0U);
    for (std::size_t index = 0; index < quotient_size; ++index) {
      quotient[index] = cursor % prime;
      cursor /= prime;
    }
    trim(quotient);
    if (small_multiply(divisor, quotient, prime) == dividend) return true;
  }
  return false;
}

PrimeFieldPolynomial brute_gcd(const PrimeFieldPolynomial& first,
                               const PrimeFieldPolynomial& second,
                               std::uint64_t prime) {
  if (first.empty()) return monic(second, prime);
  if (second.empty()) return monic(first, prime);
  const std::size_t maximum_degree = std::min(first.size(), second.size()) - 1U;
  for (std::size_t degree_plus_one = maximum_degree + 1U;
       degree_plus_one != 0U; --degree_plus_one) {
    const std::size_t degree = degree_plus_one - 1U;
    std::uint64_t candidate_count = 1U;
    for (std::size_t index = 0; index < degree; ++index) candidate_count *= prime;
    for (std::uint64_t code = 0U; code < candidate_count; ++code) {
      std::uint64_t cursor = code;
      PrimeFieldPolynomial candidate(degree + 1U, 0U);
      candidate[degree] = 1U;
      for (std::size_t index = 0; index < degree; ++index) {
        candidate[index] = cursor % prime;
        cursor /= prime;
      }
      if (brute_divides(candidate, first, prime) &&
          brute_divides(candidate, second, prime)) {
        return candidate;
      }
    }
  }
  throw std::logic_error("constant one must divide both polynomials");
}

PrimeFieldPolynomial random_polynomial(std::mt19937_64& rng,
                                       std::uint64_t prime,
                                       std::size_t maximum_size) {
  const std::size_t size =
      static_cast<std::size_t>(rng() % (maximum_size + 1U));
  PrimeFieldPolynomial result(size, 0U);
  for (std::uint64_t& coefficient : result) coefficient = rng() % prime;
  trim(result);
  return result;
}

}  // namespace

TEST_CASE(prime_field_polynomial_validation_and_known_division) {
  REQUIRE_EQ(polynomial_gcd_mod({}, {}, 5U), PrimeFieldPolynomial{});
  REQUIRE_EQ(polynomial_gcd_mod({4U, 1U, 0U}, {}, 5U),
             (PrimeFieldPolynomial{4U, 1U}));
  REQUIRE_THROWS_AS(polynomial_gcd_mod({1U}, {1U}, 9U), std::invalid_argument);
  REQUIRE_THROWS_AS(polynomial_gcd_mod({5U}, {1U}, 5U), std::invalid_argument);
  REQUIRE_THROWS_AS(polynomial_divide_mod({1U}, {}, 5U), std::invalid_argument);

  const auto division = polynomial_divide_mod({4U, 0U, 1U}, {4U, 1U}, 5U);
  REQUIRE_EQ(division.quotient, (PrimeFieldPolynomial{1U, 1U}));
  REQUIRE(division.remainder.empty());
}

TEST_CASE(prime_field_polynomial_extended_gcd_and_full_width_prime) {
  const auto extended =
      polynomial_extended_gcd_mod({3U, 1U, 1U}, {2U, 2U, 1U}, 5U);
  REQUIRE_EQ(extended.gcd, (PrimeFieldPolynomial{4U, 1U}));
  REQUIRE_EQ(small_add(small_multiply(extended.bezout_first, {3U, 1U, 1U}, 5U),
                       small_multiply(extended.bezout_second, {2U, 2U, 1U}, 5U),
                       5U),
             extended.gcd);

  constexpr std::uint64_t prime = 18'446'744'073'709'551'557ULL;
  const PrimeFieldPolynomial first{prime - 2U, 1U, 1U};
  const PrimeFieldPolynomial second{prime - 3U, 2U, 1U};
  const auto gcd = polynomial_gcd_mod(first, second, prime);
  REQUIRE_EQ(gcd, (PrimeFieldPolynomial{prime - 1U, 1U}));
  REQUIRE(polynomial_divide_mod(first, gcd, prime).remainder.empty());
  REQUIRE(polynomial_divide_mod(second, gcd, prime).remainder.empty());
}

TEST_CASE(prime_field_polynomial_random_division_reconstruction) {
  std::mt19937_64 rng(0xD1A1DEULL);
  constexpr std::uint64_t prime = 5U;
  for (std::size_t trial = 0; trial < 300U; ++trial) {
    auto divisor = random_polynomial(rng, prime, 4U);
    if (divisor.empty()) divisor = {1U};
    const auto quotient = random_polynomial(rng, prime, 4U);
    PrimeFieldPolynomial remainder;
    if (divisor.size() > 1U) {
      remainder = random_polynomial(rng, prime, divisor.size() - 1U);
    }
    const auto dividend = small_add(
        small_multiply(divisor, quotient, prime), remainder, prime);
    const auto actual = polynomial_divide_mod(dividend, divisor, prime);
    REQUIRE_EQ(actual.quotient, quotient);
    REQUIRE_EQ(actual.remainder, remainder);
    REQUIRE(actual.remainder.empty() || actual.remainder.size() < divisor.size());
  }
}

TEST_CASE(prime_field_polynomial_random_gcd_against_exhaustive_divisors) {
  std::mt19937_64 rng(0x6CD6CDULL);
  constexpr std::uint64_t prime = 3U;
  for (std::size_t trial = 0; trial < 350U; ++trial) {
    const auto first = random_polynomial(rng, prime, 5U);
    const auto second = random_polynomial(rng, prime, 5U);
    const auto expected = brute_gcd(first, second, prime);
    const auto actual = polynomial_extended_gcd_mod(first, second, prime);
    REQUIRE_EQ(actual.gcd, expected);
    REQUIRE(actual.gcd.empty() || actual.gcd.back() == 1U);
    REQUIRE_EQ(small_add(small_multiply(actual.bezout_first, first, prime),
                         small_multiply(actual.bezout_second, second, prime),
                         prime),
               actual.gcd);
  }
}
