#pragma once

#include <cstdint>

namespace algorithms::number_theory {

[[nodiscard]] std::uint64_t gcd(std::uint64_t a, std::uint64_t b) noexcept;

// Computes (a * b) mod modulus without overflowing uint64_t. modulus must be
// non-zero. Complexity is O(log b) modular additions/doublings.
[[nodiscard]] std::uint64_t multiply_mod(std::uint64_t a, std::uint64_t b,
                                         std::uint64_t modulus);

// Computes base^exponent mod modulus through overflow-safe multiplication.
// modulus must be non-zero.
[[nodiscard]] std::uint64_t power_mod(std::uint64_t base,
                                      std::uint64_t exponent,
                                      std::uint64_t modulus);

// Deterministic Miller-Rabin primality test for the full uint64_t domain.
[[nodiscard]] bool is_prime(std::uint64_t value);

}  // namespace algorithms::number_theory
