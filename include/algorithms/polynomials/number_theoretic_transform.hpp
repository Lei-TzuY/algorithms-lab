#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::polynomials {

inline constexpr std::uint64_t kNttModulus = 998244353U;
inline constexpr std::uint64_t kNttPrimitiveRoot = 3U;
inline constexpr std::size_t kMaximumNttSize = std::size_t{1} << 23U;

// In-place radix-2 transform over Z/998244353Z. Empty input is a no-op;
// non-empty input must have power-of-two size <= 2^23. Inputs are normalized
// modulo kNttModulus. Inverse transform includes division by the size.
void number_theoretic_transform(std::vector<std::uint64_t>& values,
                                bool inverse);

// Polynomial convolution modulo kNttModulus. Empty input yields an empty
// result. Throws length_error if the required transform exceeds 2^23.
[[nodiscard]] std::vector<std::uint64_t> convolution_mod_998244353(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right);

}  // namespace algorithms::polynomials
