#pragma once

#include "algorithms/numerical/bluestein_fourier_transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::numerical::Complex64;
using algorithms::numerical::bluestein_fourier_transform;
using algorithms::numerical::fast_fourier_transform;

namespace bluestein_fourier_transform_test_detail {

inline std::vector<Complex64> direct_dft(
    const std::vector<Complex64>& input,
    const bool inverse) {
  const std::size_t n = input.size();
  if (n == 0U) {
    return {};
  }

  std::vector<Complex64> output(n);
  const long double sign = inverse ? 2.0L : -2.0L;
  for (std::size_t frequency = 0U;
       frequency < n; ++frequency) {
    Complex64 sum(0.0, 0.0);
    for (std::size_t time = 0U; time < n; ++time) {
      const long double angle =
          sign * std::numbers::pi_v<long double> *
          static_cast<long double>(frequency) *
          static_cast<long double>(time) /
          static_cast<long double>(n);
      const Complex64 root(
          static_cast<double>(std::cos(angle)),
          static_cast<double>(std::sin(angle)));
      sum += input[time] * root;
    }
    if (inverse) {
      sum /= static_cast<double>(n);
    }
    output[frequency] = sum;
  }
  return output;
}

inline double scale_for(
    const std::vector<Complex64>& values) {
  double scale = 1.0;
  for (const Complex64& value : values) {
    scale = std::max(scale, std::abs(value));
  }
  return scale;
}

inline void require_close(
    const std::vector<Complex64>& actual,
    const std::vector<Complex64>& expected,
    const double tolerance) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t index = 0U;
       index < actual.size(); ++index) {
    REQUIRE(std::abs(actual[index] - expected[index]) <=
            tolerance);
  }
}

}  // namespace bluestein_fourier_transform_test_detail

TEST_CASE(bluestein_transform_empty_singleton_and_nonfinite_contract) {
  std::vector<Complex64> empty;
  bluestein_fourier_transform(empty, false);
  REQUIRE(empty.empty());

  std::vector<Complex64> singleton{
      Complex64(3.5, -2.25)};
  const auto original = singleton;
  bluestein_fourier_transform(singleton, false);
  REQUIRE(singleton == original);
  bluestein_fourier_transform(singleton, true);
  REQUIRE(singleton == original);

  std::vector<Complex64> nonfinite_three{
      Complex64(1.0, 0.0),
      Complex64(std::numeric_limits<double>::infinity(), 0.0),
      Complex64(2.0, 0.0)};
  REQUIRE_THROWS_AS(
      bluestein_fourier_transform(nonfinite_three, false),
      std::invalid_argument);

  std::vector<Complex64> nonfinite_four{
      Complex64(1.0, 0.0),
      Complex64(2.0, 0.0),
      Complex64(3.0, 0.0),
      Complex64(0.0, std::numeric_limits<double>::quiet_NaN())};
  REQUIRE_THROWS_AS(
      bluestein_fourier_transform(nonfinite_four, false),
      std::invalid_argument);
}

TEST_CASE(bluestein_transform_matches_direct_dft_fixed_lengths) {
  using namespace bluestein_fourier_transform_test_detail;

  const std::array<std::size_t, 11U> lengths{
      2U, 3U, 5U, 6U, 7U, 9U,
      10U, 11U, 12U, 13U, 15U};

  for (const std::size_t length : lengths) {
    std::vector<Complex64> input(length);
    for (std::size_t index = 0U;
         index < length; ++index) {
      input[index] = Complex64(
          static_cast<double>(
              static_cast<long long>(index * 7U % 17U) - 8LL),
          static_cast<double>(
              static_cast<long long>(index * 5U % 13U) - 6LL) /
              3.0);
    }

    const auto expected_forward = direct_dft(input, false);
    auto actual_forward = input;
    bluestein_fourier_transform(actual_forward, false);

    const double forward_tolerance =
        2.0e-10 * static_cast<double>(length) *
        scale_for(input);
    require_close(
        actual_forward, expected_forward, forward_tolerance);

    const auto expected_inverse = direct_dft(input, true);
    auto actual_inverse = input;
    bluestein_fourier_transform(actual_inverse, true);

    const double inverse_tolerance =
        2.0e-10 * static_cast<double>(length) *
        scale_for(input);
    require_close(
        actual_inverse, expected_inverse, inverse_tolerance);
  }
}

TEST_CASE(bluestein_power_of_two_matches_radix2_fft) {
  using namespace bluestein_fourier_transform_test_detail;

  std::mt19937_64 random(0xB1E57E1ULL);
  const std::array<std::size_t, 6U> lengths{
      1U, 2U, 4U, 8U, 16U, 32U};

  for (const std::size_t length : lengths) {
    for (std::size_t trial = 0U; trial < 30U; ++trial) {
      std::vector<Complex64> input(length);
      for (Complex64& value : input) {
        value = Complex64(
            static_cast<double>(
                static_cast<long long>(random() % 2001U) -
                1000LL) /
                100.0,
            static_cast<double>(
                static_cast<long long>(random() % 2001U) -
                1000LL) /
                100.0);
      }

      auto bluestein = input;
      auto radix2 = input;
      bluestein_fourier_transform(bluestein, false);
      fast_fourier_transform(radix2, false);
      require_close(bluestein, radix2, 1.0e-13);

      bluestein_fourier_transform(bluestein, true);
      fast_fourier_transform(radix2, true);
      require_close(bluestein, radix2, 1.0e-12);
    }
  }
}

TEST_CASE(bluestein_random_arbitrary_lengths_match_direct_dft) {
  using namespace bluestein_fourier_transform_test_detail;

  std::mt19937_64 random(0xA2B17A2FULL);
  for (std::size_t trial = 0U; trial < 360U; ++trial) {
    const std::size_t length =
        1U + static_cast<std::size_t>(random() % 31U);
    std::vector<Complex64> input(length);
    for (Complex64& value : input) {
      value = Complex64(
          static_cast<double>(
              static_cast<long long>(random() % 2001U) -
              1000LL) /
              80.0,
          static_cast<double>(
              static_cast<long long>(random() % 2001U) -
              1000LL) /
              80.0);
    }

    const bool inverse = (random() & 1ULL) != 0ULL;
    const auto expected = direct_dft(input, inverse);
    auto actual = input;
    bluestein_fourier_transform(actual, inverse);

    const double tolerance =
        5.0e-10 * static_cast<double>(length) *
        scale_for(input);
    require_close(actual, expected, tolerance);
  }
}

TEST_CASE(bluestein_arbitrary_length_round_trip) {
  using namespace bluestein_fourier_transform_test_detail;

  std::mt19937_64 random(0xC41A9E5EEDULL);
  for (std::size_t trial = 0U; trial < 480U; ++trial) {
    const std::size_t length =
        1U + static_cast<std::size_t>(random() % 63U);
    std::vector<Complex64> values(length);
    for (Complex64& value : values) {
      value = Complex64(
          static_cast<double>(
              static_cast<long long>(random() % 4001U) -
              2000LL) /
              125.0,
          static_cast<double>(
              static_cast<long long>(random() % 4001U) -
              2000LL) /
              125.0);
    }

    const auto original = values;
    bluestein_fourier_transform(values, false);
    bluestein_fourier_transform(values, true);

    const double tolerance =
        1.0e-9 * static_cast<double>(length) *
        scale_for(original);
    require_close(values, original, tolerance);
  }
}
