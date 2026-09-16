#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::number_theory {

struct PellSolution {
  std::uint64_t x = 0;
  std::uint64_t y = 0;
  std::size_t period_length = 0;
  std::size_t convergent_index = 0;

  friend bool operator==(const PellSolution&, const PellSolution&) = default;
};

namespace pell_detail {

inline std::uint64_t floor_sqrt(std::uint64_t value) {
  std::uint64_t low = 0;
  std::uint64_t high = value + 1;
  while (low + 1 < high) {
    const std::uint64_t mid = low + (high - low) / 2;
    if (mid == 0 || mid <= value / mid) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return low;
}

inline std::uint64_t checked_mul_add(std::uint64_t first,
                                     std::uint64_t second,
                                     std::uint64_t addend) {
  constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  if (first != 0 && second > (maximum - addend) / first) {
    throw std::overflow_error("Pell fundamental solution exceeds uint64_t");
  }
  return first * second + addend;
}

}  // namespace pell_detail

// Return the fundamental positive solution (x,y), y>0, of x^2-D*y^2=1.
//
// Contract:
// - 2 <= D <= 1e9;
// - D must not be a perfect square;
// - if the mathematical fundamental x or y does not fit uint64_t, fail closed
//   with std::overflow_error rather than returning a truncated value.
//
// The implementation uses the periodic simple continued fraction of sqrt(D).
// For period length L, the fundamental solution is convergent L-1 when L is
// even and convergent 2L-1 when L is odd (zero-based, including a0).
inline PellSolution fundamental_pell_solution(std::uint64_t D) {
  constexpr std::uint64_t max_D = 1'000'000'000ULL;
  if (D < 2 || D > max_D) {
    throw std::invalid_argument("Pell D must lie in [2, 1e9]");
  }

  const std::uint64_t a0 = pell_detail::floor_sqrt(D);
  if (a0 * a0 == D) {
    throw std::invalid_argument("Pell D must be nonsquare");
  }

  std::vector<std::uint64_t> period;
  std::uint64_t m = 0;
  std::uint64_t denominator = 1;
  std::uint64_t a = a0;
  do {
    m = denominator * a - m;
    denominator = (D - m * m) / denominator;
    a = (a0 + m) / denominator;
    period.push_back(a);
  } while (a != 2 * a0);

  const std::size_t L = period.size();
  const std::size_t target_index = (L % 2 == 0) ? (L - 1) : (2 * L - 1);

  // Standard convergent recurrence:
  // p[-2]=0, p[-1]=1; q[-2]=1, q[-1]=0.
  std::uint64_t p_nm2 = 0;
  std::uint64_t p_nm1 = 1;
  std::uint64_t q_nm2 = 1;
  std::uint64_t q_nm1 = 0;

  std::uint64_t p = 0;
  std::uint64_t q = 0;
  for (std::size_t index = 0; index <= target_index; ++index) {
    const std::uint64_t coefficient =
        index == 0 ? a0 : period[(index - 1) % L];
    p = pell_detail::checked_mul_add(coefficient, p_nm1, p_nm2);
    q = pell_detail::checked_mul_add(coefficient, q_nm1, q_nm2);
    p_nm2 = p_nm1;
    p_nm1 = p;
    q_nm2 = q_nm1;
    q_nm1 = q;
  }

  return PellSolution{p, q, L, target_index};
}

}  // namespace algorithms::number_theory
