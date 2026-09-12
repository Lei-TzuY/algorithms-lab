#pragma once

#include "algorithms/numerical/fast_fourier_transform.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

namespace {

using algorithms::numerical::Complex64;

[[nodiscard]] std::vector<Complex64> direct_dft(
    const std::vector<Complex64>& input, bool inverse) {
  const std::size_t n = input.size();
  std::vector<Complex64> result(n);
  if (n == 0U) {
    return result;
  }
  const double sign = inverse ? 1.0 : -1.0;
  for (std::size_t k = 0U; k < n; ++k) {
    Complex64 sum(0.0, 0.0);
    for (std::size_t t = 0U; t < n; ++t) {
      const double angle = sign * 2.0 * std::numbers::pi_v<double> *
                           static_cast<double>(k) * static_cast<double>(t) /
                           static_cast<double>(n);
      sum += input[t] * Complex64(std::cos(angle), std::sin(angle));
    }
    if (inverse) {
      sum /= static_cast<double>(n);
    }
    result[k] = sum;
  }
  return result;
}

[[nodiscard]] bool close_complex(const Complex64& actual,
                                 const Complex64& expected,
                                 double scale = 1.0) {
  const double tolerance = 2.0e-10 * (1.0 + scale + std::abs(expected));
  return std::abs(actual - expected) <= tolerance;
}

[[nodiscard]] std::vector<double> naive_convolution(
    const std::vector<double>& left, const std::vector<double>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }
  std::vector<double> result(left.size() + right.size() - 1U, 0.0);
  for (std::size_t i = 0U; i < left.size(); ++i) {
    for (std::size_t j = 0U; j < right.size(); ++j) {
      result[i + j] += left[i] * right[j];
    }
  }
  return result;
}

TEST_CASE(fft_deterministic_contract_and_validation) {
  using algorithms::numerical::fast_fourier_transform;
  using algorithms::numerical::fft_real_convolution;

  std::vector<Complex64> empty;
  fast_fourier_transform(empty, false);
  REQUIRE(empty.empty());

  std::vector<Complex64> singleton = {Complex64(3.5, -2.0)};
  fast_fourier_transform(singleton, false);
  REQUIRE(close_complex(singleton[0], Complex64(3.5, -2.0)));
  fast_fourier_transform(singleton, true);
  REQUIRE(close_complex(singleton[0], Complex64(3.5, -2.0)));

  std::vector<Complex64> invalid_length(3U);
  REQUIRE_THROWS_AS(fast_fourier_transform(invalid_length, false),
                    std::invalid_argument);

  std::vector<Complex64> nonfinite = {
      Complex64(std::numeric_limits<double>::infinity(), 0.0), Complex64{}};
  REQUIRE_THROWS_AS(fast_fourier_transform(nonfinite, false),
                    std::invalid_argument);

  const std::vector<double> no_values;
  const std::vector<double> one_value = {1.0};
  REQUIRE(fft_real_convolution(no_values, one_value).empty());
  const std::vector<double> bad = {std::numeric_limits<double>::quiet_NaN()};
  REQUIRE_THROWS_AS(fft_real_convolution(bad, one_value), std::invalid_argument);
}

TEST_CASE(fft_matches_independent_direct_dft) {
  using algorithms::numerical::fast_fourier_transform;

  std::mt19937_64 rng(0xF17F17ULL);
  std::uniform_real_distribution<double> distribution(-10.0, 10.0);
  const std::vector<std::size_t> sizes = {1U, 2U, 4U, 8U, 16U, 32U};

  for (const std::size_t size : sizes) {
    for (std::size_t trial = 0U; trial < 80U; ++trial) {
      std::vector<Complex64> values(size);
      double max_abs = 0.0;
      for (auto& value : values) {
        value = Complex64(distribution(rng), distribution(rng));
        max_abs = std::max(max_abs, std::abs(value));
      }
      const auto expected = direct_dft(values, false);
      auto actual = values;
      fast_fourier_transform(actual, false);
      const double scale = static_cast<double>(size) * max_abs;
      for (std::size_t index = 0U; index < size; ++index) {
        REQUIRE(close_complex(actual[index], expected[index], scale));
      }
    }
  }
}

TEST_CASE(fft_round_trip_randomized) {
  using algorithms::numerical::fast_fourier_transform;

  std::mt19937_64 rng(0xC0FFEE1234ULL);
  std::uniform_real_distribution<double> distribution(-100.0, 100.0);
  const std::vector<std::size_t> sizes = {1U, 2U, 4U, 8U, 16U, 32U,
                                          64U, 128U, 256U};
  for (const std::size_t size : sizes) {
    for (std::size_t trial = 0U; trial < 80U; ++trial) {
      std::vector<Complex64> original(size);
      for (auto& value : original) {
        value = Complex64(distribution(rng), distribution(rng));
      }
      auto transformed = original;
      fast_fourier_transform(transformed, false);
      fast_fourier_transform(transformed, true);
      for (std::size_t index = 0U; index < size; ++index) {
        REQUIRE(close_complex(transformed[index], original[index], 200.0));
      }
    }
  }
}

TEST_CASE(fft_real_convolution_matches_naive_oracle) {
  using algorithms::numerical::fft_real_convolution;

  const std::vector<double> left_known = {1.0, -2.0, 3.0};
  const std::vector<double> right_known = {4.0, 5.0};
  const auto known = fft_real_convolution(left_known, right_known);
  const std::vector<double> known_expected = {4.0, -3.0, 2.0, 15.0};
  REQUIRE_EQ(known.size(), known_expected.size());
  for (std::size_t index = 0U; index < known.size(); ++index) {
    REQUIRE(std::abs(known[index] - known_expected[index]) <= 1.0e-10);
  }

  std::mt19937_64 rng(0xD1FF7ULL);
  std::uniform_int_distribution<int> length_distribution(1, 32);
  std::uniform_real_distribution<double> value_distribution(-20.0, 20.0);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t left_size =
        static_cast<std::size_t>(length_distribution(rng));
    const std::size_t right_size =
        static_cast<std::size_t>(length_distribution(rng));
    std::vector<double> left(left_size);
    std::vector<double> right(right_size);
    for (auto& value : left) {
      value = value_distribution(rng);
    }
    for (auto& value : right) {
      value = value_distribution(rng);
    }
    const auto expected = naive_convolution(left, right);
    const auto actual = fft_real_convolution(left, right);
    REQUIRE_EQ(actual.size(), expected.size());
    const double scale =
        400.0 * static_cast<double>(std::min(left_size, right_size));
    for (std::size_t index = 0U; index < actual.size(); ++index) {
      const double tolerance =
          5.0e-10 * (1.0 + scale + std::abs(expected[index]));
      REQUIRE(std::abs(actual[index] - expected[index]) <= tolerance);
    }
  }
}

}  // namespace
