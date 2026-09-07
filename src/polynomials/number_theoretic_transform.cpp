#include "algorithms/polynomials/number_theoretic_transform.hpp"

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::polynomials {
namespace {

std::uint64_t multiply_fixed_modulus(std::uint64_t left,
                                     std::uint64_t right) noexcept {
  // Both factors are always below 998244353, so the product is < 1e18 and is
  // representable in uint64_t before reduction.
  return (left * right) % kNttModulus;
}

void validate_transform_size(std::size_t size) {
  if (size == 0U) {
    return;
  }
  if ((size & (size - 1U)) != 0U) {
    throw std::invalid_argument("NTT size must be a power of two");
  }
  if (size > kMaximumNttSize) {
    throw std::length_error("NTT size exceeds the 2^23 root-of-unity limit");
  }
}

}  // namespace

void number_theoretic_transform(std::vector<std::uint64_t>& values,
                                bool inverse) {
  const std::size_t size = values.size();
  validate_transform_size(size);
  if (size == 0U) {
    return;
  }
  for (auto& value : values) {
    value %= kNttModulus;
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
        (kNttModulus - 1U) / static_cast<std::uint64_t>(length);
    const std::uint64_t exponent =
        inverse ? (kNttModulus - 1U) - forward_exponent : forward_exponent;
    const std::uint64_t stage_root = algorithms::number_theory::power_mod(
        kNttPrimitiveRoot, exponent, kNttModulus);
    const std::size_t half = length / 2U;

    for (std::size_t block = 0U; block < size; block += length) {
      std::uint64_t root_power = 1U;
      for (std::size_t offset = 0U; offset < half; ++offset) {
        const std::uint64_t even = values[block + offset];
        const std::uint64_t odd = multiply_fixed_modulus(
            values[block + offset + half], root_power);
        const std::uint64_t sum = even + odd;
        values[block + offset] =
            sum >= kNttModulus ? sum - kNttModulus : sum;
        values[block + offset + half] =
            even >= odd ? even - odd : even + kNttModulus - odd;
        root_power = multiply_fixed_modulus(root_power, stage_root);
      }
    }

    if (length == size) {
      break;
    }
  }

  if (inverse) {
    const std::uint64_t inverse_size = algorithms::number_theory::power_mod(
        static_cast<std::uint64_t>(size), kNttModulus - 2U, kNttModulus);
    for (auto& value : values) {
      value = multiply_fixed_modulus(value, inverse_size);
    }
  }
}

std::vector<std::uint64_t> convolution_mod_998244353(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }

  const std::size_t maximum = std::numeric_limits<std::size_t>::max();
  if (left.size() - 1U > maximum - right.size()) {
    throw std::length_error("convolution result size overflows size_t");
  }
  const std::size_t result_size = (left.size() - 1U) + right.size();
  if (result_size > kMaximumNttSize) {
    throw std::length_error("convolution requires an NTT larger than 2^23");
  }

  std::size_t transform_size = 1U;
  while (transform_size < result_size) {
    transform_size <<= 1U;
  }

  std::vector<std::uint64_t> transformed_left(transform_size, 0U);
  std::vector<std::uint64_t> transformed_right(transform_size, 0U);
  for (std::size_t index = 0U; index < left.size(); ++index) {
    transformed_left[index] = left[index] % kNttModulus;
  }
  for (std::size_t index = 0U; index < right.size(); ++index) {
    transformed_right[index] = right[index] % kNttModulus;
  }

  number_theoretic_transform(transformed_left, false);
  number_theoretic_transform(transformed_right, false);
  for (std::size_t index = 0U; index < transform_size; ++index) {
    transformed_left[index] = multiply_fixed_modulus(
        transformed_left[index], transformed_right[index]);
  }
  number_theoretic_transform(transformed_left, true);
  transformed_left.resize(result_size);
  return transformed_left;
}

}  // namespace algorithms::polynomials
