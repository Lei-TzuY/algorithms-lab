#pragma once

#include "algorithms/number_theory/pell_equation.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace pell_test_detail {

inline std::uint64_t floor_sqrt_small(std::uint64_t value) {
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

inline algorithms::number_theory::PellSolution brute_small(std::uint64_t D) {
  for (std::uint64_t y = 1; y <= 100'000; ++y) {
    const std::uint64_t yy = y * y;
    const std::uint64_t rhs = D * yy + 1;
    const std::uint64_t x = floor_sqrt_small(rhs);
    if (x * x == rhs) {
      return algorithms::number_theory::PellSolution{x, y, 0, 0};
    }
  }
  throw std::runtime_error("small Pell brute-force bound exhausted");
}

inline bool is_square(std::uint64_t D) {
  const std::uint64_t root = floor_sqrt_small(D);
  return root * root == D;
}

inline void require_equation_small(const algorithms::number_theory::PellSolution& solution,
                                   std::uint64_t D) {
  REQUIRE(solution.y > 0);
  // All callers of this helper use known values small enough for exact uint64 replay.
  REQUIRE_EQ(solution.x * solution.x - D * solution.y * solution.y,
             std::uint64_t{1});
}

}  // namespace pell_test_detail

TEST_CASE(pell_equation_rejects_invalid_domain_and_squares) {
  using algorithms::number_theory::fundamental_pell_solution;
  REQUIRE_THROWS_AS(fundamental_pell_solution(0), std::invalid_argument);
  REQUIRE_THROWS_AS(fundamental_pell_solution(1), std::invalid_argument);
  REQUIRE_THROWS_AS(fundamental_pell_solution(4), std::invalid_argument);
  REQUIRE_THROWS_AS(fundamental_pell_solution(999'950'884ULL), std::invalid_argument);
  REQUIRE_THROWS_AS(fundamental_pell_solution(1'000'000'001ULL), std::invalid_argument);
}

TEST_CASE(pell_equation_known_fundamental_solutions_and_period_parity) {
  using algorithms::number_theory::PellSolution;
  using algorithms::number_theory::fundamental_pell_solution;
  REQUIRE_EQ(fundamental_pell_solution(2), (PellSolution{3, 2, 1, 1}));
  REQUIRE_EQ(fundamental_pell_solution(3), (PellSolution{2, 1, 2, 1}));
  REQUIRE_EQ(fundamental_pell_solution(5), (PellSolution{9, 4, 1, 1}));
  REQUIRE_EQ(fundamental_pell_solution(6), (PellSolution{5, 2, 2, 1}));
  REQUIRE_EQ(fundamental_pell_solution(7), (PellSolution{8, 3, 4, 3}));
  const auto d13 = fundamental_pell_solution(13);
  REQUIRE_EQ(d13.x, std::uint64_t{649});
  REQUIRE_EQ(d13.y, std::uint64_t{180});
  REQUIRE_EQ(d13.period_length, std::size_t{5});
  REQUIRE_EQ(d13.convergent_index, std::size_t{9});

  const auto d61 = fundamental_pell_solution(61);
  REQUIRE_EQ(d61.x, std::uint64_t{1'766'319'049});
  REQUIRE_EQ(d61.y, std::uint64_t{226'153'980});
  pell_test_detail::require_equation_small(d61, 61);
}

TEST_CASE(pell_equation_matches_independent_small_domain_brute_force) {
  using algorithms::number_theory::fundamental_pell_solution;
  using namespace pell_test_detail;
  for (std::uint64_t D = 2; D <= 50; ++D) {
    if (is_square(D)) {
      continue;
    }
    const auto expected = brute_small(D);
    const auto actual = fundamental_pell_solution(D);
    REQUIRE_EQ(actual.x, expected.x);
    REQUIRE_EQ(actual.y, expected.y);
    REQUIRE(actual.period_length > 0);
    REQUIRE_EQ(actual.convergent_index,
               actual.period_length % 2 == 0
                   ? actual.period_length - 1
                   : 2 * actual.period_length - 1);
  }
}

TEST_CASE(pell_equation_fails_closed_when_fundamental_solution_exceeds_u64) {
  using algorithms::number_theory::fundamental_pell_solution;
  // D=661 has a famous fundamental solution with 38 decimal digits in x,
  // so the exact uint64_t API must reject rather than truncate or wrap.
  REQUIRE_THROWS_AS(fundamental_pell_solution(661), std::overflow_error);
}
