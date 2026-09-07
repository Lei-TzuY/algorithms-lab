#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::polynomials {

inline constexpr std::size_t kMaximumExactConvolutionNttSize =
    std::size_t{1} << 21U;
inline constexpr std::uint64_t kExactConvolutionCenteredLimit =
    501386099360268288ULL;

// Exact signed integer convolution when the conservative per-coefficient bound
// min(n,m) * max(abs(left)) * max(abs(right)) fits the centered two-prime CRT
// reconstruction range. Empty input yields an empty result. Throws
// length_error if the second NTT prime cannot represent the transform length,
// and overflow_error when the conservative exactness bound is not provable.
[[nodiscard]] std::vector<std::int64_t> convolution_exact_int64(
    const std::vector<std::int64_t>& left,
    const std::vector<std::int64_t>& right);

}  // namespace algorithms::polynomials
