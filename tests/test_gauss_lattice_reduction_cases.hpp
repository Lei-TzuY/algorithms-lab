#pragma once

#include "test_framework.hpp"
#include "algorithms/number_theory/gauss_lattice_reduction.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>

namespace {

using algorithms::number_theory::GaussReducedBasis2i;
using algorithms::number_theory::LatticeVector2i;
using algorithms::number_theory::gauss_reduce_lattice_basis_2d;
using algorithms::number_theory::kGaussLatticeCoordinateBound;

std::int64_t oracle_dot(const LatticeVector2i a, const LatticeVector2i b) {
  return a.x * b.x + a.y * b.y;
}
std::int64_t oracle_norm(const LatticeVector2i v) { return oracle_dot(v, v); }
std::int64_t oracle_det(const LatticeVector2i a, const LatticeVector2i b) {
  return a.x * b.y - a.y * b.x;
}
std::int64_t abs_i64(const std::int64_t value) { return value < 0 ? -value : value; }

LatticeVector2i replay_row(const std::int64_t a, const std::int64_t b,
                           const LatticeVector2i first,
                           const LatticeVector2i second) {
  return LatticeVector2i{a * first.x + b * second.x,
                         a * first.y + b * second.y};
}

void require_valid_reduction(const LatticeVector2i original_first,
                             const LatticeVector2i original_second,
                             const GaussReducedBasis2i& reduced) {
  REQUIRE_EQ(replay_row(reduced.transform.first_from_first,
                        reduced.transform.first_from_second, original_first,
                        original_second),
             reduced.first);
  REQUIRE_EQ(replay_row(reduced.transform.second_from_first,
                        reduced.transform.second_from_second, original_first,
                        original_second),
             reduced.second);
  const std::int64_t transform_det =
      reduced.transform.first_from_first * reduced.transform.second_from_second -
      reduced.transform.first_from_second * reduced.transform.second_from_first;
  REQUIRE(abs_i64(transform_det) == 1);
  REQUIRE(abs_i64(oracle_det(reduced.first, reduced.second)) ==
          abs_i64(oracle_det(original_first, original_second)));
  REQUIRE(oracle_norm(reduced.first) <= oracle_norm(reduced.second));
  REQUIRE(2 * abs_i64(oracle_dot(reduced.first, reduced.second)) <=
          oracle_norm(reduced.first));
  REQUIRE(reduced.first.x > 0 ||
          (reduced.first.x == 0 && reduced.first.y > 0));
  REQUIRE(reduced.second.x > 0 ||
          (reduced.second.x == 0 && reduced.second.y > 0));
}

std::int64_t exhaustive_shortest_norm(const LatticeVector2i first,
                                      const LatticeVector2i second,
                                      const std::int64_t candidate_norm) {
  const long double determinant =
      static_cast<long double>(abs_i64(oracle_det(first, second)));
  const long double radius = std::sqrt(static_cast<long double>(candidate_norm));
  const long double first_length =
      std::sqrt(static_cast<long double>(oracle_norm(first)));
  const long double second_length =
      std::sqrt(static_cast<long double>(oracle_norm(second)));
  const std::int64_t bound_first = static_cast<std::int64_t>(
      std::ceil(radius * second_length / determinant)) + 2;
  const std::int64_t bound_second = static_cast<std::int64_t>(
      std::ceil(radius * first_length / determinant)) + 2;

  std::int64_t best = std::numeric_limits<std::int64_t>::max();
  for (std::int64_t a = -bound_first; a <= bound_first; ++a) {
    for (std::int64_t b = -bound_second; b <= bound_second; ++b) {
      if (a == 0 && b == 0) {
        continue;
      }
      const LatticeVector2i value{a * first.x + b * second.x,
                                  a * first.y + b * second.y};
      best = std::min(best, oracle_norm(value));
    }
  }
  return best;
}

TEST_CASE(gauss_lattice_reduction_validation_and_known_basis) {
  REQUIRE_THROWS_AS(gauss_reduce_lattice_basis_2d({0, 0}, {1, 0}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(gauss_reduce_lattice_basis_2d({1, 2}, {2, 4}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      gauss_reduce_lattice_basis_2d({kGaussLatticeCoordinateBound + 1, 0},
                                    {0, 1}),
      std::out_of_range);

  const auto result = gauss_reduce_lattice_basis_2d({4, 1}, {1, 3});
  REQUIRE_EQ(result.first, (LatticeVector2i{1, 3}));
  REQUIRE_EQ(result.second, (LatticeVector2i{3, -2}));
  REQUIRE(result.size_reductions == 1);
  require_valid_reduction({4, 1}, {1, 3}, result);
}

TEST_CASE(gauss_lattice_reduction_boundary_and_replay) {
  const LatticeVector2i first{kGaussLatticeCoordinateBound, 0};
  const LatticeVector2i second{kGaussLatticeCoordinateBound - 1, 1};
  const auto result = gauss_reduce_lattice_basis_2d(first, second);
  require_valid_reduction(first, second, result);
  REQUIRE_EQ(oracle_norm(result.first), std::int64_t{2});
  REQUIRE_EQ(result.first, (LatticeVector2i{1, -1}));
  REQUIRE(result.size_reductions > 0);

  const auto repeated = gauss_reduce_lattice_basis_2d(first, second);
  REQUIRE_EQ(result.first, repeated.first);
  REQUIRE_EQ(result.second, repeated.second);
  REQUIRE_EQ(result.transform, repeated.transform);
  REQUIRE_EQ(result.size_reductions, repeated.size_reductions);
}

TEST_CASE(gauss_lattice_reduction_randomized_shortest_vector_oracle) {
  std::mt19937_64 random(0x6A7555ULL);
  std::uniform_int_distribution<std::int64_t> coordinate(-20, 20);

  std::size_t accepted = 0;
  while (accepted < 600) {
    const LatticeVector2i first{coordinate(random), coordinate(random)};
    const LatticeVector2i second{coordinate(random), coordinate(random)};
    if (oracle_det(first, second) == 0) {
      continue;
    }
    ++accepted;

    const auto result = gauss_reduce_lattice_basis_2d(first, second);
    require_valid_reduction(first, second, result);

    const std::int64_t candidate_norm = oracle_norm(result.first);
    REQUIRE_EQ(exhaustive_shortest_norm(first, second, candidate_norm),
               candidate_norm);

    const auto repeated = gauss_reduce_lattice_basis_2d(first, second);
    REQUIRE_EQ(result.first, repeated.first);
    REQUIRE_EQ(result.second, repeated.second);
    REQUIRE_EQ(result.transform, repeated.transform);
  }
}

TEST_CASE(gauss_lattice_reduction_sign_and_swap_invariance_of_lattice) {
  const LatticeVector2i first{-7, 11};
  const LatticeVector2i second{13, 5};
  const auto baseline = gauss_reduce_lattice_basis_2d(first, second);
  const auto swapped = gauss_reduce_lattice_basis_2d(second, first);
  const auto negated = gauss_reduce_lattice_basis_2d(
      LatticeVector2i{-first.x, -first.y}, second);

  require_valid_reduction(first, second, baseline);
  require_valid_reduction(second, first, swapped);
  require_valid_reduction({-first.x, -first.y}, second, negated);
  REQUIRE_EQ(oracle_norm(baseline.first), oracle_norm(swapped.first));
  REQUIRE_EQ(oracle_norm(baseline.first), oracle_norm(negated.first));
}

}  // namespace
