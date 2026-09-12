#pragma once

#include "algorithms/polynomials/distinct_degree_factorization.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace distinct_degree_test_detail {

using algorithms::polynomials::PrimeFieldPolynomial;

inline void trim(PrimeFieldPolynomial& polynomial) {
  while (!polynomial.empty() && polynomial.back() == 0U) {
    polynomial.pop_back();
  }
}

[[nodiscard]] inline PrimeFieldPolynomial multiply(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    std::uint64_t prime) {
  if (first.empty() || second.empty()) {
    return {};
  }
  PrimeFieldPolynomial result(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0U; left < first.size(); ++left) {
    for (std::size_t right = 0U; right < second.size(); ++right) {
      result[left + right] =
          (result[left + right] + (first[left] * second[right]) % prime) %
          prime;
    }
  }
  trim(result);
  return result;
}

[[nodiscard]] inline std::uint64_t inverse(std::uint64_t value,
                                           std::uint64_t prime) {
  for (std::uint64_t candidate = 1U; candidate < prime; ++candidate) {
    if ((value * candidate) % prime == 1U) {
      return candidate;
    }
  }
  throw std::logic_error("small-field oracle could not find an inverse");
}

[[nodiscard]] inline std::pair<PrimeFieldPolynomial, PrimeFieldPolynomial>
divide(PrimeFieldPolynomial dividend, PrimeFieldPolynomial divisor,
       std::uint64_t prime) {
  trim(dividend);
  trim(divisor);
  if (divisor.empty()) {
    throw std::logic_error("small-field oracle division by zero");
  }

  PrimeFieldPolynomial quotient(
      dividend.size() >= divisor.size()
          ? dividend.size() - divisor.size() + 1U
          : 0U,
      0U);
  const std::uint64_t inverse_leading = inverse(divisor.back(), prime);
  while (!dividend.empty() && dividend.size() >= divisor.size()) {
    const std::size_t shift = dividend.size() - divisor.size();
    const std::uint64_t factor =
        (dividend.back() * inverse_leading) % prime;
    quotient[shift] = factor;
    for (std::size_t index = 0U; index < divisor.size(); ++index) {
      const std::uint64_t term = (factor * divisor[index]) % prime;
      dividend[shift + index] = dividend[shift + index] >= term
                                    ? dividend[shift + index] - term
                                    : prime - (term - dividend[shift + index]);
    }
    trim(dividend);
  }
  trim(quotient);
  return {std::move(quotient), std::move(dividend)};
}

[[nodiscard]] inline bool divides(const PrimeFieldPolynomial& divisor,
                                  const PrimeFieldPolynomial& dividend,
                                  std::uint64_t prime) {
  return divide(dividend, divisor, prime).second.empty();
}

[[nodiscard]] inline std::vector<PrimeFieldPolynomial> monic_polynomials(
    std::size_t degree, std::uint64_t prime) {
  std::vector<PrimeFieldPolynomial> result;
  std::uint64_t count = 1U;
  for (std::size_t index = 0U; index < degree; ++index) {
    count *= prime;
  }
  for (std::uint64_t code = 0U; code < count; ++code) {
    std::uint64_t cursor = code;
    PrimeFieldPolynomial polynomial(degree + 1U, 0U);
    polynomial[degree] = 1U;
    for (std::size_t index = 0U; index < degree; ++index) {
      polynomial[index] = cursor % prime;
      cursor /= prime;
    }
    result.push_back(std::move(polynomial));
  }
  return result;
}

