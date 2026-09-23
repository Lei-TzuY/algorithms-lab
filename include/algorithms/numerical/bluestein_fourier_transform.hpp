#pragma once

#include "algorithms/numerical/fast_fourier_transform.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

namespace detail {

[[nodiscard]] inline Complex64 bluestein_chirp(
    const std::size_t index,
    const std::size_t length,
    const bool inverse) {
  if (length == 0U) {
    throw std::logic_error("Bluestein chirp requires nonzero length");
  }

  const long double n = static_cast<long double>(length);
  const long double i = static_cast<long double>(index);
  const long double period = 2.0L * n;
  const long double square_mod = std::fmod(i * i, period);
  const long double sign = inverse ? 1.0L : -1.0L;
  const long double angle =
      sign * std::numbers::pi_v<long double> * square_mod / n;

  return Complex64(
      static_cast<double>(std::cos(angle)),
      static_cast<double>(std::sin(angle)));
}

}  // namespace detail

// In-place arbitrary-length complex DFT using Bluestein's chirp transform.
//
// Empty input is a no-op. Non-empty inputs may have any representable length.
// Forward transform uses exp(-2*pi*i*k/n). Inverse uses the opposite sign and
// divides every output by n, matching fast_fourier_transform.
//
// Power-of-two lengths delegate to the sealed radix-2 FFT. Other lengths are
// reduced to one power-of-two circular convolution of length at least 2*n-1.
//
// This is floating-point numerical output, not exact algebraic output.
inline void bluestein_fourier_transform(
    std::span<Complex64> values,
    const bool inverse) {
  const std::size_t n = values.size();
  if (n == 0U) {
    return;
  }

  detail::validate_finite(
      std::span<const Complex64>(values.data(), values.size()));

  if (detail::is_power_of_two(n)) {
    fast_fourier_transform(values, inverse);
    return;
  }

  const std::size_t maximum =
      std::numeric_limits<std::size_t>::max();
  if (n > maximum / 2U + 1U) {
    throw std::length_error(
        "Bluestein convolution span is not representable");
  }

  const std::size_t convolution_size = 2U * n - 1U;
  const std::size_t transform_size =
      detail::next_power_of_two(convolution_size);

  std::vector<Complex64> signal(transform_size);
  std::vector<Complex64> kernel(transform_size);

  for (std::size_t index = 0U; index < n; ++index) {
    const Complex64 chirp =
        detail::bluestein_chirp(index, n, inverse);
    const Complex64 inverse_chirp = std::conj(chirp);

    signal[index] = values[index] * chirp;
    kernel[index] = inverse_chirp;
    if (index != 0U) {
      kernel[transform_size - index] = inverse_chirp;
    }
  }

  fast_fourier_transform(signal, false);
  fast_fourier_transform(kernel, false);
  for (std::size_t index = 0U; index < transform_size; ++index) {
    signal[index] *= kernel[index];
  }
  fast_fourier_transform(signal, true);

  const double divisor =
      inverse ? static_cast<double>(n) : 1.0;
  for (std::size_t index = 0U; index < n; ++index) {
    const Complex64 chirp =
        detail::bluestein_chirp(index, n, inverse);
    values[index] = signal[index] * chirp / divisor;
  }
}

}  // namespace algorithms::numerical
