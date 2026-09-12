#pragma once

#include "algorithms/number_theory/modular.hpp"
#include "algorithms/polynomials/equal_degree_factorization.hpp"
#include "test_distinct_degree_factorization_cases.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace equal_degree_test_detail {
using algorithms::polynomials::PrimeFieldPolynomial;
using distinct_degree_test_detail::irreducibles;
using distinct_degree_test_detail::multiply;
using distinct_degree_test_detail::scale;

[[nodiscard]] inline std::uint64_t add_mod_reconstruction(
    std::uint64_t first, std::uint64_t second,
    std::uint64_t prime) noexcept {
  return first >= prime - second ? first - (prime - second) : first + second;
}

[[nodiscard]] inline PrimeFieldPolynomial multiply_reconstruction(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    std::uint64_t prime) {
  if (first.empty() || second.empty()) {
    return {};
  }
  PrimeFieldPolynomial result(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0U; left < first.size(); ++left) {
    for (std::size_t right = 0U; right < second.size(); ++right) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          first[left], second[right], prime);
      result[left + right] =
          add_mod_reconstruction(result[left + right], term, prime);
    }
  }
  distinct_degree_test_detail::trim(result);
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial scale_reconstruction(
    PrimeFieldPolynomial polynomial, std::uint64_t unit,
    std::uint64_t prime) {
  for (std::uint64_t& coefficient : polynomial) {
    coefficient =
        algorithms::number_theory::multiply_mod(coefficient, unit, prime);
  }
  distinct_degree_test_detail::trim(polynomial);
  return polynomial;
}

inline void require_complete_factorization(
    const PrimeFieldPolynomial& input, std::size_t degree, std::uint64_t prime,
    std::uint64_t seed, const std::vector<PrimeFieldPolynomial>& expected,
    std::uint64_t expected_unit) {
  const auto result =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          input, degree, prime, seed, 512U);
  REQUIRE(result.complete);
  REQUIRE_EQ(result.unit, expected_unit);
  REQUIRE_EQ(result.seed, seed);
  auto sorted_expected = expected;
  std::sort(sorted_expected.begin(), sorted_expected.end());
  REQUIRE_EQ(result.factors, sorted_expected);

  PrimeFieldPolynomial reconstructed{1U};
  for (const auto& factor : result.factors) {
    REQUIRE_EQ(factor.size(), degree + 1U);
    REQUIRE_EQ(factor.back(), 1U);
    reconstructed = multiply_reconstruction(reconstructed, factor, prime);
  }
  REQUIRE_EQ(scale_reconstruction(reconstructed, result.unit, prime), input);
}
}  // namespace equal_degree_test_detail

TEST_CASE(prime_field_equal_degree_validation_budget_and_replay) {
  using namespace equal_degree_test_detail;
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          {1U, 1U}, 0U, 5U, 1U, 10U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          {}, 1U, 5U, 1U, 10U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          {1U, 1U}, 1U, 4U, 1U, 10U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          {1U, 5U}, 1U, 5U, 1U, 10U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          multiply({1U, 1U}, {1U, 1U}, 5U), 1U, 5U, 1U, 10U),
      std::invalid_argument);

  const PrimeFieldPolynomial linear{1U, 1U};
  const PrimeFieldPolynomial quadratic{1U, 1U, 1U};
  REQUIRE_THROWS_AS(
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          multiply(linear, quadratic, 2U), 1U, 2U, 1U, 10U),
      std::invalid_argument);

  const auto constant =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          {3U}, 1U, 5U, 17U, 0U);
  REQUIRE(constant.complete);
  REQUIRE_EQ(constant.unit, 3U);
  REQUIRE(constant.factors.empty());
  REQUIRE_EQ(constant.trials_executed, 0U);

  const auto irreducible =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          quadratic, 2U, 2U, 17U, 0U);
  REQUIRE(irreducible.complete);
  REQUIRE_EQ(irreducible.factors,
             std::vector<PrimeFieldPolynomial>{quadratic});

  const auto reducible = multiply({4U, 1U}, {3U, 1U}, 5U);
  const auto exhausted =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          reducible, 1U, 5U, 7U, 0U);
  REQUIRE(!exhausted.complete);
  REQUIRE(exhausted.factors.empty());
  REQUIRE_EQ(exhausted.trials_executed, 0U);

  const auto first =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          reducible, 1U, 5U, 0xE0F123ULL, 128U);
  const auto second =
      algorithms::polynomials::polynomial_equal_degree_factorization_mod(
          reducible, 1U, 5U, 0xE0F123ULL, 128U);
  REQUIRE(first.complete);
  REQUIRE_EQ(first.factors, second.factors);
  REQUIRE_EQ(first.trials_executed, second.trials_executed);
}

