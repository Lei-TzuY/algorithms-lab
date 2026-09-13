#pragma once

#include "algorithms/number_theory/prime_counting.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace prime_counting_test_detail {

inline algorithms::number_theory::LehmerPrimeCounter& counter() {
  static algorithms::number_theory::LehmerPrimeCounter value;
  return value;
}

inline std::vector<std::uint32_t> sieve_primes(std::size_t limit) {
  std::vector<bool> is_prime(limit + 1U, true);
  if (!is_prime.empty()) {
    is_prime[0] = false;
  }
  if (limit >= 1U) {
    is_prime[1] = false;
  }
  for (std::size_t prime = 2U; prime <= limit / prime; ++prime) {
    if (!is_prime[prime]) {
      continue;
    }
    for (std::size_t composite = prime * prime; composite <= limit;
         composite += prime) {
      is_prime[composite] = false;
    }
  }
  std::vector<std::uint32_t> primes;
  for (std::size_t value = 2U; value <= limit; ++value) {
    if (is_prime[value]) {
      primes.push_back(static_cast<std::uint32_t>(value));
    }
  }
  return primes;
}

inline std::uint64_t oracle_count(const std::vector<std::uint32_t>& primes,
                                  std::uint64_t value) {
  return static_cast<std::uint64_t>(
      std::upper_bound(primes.begin(), primes.end(), value) - primes.begin());
}

}  // namespace prime_counting_test_detail

TEST_CASE(lehmer_prime_counting_known_values) {
  auto& counter = prime_counting_test_detail::counter();
  const std::vector<std::pair<std::uint64_t, std::uint64_t>> known = {
      {0ULL, 0ULL},
      {1ULL, 0ULL},
      {2ULL, 1ULL},
      {10ULL, 4ULL},
      {100ULL, 25ULL},
      {1'000ULL, 168ULL},
      {10'000ULL, 1'229ULL},
      {100'000ULL, 9'592ULL},
      {1'000'000ULL, 78'498ULL},
      {10'000'000ULL, 664'579ULL},
      {100'000'000ULL, 5'761'455ULL},
      {1'000'000'000ULL, 50'847'534ULL},
      {10'000'000'000ULL, 455'052'511ULL},
      {100'000'000'000ULL, 4'118'054'813ULL},
  };
  for (const auto& [input, expected] : known) {
    REQUIRE_EQ(counter.count(input), expected);
  }
}

TEST_CASE(lehmer_prime_counting_rejects_beyond_supported_bound) {
  auto& counter = prime_counting_test_detail::counter();
  REQUIRE_EQ(counter.max_supported_input(), 100'000'000'000ULL);
  REQUIRE_THROWS_AS(counter.count(counter.max_supported_input() + 1ULL),
                    std::out_of_range);
}

TEST_CASE(lehmer_prime_counting_randomized_against_independent_sieve) {
  auto& counter = prime_counting_test_detail::counter();
  constexpr std::size_t kOracleLimit = 10'000'000U;
  const auto primes = prime_counting_test_detail::sieve_primes(kOracleLimit);

  std::mt19937_64 random(0xA11CEB00CULL);
  std::uniform_int_distribution<std::uint64_t> input_distribution(
      400'001ULL, static_cast<std::uint64_t>(kOracleLimit));
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::uint64_t input = input_distribution(random);
    REQUIRE_EQ(counter.count(input),
               prime_counting_test_detail::oracle_count(primes, input));
  }
}

TEST_CASE(lehmer_prime_counting_repeated_queries_are_stable) {
  auto& counter = prime_counting_test_detail::counter();
  const std::uint64_t first = counter.count(9'999'999'967ULL);
  const std::uint64_t second = counter.count(9'999'999'967ULL);
  REQUIRE_EQ(first, second);
  REQUIRE(first <= 9'999'999'967ULL);
}
