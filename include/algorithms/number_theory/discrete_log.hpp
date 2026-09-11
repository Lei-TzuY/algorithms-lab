#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace algorithms::number_theory {

// Finds the smallest non-negative exponent x such that base^x == target
// (mod prime_modulus) inside the non-zero prime-field multiplicative group.
//
// The search is exact, but its O(sqrt(p)) baby table is explicitly bounded by
// max_baby_steps. Non-trivial instances whose required table exceeds that bound
// fail with std::length_error rather than attempting an unbounded allocation.
[[nodiscard]] std::optional<std::uint64_t> prime_discrete_log_bsgs(
    std::uint64_t base, std::uint64_t target, std::uint64_t prime_modulus,
    std::size_t max_baby_steps = 1'000'000U);

}  // namespace algorithms::number_theory
