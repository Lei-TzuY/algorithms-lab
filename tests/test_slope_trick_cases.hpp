#pragma once

#include "algorithms/optimization/slope_trick.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace slope_trick_test_detail {

using algorithms::optimization::SlopeTrick;
using Value = SlopeTrick::Value;

constexpr int kGridMin = -24;
constexpr int kGridMax = 24;
constexpr std::size_t kGridSize =
    static_cast<std::size_t>(kGridMax - kGridMin + 1);
using Grid = std::array<Value, kGridSize>;

[[nodiscard]] constexpr std::size_t grid_index(int x) {
  return static_cast<std::size_t>(x - kGridMin);
}

[[nodiscard]] bool interval_contains(const SlopeTrick::MinimizerInterval& interval,
                                     Value x) {
  if (interval.lower.has_value() && x < *interval.lower) {
    return false;
  }
  if (interval.upper.has_value() && x > *interval.upper) {
    return false;
  }
  return true;
}

void verify_against_grid(const SlopeTrick& slope, const Grid& grid) {
  REQUIRE(slope.valid_structure());
  const Value exact_minimum = *std::min_element(grid.begin(), grid.end());
  REQUIRE_EQ(slope.minimum_value(), exact_minimum);
  const auto interval = slope.minimizer_interval();
  for (int x = kGridMin; x <= kGridMax; ++x) {
    const bool grid_minimizer = grid[grid_index(x)] == exact_minimum;
    REQUIRE(interval_contains(interval, static_cast<Value>(x)) == grid_minimizer);
  }
}

void add_x_minus_a(Grid& grid, Value a) {
  for (int x = kGridMin; x <= kGridMax; ++x) {
    const Value xv = static_cast<Value>(x);
    grid[grid_index(x)] += std::max<Value>(0, xv - a);
  }
}

void add_a_minus_x(Grid& grid, Value a) {
  for (int x = kGridMin; x <= kGridMax; ++x) {
    const Value xv = static_cast<Value>(x);
    grid[grid_index(x)] += std::max<Value>(0, a - xv);
  }
}

void prefix_min(Grid& grid) {
  for (std::size_t i = 1; i < grid.size(); ++i) {
    grid[i] = std::min(grid[i], grid[i - 1]);
  }
}

void suffix_min(Grid& grid) {
  for (std::size_t i = grid.size() - 1; i > 0; --i) {
    grid[i - 1] = std::min(grid[i - 1], grid[i]);
  }
}

}  // namespace slope_trick_test_detail

TEST_CASE(slope_trick_basic_hinges_absolute_and_minimizers) {
  using slope_trick_test_detail::SlopeTrick;

  SlopeTrick slope;
  REQUIRE_EQ(slope.minimum_value(), 0);
  REQUIRE_EQ(slope.breakpoint_count(), 0U);
  REQUIRE(!slope.minimizer_interval().lower.has_value());
  REQUIRE(!slope.minimizer_interval().upper.has_value());
  REQUIRE(slope.valid_structure());

  slope.add_abs(2);
  REQUIRE_EQ(slope.minimum_value(), 0);
  REQUIRE_EQ(*slope.minimizer_interval().lower, 2);
  REQUIRE_EQ(*slope.minimizer_interval().upper, 2);

  slope.add_abs(5);
  REQUIRE_EQ(slope.minimum_value(), 3);
  REQUIRE_EQ(*slope.minimizer_interval().lower, 2);
  REQUIRE_EQ(*slope.minimizer_interval().upper, 5);

  slope.add_abs(10);
  REQUIRE_EQ(slope.minimum_value(), 8);
  REQUIRE_EQ(*slope.minimizer_interval().lower, 5);
  REQUIRE_EQ(*slope.minimizer_interval().upper, 5);
  REQUIRE_EQ(slope.breakpoint_count(), 6U);
  REQUIRE(slope.valid_structure());
}

TEST_CASE(slope_trick_prefix_suffix_and_one_sided_hinges) {
  using slope_trick_test_detail::SlopeTrick;

  SlopeTrick left_hinge;
  left_hinge.add_a_minus_x(4);
  REQUIRE_EQ(left_hinge.minimum_value(), 0);
  REQUIRE_EQ(*left_hinge.minimizer_interval().lower, 4);
  REQUIRE(!left_hinge.minimizer_interval().upper.has_value());

  SlopeTrick right_hinge;
  right_hinge.add_x_minus_a(-3);
  REQUIRE_EQ(right_hinge.minimum_value(), 0);
  REQUIRE(!right_hinge.minimizer_interval().lower.has_value());
  REQUIRE_EQ(*right_hinge.minimizer_interval().upper, -3);

  SlopeTrick prefix;
  prefix.add_abs(0);
  prefix.prefix_min();
  REQUIRE_EQ(*prefix.minimizer_interval().lower, 0);
  REQUIRE(!prefix.minimizer_interval().upper.has_value());
  REQUIRE_EQ(prefix.right_breakpoint_count(), 0U);

  SlopeTrick suffix;
  suffix.add_abs(0);
  suffix.suffix_min();
  REQUIRE(!suffix.minimizer_interval().lower.has_value());
  REQUIRE_EQ(*suffix.minimizer_interval().upper, 0);
  REQUIRE_EQ(suffix.left_breakpoint_count(), 0U);
}

