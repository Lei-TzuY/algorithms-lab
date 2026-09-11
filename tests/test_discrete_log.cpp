#include "test_framework.hpp"

#include "algorithms/number_theory/discrete_log.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace {

std::optional<std::uint64_t> brute_discrete_log(std::uint64_t base,
                                                std::uint64_t target,
                                                std::uint64_t prime) {
  std::uint64_t value = 1U;
  for (std::uint64_t exponent = 0U; exponent < prime - 1U; ++exponent) {
    if (value == target) {
      return exponent;
    }
    value = (value * base) % prime;
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE(discrete_log_validation_and_trivial_cases) {
  using algorithms::number_theory::prime_discrete_log_bsgs;
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 3U, 15U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(0U, 1U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(7U, 1U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 0U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 7U, 7U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 3U, 7U, 0U),
                    std::invalid_argument);

  REQUIRE_EQ(prime_discrete_log_bsgs(1U, 1U, 2U),
             std::optional<std::uint64_t>{0U});
  REQUIRE_EQ(prime_discrete_log_bsgs(1U, 1U, 97U),
             std::optional<std::uint64_t>{0U});
  REQUIRE_EQ(prime_discrete_log_bsgs(1U, 2U, 97U), std::nullopt);
  REQUIRE_EQ(prime_discrete_log_bsgs(5U, 5U, 23U),
             std::optional<std::uint64_t>{1U});
}

TEST_CASE(discrete_log_known_solutions_and_non_subgroup_target) {
  using algorithms::number_theory::prime_discrete_log_bsgs;
  REQUIRE_EQ(prime_discrete_log_bsgs(5U, 17U, 23U),
             std::optional<std::uint64_t>{7U});
  REQUIRE_EQ(prime_discrete_log_bsgs(4U, 1U, 23U),
             std::optional<std::uint64_t>{0U});
  REQUIRE_EQ(prime_discrete_log_bsgs(4U, 5U, 23U), std::nullopt);
}

TEST_CASE(discrete_log_resource_cap_and_full_width_validation) {
  using algorithms::number_theory::prime_discrete_log_bsgs;
  constexpr std::uint64_t large_prime = 18446744073709551557ULL;
  REQUIRE_EQ(prime_discrete_log_bsgs(1U, 1U, large_prime, 1U),
             std::optional<std::uint64_t>{0U});
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 3U, large_prime, 1000U),
                    std::length_error);
  REQUIRE_THROWS_AS(prime_discrete_log_bsgs(2U, 3U, 101U, 9U),
                    std::length_error);
}

TEST_CASE(discrete_log_exhaustive_small_prime_differential) {
  using algorithms::number_theory::prime_discrete_log_bsgs;
  constexpr std::array<std::uint64_t, 25> primes{
      2U,  3U,  5U,  7U,  11U, 13U, 17U, 19U, 23U,
      29U, 31U, 37U, 41U, 43U, 47U, 53U, 59U, 61U,
      67U, 71U, 73U, 79U, 83U, 89U, 97U};

  for (const std::uint64_t prime : primes) {
    for (std::uint64_t base = 1U; base < prime; ++base) {
      for (std::uint64_t target = 1U; target < prime; ++target) {
        const auto expected = brute_discrete_log(base, target, prime);
        const auto actual = prime_discrete_log_bsgs(base, target, prime, 32U);
        REQUIRE_EQ(actual, expected);
      }
    }
  }
}
