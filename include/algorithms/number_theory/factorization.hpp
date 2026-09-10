#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::number_theory {

struct FactorizationResult {
  std::vector<std::uint64_t> prime_factors;
  std::uint64_t seed = 0;
  std::size_t rho_polynomials_tried = 0;

  friend bool operator==(const FactorizationResult&,
                         const FactorizationResult&) = default;
};

// Returns the exact prime factorization of value in nondecreasing order.
// Multiplicity is preserved. value == 1 has the empty factorization; value == 0
// is rejected because prime factorization is undefined.
//
// The seed affects only Pollard-Rho's search path, never result correctness.
// The pseudorandom generator and bounded-sampling mapping are repository-defined,
// so a fixed (value, seed) pair is replayable across standard-library versions.
[[nodiscard]] FactorizationResult factorize_uint64(
    std::uint64_t value, std::uint64_t seed = 0x9E3779B97F4A7C15ULL);

}  // namespace algorithms::number_theory
