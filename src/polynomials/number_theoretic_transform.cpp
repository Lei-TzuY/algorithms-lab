#include "algorithms/polynomials/number_theoretic_transform.hpp"

#include "algorithms/number_theory/modular.hpp"
#include "algorithms/polynomials/exact_convolution.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::polynomials {
namespace {

struct NttParameters {
  std::uint64_t modulus;
  std::uint64_t primitive_root;
  std::size_t maximum_size;
};

constexpr NttParameters kPrimaryNtt{kNttModulus, kNttPrimitiveRoot,
                                    kMaximumNttSize};
constexpr NttParameters kSecondaryNtt{1004535809U, 3U,
                                      kMaximumExactConvolutionNttSize};
constexpr std::uint64_t kCrtModulus =
    kPrimaryNtt.modulus * kSecondaryNtt.modulus;
static_assert(kCrtModulus == 1002772198720536577ULL);
static_assert(kExactConvolutionCenteredLimit == kCrtModulus / 2U);

std::uint64_t multiply_transform_modulus(std::uint64_t left,
                                         std::uint64_t right,
                                         std::uint64_t modulus) noexcept {
  // Both supported transform primes are < 1.01e9, and both operands are
  // reduced below their modulus, so the product is < 1.021e18 < UINT64_MAX.
  return (left * right) % modulus;
}

void validate_transform_size(std::size_t size,
                             const NttParameters& parameters) {
  if (size == 0U) {
    return;
  }
  if ((size & (size - 1U)) != 0U) {
    throw std::invalid_argument("NTT size must be a power of two");
  }
  if (size > parameters.maximum_size) {
    throw std::length_error("NTT size exceeds the root-of-unity limit");
  }
}

void transform(std::vector<std::uint64_t>& values, bool inverse,
               const NttParameters& parameters) {
  const std::size_t size = values.size();
  validate_transform_size(size, parameters);
  if (size == 0U) {
    return;
  }
  for (auto& value : values) {
    value %= parameters.modulus;
  }

  for (std::size_t index = 1U, reversed = 0U; index < size; ++index) {
    std::size_t bit = size >> 1U;
    while ((reversed & bit) != 0U) {
      reversed ^= bit;
      bit >>= 1U;
    }
    reversed ^= bit;
    if (index < reversed) {
      std::swap(values[index], values[reversed]);
    }
  }

  for (std::size_t length = 2U; length <= size; length <<= 1U) {
    const std::uint64_t forward_exponent =
        (parameters.modulus - 1U) / static_cast<std::uint64_t>(length);
    const std::uint64_t exponent = inverse
                                       ? (parameters.modulus - 1U) -
                                             forward_exponent
                                       : forward_exponent;
    const std::uint64_t stage_root = algorithms::number_theory::power_mod(
        parameters.primitive_root, exponent, parameters.modulus);
    const std::size_t half = length / 2U;

    for (std::size_t block = 0U; block < size; block += length) {
      std::uint64_t root_power = 1U;
      for (std::size_t offset = 0U; offset < half; ++offset) {
        const std::uint64_t even = values[block + offset];
        const std::uint64_t odd = multiply_transform_modulus(
            values[block + offset + half], root_power, parameters.modulus);
        const std::uint64_t sum = even + odd;
        values[block + offset] = sum >= parameters.modulus
                                     ? sum - parameters.modulus
                                     : sum;
        values[block + offset + half] =
            even >= odd ? even - odd : even + parameters.modulus - odd;
        root_power = multiply_transform_modulus(
            root_power, stage_root, parameters.modulus);
      }
    }

    if (length == size) {
      break;
    }
  }

  if (inverse) {
    const std::uint64_t inverse_size = algorithms::number_theory::power_mod(
        static_cast<std::uint64_t>(size), parameters.modulus - 2U,
        parameters.modulus);
    for (auto& value : values) {
      value = multiply_transform_modulus(value, inverse_size,
                                         parameters.modulus);
    }
  }
}

std::size_t convolution_result_size(std::size_t left_size,
                                    std::size_t right_size) {
  const std::size_t maximum = std::numeric_limits<std::size_t>::max();
  if (left_size - 1U > maximum - right_size) {
    throw std::length_error("convolution result size overflows size_t");
  }
  return (left_size - 1U) + right_size;
}

std::size_t padded_transform_size(std::size_t result_size,
                                  std::size_t maximum_size) {
  if (result_size > maximum_size) {
    throw std::length_error("convolution exceeds the NTT size limit");
  }
  std::size_t transform_size = 1U;
  while (transform_size < result_size) {
    transform_size <<= 1U;
  }
  return transform_size;
}

std::uint64_t signed_residue(std::int64_t value,
                             std::uint64_t modulus) noexcept {
  if (value >= 0) {
    return static_cast<std::uint64_t>(value) % modulus;
  }
  const std::uint64_t magnitude =
      static_cast<std::uint64_t>(-(value + 1)) + 1U;
  const std::uint64_t remainder = magnitude % modulus;
  return remainder == 0U ? 0U : modulus - remainder;
}

std::uint64_t absolute_magnitude(std::int64_t value) noexcept {
  if (value >= 0) {
    return static_cast<std::uint64_t>(value);
  }
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

std::uint64_t maximum_magnitude(
    const std::vector<std::int64_t>& values) noexcept {
  std::uint64_t maximum = 0U;
  for (const std::int64_t value : values) {
    maximum = std::max(maximum, absolute_magnitude(value));
  }
  return maximum;
}

bool product_within_limit(std::uint64_t first, std::uint64_t second,
                          std::uint64_t third,
                          std::uint64_t limit) noexcept {
  if (first == 0U || second == 0U || third == 0U) {
    return true;
  }
  if (first > limit / second) {
    return false;
  }
  const std::uint64_t product = first * second;
  return third <= limit / product;
}

std::vector<std::uint64_t> convolution_signed_mod(
    const std::vector<std::int64_t>& left,
    const std::vector<std::int64_t>& right, std::size_t result_size,
    std::size_t transform_size, const NttParameters& parameters) {
  std::vector<std::uint64_t> transformed_left(transform_size, 0U);
  std::vector<std::uint64_t> transformed_right(transform_size, 0U);
  for (std::size_t index = 0U; index < left.size(); ++index) {
    transformed_left[index] = signed_residue(left[index], parameters.modulus);
  }
  for (std::size_t index = 0U; index < right.size(); ++index) {
    transformed_right[index] =
        signed_residue(right[index], parameters.modulus);
  }

  transform(transformed_left, false, parameters);
  transform(transformed_right, false, parameters);
  for (std::size_t index = 0U; index < transform_size; ++index) {
    transformed_left[index] = multiply_transform_modulus(
        transformed_left[index], transformed_right[index], parameters.modulus);
  }
  transform(transformed_left, true, parameters);
  transformed_left.resize(result_size);
  return transformed_left;
}

std::int64_t reconstruct_centered(std::uint64_t primary_residue,
                                  std::uint64_t secondary_residue) {
  static const std::uint64_t inverse_primary_mod_secondary =
      algorithms::number_theory::power_mod(
          kPrimaryNtt.modulus % kSecondaryNtt.modulus,
          kSecondaryNtt.modulus - 2U, kSecondaryNtt.modulus);

  const std::uint64_t primary_mod_secondary =
      primary_residue % kSecondaryNtt.modulus;
  const std::uint64_t delta =
      secondary_residue >= primary_mod_secondary
          ? secondary_residue - primary_mod_secondary
          : secondary_residue + kSecondaryNtt.modulus -
                primary_mod_secondary;
  const std::uint64_t multiplier = multiply_transform_modulus(
      delta, inverse_primary_mod_secondary, kSecondaryNtt.modulus);
  const std::uint64_t combined =
      primary_residue + kPrimaryNtt.modulus * multiplier;

  if (combined <= kExactConvolutionCenteredLimit) {
    return static_cast<std::int64_t>(combined);
  }
  const std::uint64_t magnitude = kCrtModulus - combined;
  return -static_cast<std::int64_t>(magnitude);
}

}  // namespace

void number_theoretic_transform(std::vector<std::uint64_t>& values,
                                bool inverse) {
  transform(values, inverse, kPrimaryNtt);
}

std::vector<std::uint64_t> convolution_mod_998244353(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }

