#include "algorithms/number_theory/discrete_log.hpp"

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::number_theory {
namespace {

std::uint64_t ceil_sqrt(std::uint64_t value) {
  if (value <= 1U) {
    return value;
  }

  std::uint64_t low = 1U;
  std::uint64_t high = std::uint64_t{1} << 32U;
  while (low < high) {
    const std::uint64_t middle = low + ((high - low) / 2U);
    const std::uint64_t quotient = value / middle;
    const std::uint64_t remainder = value % middle;
    const std::uint64_t rounded_up = quotient + (remainder == 0U ? 0U : 1U);
    if (middle >= rounded_up) {
      high = middle;
    } else {
      low = middle + 1U;
    }
  }
  return low;
}

}  // namespace

std::optional<std::uint64_t> prime_discrete_log_bsgs(
    std::uint64_t base, std::uint64_t target, std::uint64_t prime_modulus,
    std::size_t max_baby_steps) {
  if (!is_prime(prime_modulus)) {
    throw std::invalid_argument("discrete log modulus must be prime");
  }
  if (base == 0U || base >= prime_modulus || target == 0U ||
      target >= prime_modulus) {
    throw std::invalid_argument(
        "discrete log base and target must be non-zero field residues");
  }
  if (max_baby_steps == 0U) {
    throw std::invalid_argument("max_baby_steps must be positive");
  }

  if (target == 1U) {
    return std::uint64_t{0};
  }
  if (base == target) {
    return std::uint64_t{1};
  }
  if (base == 1U) {
    return std::nullopt;
  }

  const std::uint64_t group_order = prime_modulus - 1U;
  const std::uint64_t baby_count = ceil_sqrt(group_order);
  if (baby_count > static_cast<std::uint64_t>(max_baby_steps) ||
      baby_count >
          static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    throw std::length_error("discrete log baby-step table exceeds resource cap");
  }

  std::vector<std::pair<std::uint64_t, std::uint64_t>> baby_steps;
  baby_steps.reserve(static_cast<std::size_t>(baby_count));

  std::uint64_t value = 1U;
  for (std::uint64_t exponent = 0U; exponent < baby_count; ++exponent) {
    baby_steps.emplace_back(value, exponent);
    value = multiply_mod(value, base, prime_modulus);
  }
  std::sort(baby_steps.begin(), baby_steps.end());

  const std::uint64_t inverse = power_mod(base, prime_modulus - 2U,
                                          prime_modulus);
  const std::uint64_t giant_factor =
      power_mod(inverse, baby_count, prime_modulus);

  std::uint64_t gamma = target;
  const std::uint64_t max_exponent = group_order - 1U;
  const std::uint64_t max_giant = max_exponent / baby_count;
  for (std::uint64_t giant = 0U; giant <= max_giant; ++giant) {
    const auto found = std::lower_bound(
        baby_steps.begin(), baby_steps.end(),
        std::pair<std::uint64_t, std::uint64_t>{gamma, 0U});
    if (found != baby_steps.end() && found->first == gamma) {
      const std::uint64_t block_start = giant * baby_count;
      const std::uint64_t baby = found->second;
      if (baby <= max_exponent - block_start) {
        return block_start + baby;
      }
    }
    if (giant != max_giant) {
      gamma = multiply_mod(gamma, giant_factor, prime_modulus);
    }
  }
  return std::nullopt;
}

}  // namespace algorithms::number_theory
