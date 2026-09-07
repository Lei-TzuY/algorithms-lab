#include "algorithms/polynomials/formal_power_series.hpp"

#include "algorithms/number_theory/modular.hpp"
#include "algorithms/polynomials/number_theoretic_transform.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace algorithms::polynomials {

std::vector<std::uint64_t> formal_power_series_inverse(
    const std::vector<std::uint64_t>& series, std::size_t terms) {
  if (terms == 0U) {
    return {};
  }
  if (terms > kMaximumFormalPowerSeriesInverseTerms) {
    throw std::length_error("formal power series inverse exceeds NTT limit");
  }
  if (series.empty()) {
    throw std::invalid_argument("formal power series must have a constant term");
  }

  const std::uint64_t constant = series[0] % kNttModulus;
  if (constant == 0U) {
    throw std::invalid_argument(
        "formal power series constant term must be invertible");
  }

  std::vector<std::uint64_t> inverse{
      algorithms::number_theory::power_mod(constant, kNttModulus - 2U,
                                            kNttModulus)};

  while (inverse.size() < terms) {
    const std::size_t doubled = inverse.size() * 2U;
    const std::size_t target = std::min(doubled, terms);
    const std::size_t prefix_size = std::min(series.size(), target);

    std::vector<std::uint64_t> prefix(prefix_size, 0U);
    for (std::size_t index = 0U; index < prefix_size; ++index) {
      prefix[index] = series[index] % kNttModulus;
    }

    auto product = convolution_mod_998244353(prefix, inverse);
    product.resize(target, 0U);

    std::vector<std::uint64_t> correction(target, 0U);
    correction[0] =
        product[0] <= 2U ? 2U - product[0] : kNttModulus + 2U - product[0];
    correction[0] %= kNttModulus;
    for (std::size_t index = 1U; index < target; ++index) {
      correction[index] =
          product[index] == 0U ? 0U : kNttModulus - product[index];
    }

    inverse = convolution_mod_998244353(inverse, correction);
    inverse.resize(target);
  }

  return inverse;
}

}  // namespace algorithms::polynomials
