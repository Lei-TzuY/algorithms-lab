#include "algorithms/number_theory/polynomial_interpolation.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::number_theory::FieldSample;
using algorithms::number_theory::evaluate_polynomial_mod;
using algorithms::number_theory::interpolate_prime_field;

std::uint64_t evaluate_small_oracle(
    const std::vector<std::uint64_t>& coefficients, std::uint64_t x,
    std::uint64_t modulus) {
  std::uint64_t result = 0U;
  std::uint64_t power = 1U;
  for (const std::uint64_t coefficient : coefficients) {
    result = (result + (coefficient % modulus) * power) % modulus;
    power = (power * (x % modulus)) % modulus;
  }
  return result;
}

TEST_CASE(prime_field_interpolation_known_and_empty_cases) {
  REQUIRE(interpolate_prime_field({}, 5U).empty());
  REQUIRE_EQ(evaluate_polynomial_mod({}, 123U, 5U), 0U);

  const std::vector<FieldSample> samples{{0U, 1U}, {1U, 6U}, {2U, 17U}};
  REQUIRE_EQ(interpolate_prime_field(samples, 101U),
             (std::vector<std::uint64_t>{1U, 2U, 3U}));
}

TEST_CASE(prime_field_interpolation_validates_field_and_abscissas) {
  REQUIRE_THROWS_AS(interpolate_prime_field(std::vector<FieldSample>{{0U, 1U}},
                                            1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(interpolate_prime_field(std::vector<FieldSample>{{0U, 1U}},
                                            21U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      interpolate_prime_field(
          std::vector<FieldSample>{{0U, 1U}, {5U, 2U}}, 5U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      interpolate_prime_field(
          std::vector<FieldSample>{{0U, 0U}, {1U, 0U}, {2U, 0U}}, 2U),
      std::invalid_argument);
}

TEST_CASE(prime_field_interpolation_full_width_known_answer) {
  constexpr std::uint64_t prime = 18446744073709551557ULL;
  const std::vector<FieldSample> samples{
      {prime - 1U, 18446744073709551542ULL},
      {0U, 18446744073709551554ULL},
      {2U, 18446744073709551536ULL}};
  const std::vector<std::uint64_t> expected{prime - 3U, 5U, prime - 7U};
  REQUIRE_EQ(interpolate_prime_field(samples, prime), expected);
  REQUIRE_EQ(evaluate_polynomial_mod(expected, 1234567890123456789ULL, prime),
             14755924361279484973ULL);
}

TEST_CASE(prime_field_interpolation_randomized_coefficient_differential) {
  std::mt19937_64 random(0x1A7E2B01ULL);
  const std::vector<std::uint64_t> primes{2U, 3U, 5U, 7U, 11U,
                                          17U, 31U, 101U, 257U, 1009U};

  for (int trial = 0; trial < 1500; ++trial) {
    const std::uint64_t modulus =
        primes[static_cast<std::size_t>(random() % primes.size())];
    const std::size_t max_size = static_cast<std::size_t>(
        std::min<std::uint64_t>(8U, modulus));
    const std::size_t size =
        static_cast<std::size_t>(random() % (max_size + 1U));

    std::vector<std::uint64_t> coefficients(size);
    for (auto& coefficient : coefficients) {
      coefficient = random() % modulus;
    }

    std::vector<std::uint64_t> field_x(static_cast<std::size_t>(modulus));
    for (std::size_t index = 0; index < field_x.size(); ++index) {
      field_x[index] = index;
    }
    std::shuffle(field_x.begin(), field_x.end(), random);

    std::vector<FieldSample> samples;
    samples.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      const std::uint64_t canonical_x = field_x[index];
      samples.push_back(FieldSample{
          canonical_x + modulus * (random() % 3U),
          evaluate_small_oracle(coefficients, canonical_x, modulus)});
    }

    const auto actual = interpolate_prime_field(samples, modulus);
    REQUIRE_EQ(actual, coefficients);
    for (const auto& sample : samples) {
      REQUIRE_EQ(evaluate_polynomial_mod(actual, sample.x, modulus), sample.y);
    }
  }
}

}  // namespace