[[nodiscard]] inline bool is_irreducible(const PrimeFieldPolynomial& polynomial,
                                         std::uint64_t prime) {
  const std::size_t degree = polynomial.size() - 1U;
  if (degree == 1U) {
    return true;
  }
  for (std::size_t candidate_degree = 1U;
       candidate_degree <= degree / 2U; ++candidate_degree) {
    for (const auto& candidate : monic_polynomials(candidate_degree, prime)) {
      if (divides(candidate, polynomial, prime)) {
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] inline std::vector<std::vector<PrimeFieldPolynomial>>
irreducibles(std::uint64_t prime, std::size_t maximum_degree) {
  std::vector<std::vector<PrimeFieldPolynomial>> result(maximum_degree + 1U);
  for (std::size_t degree = 1U; degree <= maximum_degree; ++degree) {
    for (auto polynomial : monic_polynomials(degree, prime)) {
      if (is_irreducible(polynomial, prime)) {
        result[degree].push_back(std::move(polynomial));
      }
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial scale(PrimeFieldPolynomial polynomial,
                                                std::uint64_t unit,
                                                std::uint64_t prime) {
  for (std::uint64_t& coefficient : polynomial) {
    coefficient = (coefficient * unit) % prime;
  }
  return polynomial;
}

inline void require_factorization(
    const PrimeFieldPolynomial& input, std::uint64_t prime,
    std::uint64_t expected_unit,
    const std::vector<std::pair<std::size_t, PrimeFieldPolynomial>>& expected) {
  const auto result =
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          input, prime);
  REQUIRE_EQ(result.unit, expected_unit);
  REQUIRE_EQ(result.factors.size(), expected.size());
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    REQUIRE_EQ(result.factors[index].irreducible_degree,
               expected[index].first);
    REQUIRE_EQ(result.factors[index].factor, expected[index].second);
  }
}

}  // namespace distinct_degree_test_detail

TEST_CASE(prime_field_distinct_degree_validation_and_known_groups) {
  using namespace distinct_degree_test_detail;
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          {}, 5U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          {1U, 2U}, 4U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          {1U, 5U}, 5U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          multiply({1U, 1U}, {1U, 1U}, 5U), 5U),
      std::invalid_argument);

  const auto constant =
      algorithms::polynomials::polynomial_distinct_degree_factorization_mod(
          {3U}, 5U);
  REQUIRE_EQ(constant.unit, 3U);
  REQUIRE(constant.factors.empty());

  const PrimeFieldPolynomial degree_one{1U, 1U};
  const PrimeFieldPolynomial degree_two{1U, 1U, 1U};
  const PrimeFieldPolynomial degree_three{1U, 1U, 0U, 1U};
  const auto mixed = multiply(multiply(degree_one, degree_two, 2U),
                              degree_three, 2U);
  require_factorization(
      mixed, 2U, 1U,
      {{1U, degree_one}, {2U, degree_two}, {3U, degree_three}});

  const PrimeFieldPolynomial other_degree_three{1U, 0U, 1U, 1U};
  const auto same_degree = multiply(degree_three, other_degree_three, 2U);
  require_factorization(same_degree, 2U, 1U, {{3U, same_degree}});
}

TEST_CASE(prime_field_distinct_degree_random_exhaustive_factor_oracle) {
  using namespace distinct_degree_test_detail;
  std::mt19937_64 rng(0xD15C71C7ULL);
  for (const std::uint64_t prime : {2ULL, 3ULL, 5ULL}) {
    const auto all_irreducibles = irreducibles(prime, 5U);
    for (std::size_t trial = 0U; trial < 450U; ++trial) {
      PrimeFieldPolynomial product{1U};
      std::vector<std::pair<std::size_t, PrimeFieldPolynomial>> expected;
      std::size_t total_degree = 0U;

      for (std::size_t degree = 1U; degree <= 5U; ++degree) {
        PrimeFieldPolynomial group{1U};
        bool used = false;
        for (const auto& factor : all_irreducibles[degree]) {
          if (total_degree + degree > 9U) {
            break;
          }
          if ((rng() % 5U) == 0U) {
            group = multiply(group, factor, prime);
            total_degree += degree;
            used = true;
          }
        }
        if (used) {
          expected.push_back({degree, group});
          product = multiply(product, group, prime);
        }
      }

      if (product.size() == 1U) {
        const auto& factor = all_irreducibles[1U][static_cast<std::size_t>(
            rng() % all_irreducibles[1U].size())];
        expected.push_back({1U, factor});
        product = factor;
      }

      const std::uint64_t unit = 1U + (rng() % (prime - 1U));
      require_factorization(scale(product, unit, prime), prime, unit,
                            expected);
    }
  }
}

TEST_CASE(prime_field_distinct_degree_full_width_prime_linear_group) {
  using namespace distinct_degree_test_detail;
  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const PrimeFieldPolynomial first{prime - 1U, 1U};
  const PrimeFieldPolynomial second{prime - 2U, 1U};
  PrimeFieldPolynomial product(3U, 0U);
  product[0U] = algorithms::number_theory::multiply_mod(first[0U], second[0U],
                                                        prime);
  product[1U] = first[0U] >= prime - second[0U]
                    ? first[0U] - (prime - second[0U])
                    : first[0U] + second[0U];
  product[2U] = 1U;
  require_factorization(product, prime, 1U, {{1U, product}});
}
