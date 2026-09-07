#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::polynomials {

inline constexpr std::size_t kMaximumFormalPowerSeriesInverseTerms =
    std::size_t{1} << 22U;

// Returns the first `terms` coefficients of the multiplicative inverse modulo
// x^terms over Z/998244353Z. terms==0 yields an empty result. For terms>0 the
// input must be non-empty and its constant coefficient must be non-zero modulo
// 998244353. Newton doubling delegates polynomial products to the existing NTT
// convolution substrate.
[[nodiscard]] std::vector<std::uint64_t> formal_power_series_inverse(
    const std::vector<std::uint64_t>& series, std::size_t terms);

}  // namespace algorithms::polynomials
