#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

using Complex64 = std::complex<double>;

namespace detail {

[[nodiscard]] inline bool is_power_of_two(std::size_t value) noexcept {
  return value != 0U && (value & (value - 1U)) == 0U;
}

inline void validate_finite(std::span<const Complex64> values) {
  for (const auto& value : values) {
    if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) {
      throw std::invalid_argument("FFT input must contain only finite values");
    }
  }
}

[[nodiscard]] inline std::size_t next_power_of_two(std::size_t value) {
  if (value == 0U) {
    return 1U;
  }
  std::size_t result = 1U;
  while (result < value) {
    if (result > std::numeric_limits<std::size_t>::max() / 2U) {
      throw std::length_error("FFT convolution size is not representable");
    }
    result *= 2U;
  }
  return result;
}

}  // namespace detail

// In-place radix-2 Cooley-Tukey transform over std::complex<double>.
// Empty input is a no-op; non-empty input must have power-of-two length.
// Forward transform uses exp(-2*pi*i*k/n). Inverse transform uses the opposite
// sign and divides every output by n. This is floating-point numerical output,
// not exact algebraic output.
inline void fast_fourier_transform(std::span<Complex64> values, bool inverse) {
  const std::size_t n = values.size();
  if (n == 0U) {
    return;
  }
  if (!detail::is_power_of_two(n)) {
    throw std::invalid_argument("FFT length must be a power of two");
  }
  detail::validate_finite(std::span<const Complex64>(values.data(), n));

  for (std::size_t index = 1U, reversed = 0U; index < n; ++index) {
    std::size_t bit = n >> 1U;
    while ((reversed & bit) != 0U) {
      reversed ^= bit;
      bit >>= 1U;
    }
    reversed ^= bit;
    if (index < reversed) {
      std::swap(values[index], values[reversed]);
    }
  }

  for (std::size_t length = 2U; length <= n;) {
    const double angle = (inverse ? 2.0 : -2.0) * std::numbers::pi_v<double> /
                         static_cast<double>(length);
    const Complex64 root(std::cos(angle), std::sin(angle));
    const std::size_t half = length / 2U;

    for (std::size_t begin = 0U; begin < n; begin += length) {
      Complex64 omega(1.0, 0.0);
      for (std::size_t offset = 0U; offset < half; ++offset) {
        const Complex64 even = values[begin + offset];
        const Complex64 odd = values[begin + offset + half] * omega;
        values[begin + offset] = even + odd;
        values[begin + offset + half] = even - odd;
        omega *= root;
      }
    }

    if (length == n) {
      break;
    }
    length *= 2U;
  }

  if (inverse) {
    const double divisor = static_cast<double>(n);
    for (auto& value : values) {
      value /= divisor;
    }
  }
}

// Floating-point real convolution. Empty input yields empty output. Values must
// be finite. The result is approximate and inherits IEEE-754 double roundoff;
// callers needing exact integer/modular convolution should use the sealed NTT/
// CRT surfaces instead.
[[nodiscard]] inline std::vector<double> fft_real_convolution(
    std::span<const double> left, std::span<const double> right) {
  if (left.empty() || right.empty()) {
    return {};
  }
  for (const double value : left) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("FFT convolution input must be finite");
    }
  }
  for (const double value : right) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("FFT convolution input must be finite");
    }
  }

  if (left.size() > std::numeric_limits<std::size_t>::max() - right.size() + 1U) {
    throw std::length_error("FFT convolution result size is not representable");
  }
  const std::size_t result_size = left.size() + right.size() - 1U;
  const std::size_t transform_size = detail::next_power_of_two(result_size);

  std::vector<Complex64> left_values(transform_size);
  std::vector<Complex64> right_values(transform_size);
  for (std::size_t index = 0U; index < left.size(); ++index) {
    left_values[index] = Complex64(left[index], 0.0);
  }
  for (std::size_t index = 0U; index < right.size(); ++index) {
    right_values[index] = Complex64(right[index], 0.0);
  }

  fast_fourier_transform(left_values, false);
  fast_fourier_transform(right_values, false);
  for (std::size_t index = 0U; index < transform_size; ++index) {
    left_values[index] *= right_values[index];
  }
  fast_fourier_transform(left_values, true);

  std::vector<double> result(result_size);
  for (std::size_t index = 0U; index < result_size; ++index) {
    result[index] = left_values[index].real();
  }
  return result;
}

}  // namespace algorithms::numerical
