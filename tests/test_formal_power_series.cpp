#include "algorithms/polynomials/formal_power_series.hpp"

#include "algorithms/polynomials/number_theoretic_transform.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::polynomials::formal_power_series_inverse;
using algorithms::polynomials::kMaximumFormalPowerSeriesInverseTerms;
using algorithms::polynomials::kNttModulus;

namespace {

std::uint64_t multiply_mod(std::uint64_t left, std::uint64_t right) {
  return ((left % kNttModulus) * (right % kNttModulus)) % kNttModulus;
}

std::uint64_t oracle_power(std::uint64_t base, std::uint64_t exponent) {
  std::uint64_t result = 1U;
  base %= kNttModulus;
  while (exponent != 0U) {
    if ((exponent & 1U) != 0U) {
      result = multiply_mod(result, base);
    }
    exponent >>= 1U;
    if (exponent != 0U) {
      base = multiply_mod(base, base);
    }
  }
  return result;
}

std::vector<std::uint64_t> naive_inverse(
    const std::vector<std::uint64_t>& series, std::size_t terms) {
  if (terms == 0U) {
    return {};
  }
  const std::uint64_t constant = series[0] % kNttModulus;
  const std::uint64_t inverse_constant =
      oracle_power(constant, kNttModulus - 2U);
  std::vector<std::uint64_t> result(terms, 0U);
  result[0] = inverse_constant;

  for (std::size_t degree = 1U; degree < terms; ++degree) {
    std::uint64_t sum = 0U;
    const std::size_t maximum_index = std::min(degree, series.size() - 1U);
    for (std::size_t index = 1U; index <= maximum_index; ++index) {
      const std::uint64_t term =
          multiply_mod(series[index], result[degree - index]);
      sum += term;
      if (sum >= kNttModulus) {
        sum -= kNttModulus;
      }
    }
    const std::uint64_t negative = sum == 0U ? 0U : kNttModulus - sum;
    result[degree] = multiply_mod(negative, inverse_constant);
  }
  return result;
}

}  // namespace

TEST_CASE(formal_power_series_inverse_deterministic_and_validation) {
  REQUIRE(formal_power_series_inverse({}, 0U).empty());
  REQUIRE_THROWS_AS(formal_power_series_inverse({}, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(formal_power_series_inverse({kNttModulus}, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      formal_power_series_inverse({1U},
                                  kMaximumFormalPowerSeriesInverseTerms + 1U),
      std::length_error);

  const auto geometric = formal_power_series_inverse({1U, 1U}, 6U);
  REQUIRE_EQ(geometric,
             (std::vector<std::uint64_t>{1U, kNttModulus - 1U, 1U,
                                         kNttModulus - 1U, 1U,
                                         kNttModulus - 1U}));

  const auto constant = formal_power_series_inverse({3U}, 5U);
  const std::uint64_t inverse_three = oracle_power(3U, kNttModulus - 2U);
  REQUIRE_EQ(constant,
             (std::vector<std::uint64_t>{inverse_three, 0U, 0U, 0U, 0U}));
}

TEST_CASE(formal_power_series_inverse_randomized_against_recurrence) {
  std::mt19937_64 rng(0xF051A11ULL);
  std::uniform_int_distribution<std::size_t> length_dist(1U, 30U);
  std::uniform_int_distribution<std::size_t> terms_dist(1U, 64U);

  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    std::vector<std::uint64_t> series(length_dist(rng), 0U);
    for (auto& value : series) {
      value = rng();
    }
    if (series[0] % kNttModulus == 0U) {
      ++series[0];
    }
    const std::size_t terms = terms_dist(rng);
    REQUIRE_EQ(formal_power_series_inverse(series, terms),
               naive_inverse(series, terms));
  }
}
