#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorics {

// Construct a deterministic cyclic de Bruijn sequence B(k, n) over symbols
// [0, k). The returned vector contains exactly k^n symbols; cyclic windows of
// length n enumerate every length-n word exactly once.
//
// This uses the Fredricksen-Kessler-Maiorana (FKM) necklace recursion rather
// than constructing an explicit de Bruijn graph.
//
// Preconditions:
//   * alphabet_size >= 1
//   * order >= 1
//
// Throws std::invalid_argument for invalid parameters and std::length_error
// when k^n or the working representation cannot fit in size_t/vector storage.
[[nodiscard]] inline std::vector<std::size_t> de_bruijn_sequence(
    const std::size_t alphabet_size,
    const std::size_t order) {
  if (alphabet_size == 0U) {
    throw std::invalid_argument(
        "de Bruijn alphabet size must be positive");
  }
  if (order == 0U) {
    throw std::invalid_argument(
        "de Bruijn order must be positive");
  }

  if (alphabet_size == 1U) {
    return {0U};
  }

  std::size_t expected_size = 1U;
  for (std::size_t exponent = 0U; exponent < order; ++exponent) {
    if (expected_size >
        std::numeric_limits<std::size_t>::max() / alphabet_size) {
      throw std::length_error(
          "de Bruijn sequence length overflows size_t");
    }
    expected_size *= alphabet_size;
  }

  if (order == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error(
        "de Bruijn order is too large for workspace indexing");
  }

  std::vector<std::size_t> output;
  if (expected_size > output.max_size()) {
    throw std::length_error(
        "de Bruijn sequence exceeds vector capacity");
  }
  output.reserve(expected_size);

  // FKM is conventionally written with 1-based workspace a[1..n], with a[0]
  // as the initial zero sentinel used by the first recursive step.
  std::vector<std::size_t> workspace(order + 1U, 0U);

  const auto generate =
      [&](auto&& self,
          const std::size_t position,
          const std::size_t period) -> void {
        if (position > order) {
          if (order % period == 0U) {
            for (std::size_t index = 1U; index <= period; ++index) {
              output.push_back(workspace[index]);
            }
          }
          return;
        }

        workspace[position] = workspace[position - period];
        self(self, position + 1U, period);

        const std::size_t first_larger =
            workspace[position - period] + 1U;
        for (std::size_t symbol = first_larger;
             symbol < alphabet_size; ++symbol) {
          workspace[position] = symbol;
          self(self, position + 1U, position);
        }
      };

  generate(generate, 1U, 1U);

  if (output.size() != expected_size) {
    throw std::logic_error(
        "FKM de Bruijn construction length invariant violated");
  }
  return output;
}

}  // namespace algorithms::combinatorics