TEST_CASE(prime_field_equal_degree_binary_trace_and_full_width_prime) {
  using namespace equal_degree_test_detail;
  const PrimeFieldPolynomial first_cubic{1U, 1U, 0U, 1U};
  const PrimeFieldPolynomial second_cubic{1U, 0U, 1U, 1U};
  const auto binary_group = multiply(first_cubic, second_cubic, 2U);
  require_complete_factorization(binary_group, 3U, 2U, 123U,
                                 {first_cubic, second_cubic}, 1U);

  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const PrimeFieldPolynomial first_linear{prime - 1U, 1U};
  const PrimeFieldPolynomial second_linear{prime - 2U, 1U};
  PrimeFieldPolynomial full_width_group(3U, 0U);
  full_width_group[0U] = algorithms::number_theory::multiply_mod(
      first_linear[0U], second_linear[0U], prime);
  full_width_group[1U] =
      first_linear[0U] >= prime - second_linear[0U]
          ? first_linear[0U] - (prime - second_linear[0U])
          : first_linear[0U] + second_linear[0U];
  full_width_group[2U] = 1U;
  REQUIRE_EQ(multiply_reconstruction(first_linear, second_linear, prime),
             full_width_group);
  require_complete_factorization(full_width_group, 1U, prime, 0xF00DULL,
                                 {first_linear, second_linear}, 1U);
}

TEST_CASE(prime_field_equal_degree_random_independent_irreducible_oracle) {
  using namespace equal_degree_test_detail;
  std::mt19937_64 rng(0xEDFC0DEULL);
  struct Configuration {
    std::uint64_t prime;
    std::size_t degree;
  };
  const std::vector<Configuration> configurations{{2U, 3U}, {3U, 1U},
                                                   {3U, 2U}, {3U, 3U},
                                                   {5U, 1U}, {5U, 2U}};

  for (const auto configuration : configurations) {
    const auto by_degree = irreducibles(configuration.prime,
                                        configuration.degree);
    const auto& pool = by_degree[configuration.degree];
    REQUIRE(!pool.empty());
    for (std::size_t trial = 0U; trial < 60U; ++trial) {
      const std::size_t maximum_count =
          std::min<std::size_t>(3U, pool.size());
      const std::size_t count =
          1U + static_cast<std::size_t>(rng() % maximum_count);
      std::vector<std::size_t> indices(pool.size());
      std::iota(indices.begin(), indices.end(), 0U);
      std::shuffle(indices.begin(), indices.end(), rng);

      PrimeFieldPolynomial monic{1U};
      std::vector<PrimeFieldPolynomial> expected;
      for (std::size_t index = 0U; index < count; ++index) {
        expected.push_back(pool[indices[index]]);
        monic = multiply(monic, pool[indices[index]], configuration.prime);
      }
      const std::uint64_t unit =
          1U + (rng() % (configuration.prime - 1U));
      const auto input = scale(monic, unit, configuration.prime);
      require_complete_factorization(
          input, configuration.degree, configuration.prime, rng(), expected,
          unit);
    }
  }
}