TEST_CASE(slope_trick_arithmetic_overflow_is_preflighted) {
  using slope_trick_test_detail::SlopeTrick;
  using Value = SlopeTrick::Value;

  SlopeTrick minimum_overflow;
  minimum_overflow.add_abs(0);
  minimum_overflow.add_constant(std::numeric_limits<Value>::max());
  const auto before_interval = minimum_overflow.minimizer_interval();
  const auto before_count = minimum_overflow.breakpoint_count();
  REQUIRE_THROWS_AS(minimum_overflow.add_abs(1), std::overflow_error);
  REQUIRE_EQ(minimum_overflow.minimum_value(), std::numeric_limits<Value>::max());
  REQUIRE_EQ(minimum_overflow.minimizer_interval(), before_interval);
  REQUIRE_EQ(minimum_overflow.breakpoint_count(), before_count);
  REQUIRE(minimum_overflow.valid_structure());

  SlopeTrick distance_overflow;
  distance_overflow.add_abs(std::numeric_limits<Value>::min());
  const auto distance_interval = distance_overflow.minimizer_interval();
  REQUIRE_THROWS_AS(distance_overflow.add_abs(std::numeric_limits<Value>::max()),
                    std::overflow_error);
  REQUIRE_EQ(distance_overflow.minimum_value(), 0);
  REQUIRE_EQ(distance_overflow.minimizer_interval(), distance_interval);
  REQUIRE_EQ(distance_overflow.breakpoint_count(), 2U);
  REQUIRE(distance_overflow.valid_structure());

  SlopeTrick one_sided;
  one_sided.add_a_minus_x(std::numeric_limits<Value>::max());
  const auto one_sided_interval = one_sided.minimizer_interval();
  REQUIRE_THROWS_AS(one_sided.add_x_minus_a(
                        std::numeric_limits<Value>::min()),
                    std::overflow_error);
  REQUIRE_EQ(one_sided.minimum_value(), 0);
  REQUIRE_EQ(one_sided.minimizer_interval(), one_sided_interval);
  REQUIRE_EQ(one_sided.breakpoint_count(), 1U);
  REQUIRE(one_sided.valid_structure());

  // The breakpoint distance is UINT64_MAX, but the negative existing minimum
  // cancels enough of it that the exact new minimum is still representable.
  SlopeTrick wide_distance_but_representable_result;
  wide_distance_but_representable_result.add_abs(
      std::numeric_limits<Value>::min());
  wide_distance_but_representable_result.add_constant(
      std::numeric_limits<Value>::min());
  wide_distance_but_representable_result.add_abs(
      std::numeric_limits<Value>::max());
  REQUIRE_EQ(wide_distance_but_representable_result.minimum_value(),
             std::numeric_limits<Value>::max());
  REQUIRE_EQ(*wide_distance_but_representable_result.minimizer_interval().lower,
             std::numeric_limits<Value>::min());
  REQUIRE_EQ(*wide_distance_but_representable_result.minimizer_interval().upper,
             std::numeric_limits<Value>::max());
  REQUIRE(wide_distance_but_representable_result.valid_structure());
}

TEST_CASE(slope_trick_randomized_exact_grid_differential) {
  using namespace slope_trick_test_detail;

  std::mt19937_64 rng(0x51A0E7A1ULL);
  std::uniform_int_distribution<int> coordinate(-8, 8);
  std::uniform_int_distribution<int> constant(-3, 3);
  std::uniform_int_distribution<int> operation(0, 5);

  for (int trial = 0; trial < 500; ++trial) {
    SlopeTrick slope;
    Grid grid{};
    verify_against_grid(slope, grid);

    for (int step = 0; step < 100; ++step) {
      const int op = operation(rng);
      const Value a = static_cast<Value>(coordinate(rng));
      if (op == 0) {
        slope.add_x_minus_a(a);
        add_x_minus_a(grid, a);
      } else if (op == 1) {
        slope.add_a_minus_x(a);
        add_a_minus_x(grid, a);
      } else if (op == 2) {
        slope.add_abs(a);
        add_x_minus_a(grid, a);
        add_a_minus_x(grid, a);
      } else if (op == 3) {
        const Value delta = static_cast<Value>(constant(rng));
        slope.add_constant(delta);
        for (Value& value : grid) {
          value += delta;
        }
      } else if (op == 4) {
        slope.prefix_min();
        prefix_min(grid);
      } else {
        slope.suffix_min();
        suffix_min(grid);
      }
      verify_against_grid(slope, grid);
    }
  }
}