  const std::size_t result_size =
      convolution_result_size(left.size(), right.size());
  const std::size_t transform_size =
      padded_transform_size(result_size, kPrimaryNtt.maximum_size);

  std::vector<std::uint64_t> transformed_left(transform_size, 0U);
  std::vector<std::uint64_t> transformed_right(transform_size, 0U);
  for (std::size_t index = 0U; index < left.size(); ++index) {
    transformed_left[index] = left[index] % kPrimaryNtt.modulus;
  }
  for (std::size_t index = 0U; index < right.size(); ++index) {
    transformed_right[index] = right[index] % kPrimaryNtt.modulus;
  }

  transform(transformed_left, false, kPrimaryNtt);
  transform(transformed_right, false, kPrimaryNtt);
  for (std::size_t index = 0U; index < transform_size; ++index) {
    transformed_left[index] = multiply_transform_modulus(
        transformed_left[index], transformed_right[index],
        kPrimaryNtt.modulus);
  }
  transform(transformed_left, true, kPrimaryNtt);
  transformed_left.resize(result_size);
  return transformed_left;
}

std::vector<std::int64_t> convolution_exact_int64(
    const std::vector<std::int64_t>& left,
    const std::vector<std::int64_t>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }

  const std::size_t result_size =
      convolution_result_size(left.size(), right.size());
  const std::size_t transform_size =
      padded_transform_size(result_size, kSecondaryNtt.maximum_size);

  const std::uint64_t overlap = static_cast<std::uint64_t>(
      std::min(left.size(), right.size()));
  const std::uint64_t left_maximum = maximum_magnitude(left);
  const std::uint64_t right_maximum = maximum_magnitude(right);
  if (!product_within_limit(overlap, left_maximum, right_maximum,
                            kExactConvolutionCenteredLimit)) {
    throw std::overflow_error(
        "exact convolution coefficient bound exceeds the CRT range");
  }

  const auto primary = convolution_signed_mod(
      left, right, result_size, transform_size, kPrimaryNtt);
  const auto secondary = convolution_signed_mod(
      left, right, result_size, transform_size, kSecondaryNtt);

  std::vector<std::int64_t> result(result_size, 0);
  for (std::size_t index = 0U; index < result_size; ++index) {
    result[index] = reconstruct_centered(primary[index], secondary[index]);
  }
  return result;
}

}  // namespace algorithms::polynomials
