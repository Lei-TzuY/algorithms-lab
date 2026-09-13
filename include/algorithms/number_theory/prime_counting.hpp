#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace algorithms::number_theory {

// Reusable bounded-exact prime-counting index using Lehmer's combinatorial
// decomposition. Construction precomputes a small ordinary sieve and a compact
// phi(x, a) table; queries above the sieve use only exact integer arithmetic
// after corrected floating-point root estimates.
class LehmerPrimeCounter {
 public:
  static constexpr std::uint64_t kMaxSupportedInput = 100'000'000'000ULL;

  LehmerPrimeCounter() { build_tables(); }

  [[nodiscard]] std::uint64_t count(std::uint64_t value) const {
    if (value > kMaxSupportedInput) {
      throw std::out_of_range("LehmerPrimeCounter input exceeds supported bound");
    }
    lehmer_cache_.clear();
    return lehmer_count(value);
  }

  [[nodiscard]] static constexpr std::uint64_t max_supported_input() noexcept {
    return kMaxSupportedInput;
  }

 private:
  static constexpr std::size_t kSieveLimit = 400'001U;
  static constexpr std::size_t kPhiXLimit = 25'000U;
  static constexpr std::size_t kPhiPrimeCount = 100U;

  std::vector<std::uint32_t> primes_;
  std::vector<std::uint32_t> prime_prefix_;
  std::vector<std::uint32_t> phi_table_;
  mutable std::unordered_map<std::uint64_t, std::uint64_t> lehmer_cache_;

  void build_tables() {
    std::vector<bool> is_prime(kSieveLimit, true);
    is_prime[0] = false;
    is_prime[1] = false;
    prime_prefix_.assign(kSieveLimit, 0U);

    for (std::size_t value = 2U; value < kSieveLimit; ++value) {
      if (is_prime[value]) {
        primes_.push_back(static_cast<std::uint32_t>(value));
        if (value <= (kSieveLimit - 1U) / value) {
          for (std::size_t composite = value * value;
               composite < kSieveLimit; composite += value) {
            is_prime[composite] = false;
          }
        }
      }
      prime_prefix_[value] =
          prime_prefix_[value - 1U] + (is_prime[value] ? 1U : 0U);
    }

    if (primes_.size() < kPhiPrimeCount) {
      throw std::logic_error("prime-counting sieve is too small for phi table");
    }

    phi_table_.resize((kPhiPrimeCount + 1U) * kPhiXLimit);
    for (std::size_t value = 0U; value < kPhiXLimit; ++value) {
      phi_table_[value] = static_cast<std::uint32_t>(value);
    }
    for (std::size_t prime_count = 1U; prime_count <= kPhiPrimeCount;
         ++prime_count) {
      const std::size_t prime = primes_[prime_count - 1U];
      const std::size_t current_offset = prime_count * kPhiXLimit;
      const std::size_t previous_offset = (prime_count - 1U) * kPhiXLimit;
      for (std::size_t value = 0U; value < kPhiXLimit; ++value) {
        phi_table_[current_offset + value] =
            phi_table_[previous_offset + value] -
            phi_table_[previous_offset + value / prime];
      }
    }
    lehmer_cache_.reserve(4096U);
  }

  [[nodiscard]] std::uint64_t phi(std::uint64_t value,
                                  std::size_t prime_count) const {
    if (prime_count == 0U) {
      return value;
    }
    if (prime_count <= kPhiPrimeCount && value < kPhiXLimit) {
      return phi_table_[prime_count * kPhiXLimit +
                        static_cast<std::size_t>(value)];
    }
    return phi(value, prime_count - 1U) -
           phi(value / primes_[prime_count - 1U], prime_count - 1U);
  }

  [[nodiscard]] static bool square_leq(std::uint64_t root,
                                       std::uint64_t value) noexcept {
    return root == 0U || root <= value / root;
  }

  [[nodiscard]] static bool cube_leq(std::uint64_t root,
                                     std::uint64_t value) noexcept {
    return root == 0U || root <= (value / root) / root;
  }

  [[nodiscard]] static bool fourth_power_leq(std::uint64_t root,
                                             std::uint64_t value) noexcept {
    if (root == 0U) {
      return true;
    }
    if (root > value / root) {
      return false;
    }
    const std::uint64_t square = root * root;
    return square <= value / square;
  }

  [[nodiscard]] static std::uint64_t integer_sqrt(std::uint64_t value) {
    std::uint64_t root =
        static_cast<std::uint64_t>(std::sqrt(static_cast<long double>(value)));
    while (root < std::numeric_limits<std::uint64_t>::max() &&
           square_leq(root + 1U, value)) {
      ++root;
    }
    while (!square_leq(root, value)) {
      --root;
    }
    return root;
  }

  [[nodiscard]] static std::uint64_t integer_cuberoot(std::uint64_t value) {
    std::uint64_t root =
        static_cast<std::uint64_t>(std::cbrt(static_cast<long double>(value)));
    while (root < std::numeric_limits<std::uint64_t>::max() &&
           cube_leq(root + 1U, value)) {
      ++root;
    }
    while (!cube_leq(root, value)) {
      --root;
    }
    return root;
  }

  [[nodiscard]] static std::uint64_t integer_fourth_root(std::uint64_t value) {
    std::uint64_t root = integer_sqrt(integer_sqrt(value));
    while (root < std::numeric_limits<std::uint64_t>::max() &&
           fourth_power_leq(root + 1U, value)) {
      ++root;
    }
    while (!fourth_power_leq(root, value)) {
      --root;
    }
    return root;
  }

  [[nodiscard]] std::uint64_t lehmer_count(std::uint64_t value) const {
    if (value < kSieveLimit) {
      return prime_prefix_[static_cast<std::size_t>(value)];
    }
    if (const auto found = lehmer_cache_.find(value);
        found != lehmer_cache_.end()) {
      return found->second;
    }

    const std::uint64_t a = lehmer_count(integer_fourth_root(value));
    const std::uint64_t b = lehmer_count(integer_sqrt(value));
    const std::uint64_t c = lehmer_count(integer_cuberoot(value));

    std::uint64_t result =
        phi(value, static_cast<std::size_t>(a)) +
        ((b + a - 2U) * (b - a + 1U)) / 2U;

    for (std::uint64_t index = a; index < b; ++index) {
      const std::uint64_t quotient =
          value / primes_[static_cast<std::size_t>(index)];
      result -= lehmer_count(quotient);
      if (index < c) {
        const std::uint64_t limit = lehmer_count(integer_sqrt(quotient));
        for (std::uint64_t inner = index; inner < limit; ++inner) {
          result -=
              lehmer_count(quotient /
                           primes_[static_cast<std::size_t>(inner)]) -
              inner;
        }
      }
    }

    lehmer_cache_.emplace(value, result);
    return result;
  }
};

}  // namespace algorithms::number_theory
