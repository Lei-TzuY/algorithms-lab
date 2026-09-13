#pragma once

#include "algorithms/geometry/minkowski_sum.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace {

using algorithms::geometry::Point2i;
using algorithms::geometry::convex_hull;
using algorithms::geometry::convex_minkowski_sum;

[[nodiscard]] std::vector<Point2i> minkowski_pairwise_oracle(
    const std::vector<Point2i>& left, const std::vector<Point2i>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }
  std::vector<Point2i> sums;
  sums.reserve(left.size() * right.size());
  for (const Point2i a : left) {
    for (const Point2i b : right) {
      sums.push_back(Point2i{
          static_cast<std::int32_t>(static_cast<std::int64_t>(a.x) + b.x),
          static_cast<std::int32_t>(static_cast<std::int64_t>(a.y) + b.y)});
    }
  }
  return convex_hull(sums);
}

void require_canonical_convex_hull(const std::vector<Point2i>& hull) {
  if (hull.empty()) {
    return;
  }
  const auto lex_less = [](const Point2i left, const Point2i right) {
    return left.x < right.x || (left.x == right.x && left.y < right.y);
  };
  REQUIRE(std::min_element(hull.begin(), hull.end(), lex_less) == hull.begin());
  if (hull.size() < 3U) {
    return;
  }
  for (std::size_t index = 0; index < hull.size(); ++index) {
    const Point2i a = hull[index];
    const Point2i b = hull[(index + 1U) % hull.size()];
    const Point2i c = hull[(index + 2U) % hull.size()];
    REQUIRE(algorithms::geometry::orientation(a, b, c) ==
            algorithms::geometry::Orientation::Counterclockwise);
  }
}

TEST_CASE(minkowski_sum_deterministic_polygon_and_degenerate_cases) {
  const std::vector<Point2i> square{{0, 0}, {2, 0}, {2, 2}, {0, 2}};
  const std::vector<Point2i> triangle{{0, 0}, {2, 0}, {0, 1}};
  const auto expected = minkowski_pairwise_oracle(square, triangle);
  REQUIRE_EQ(convex_minkowski_sum(square, triangle), expected);
  REQUIRE_EQ(convex_minkowski_sum(triangle, square), expected);

  const std::vector<Point2i> segment_x{{0, 0}, {2, 0}};
  const std::vector<Point2i> segment_y{{0, 0}, {0, 3}};
  REQUIRE_EQ(convex_minkowski_sum(segment_x, segment_y),
             minkowski_pairwise_oracle(segment_x, segment_y));

  const std::vector<Point2i> point{{7, -3}};
  REQUIRE_EQ(convex_minkowski_sum(point, square),
             minkowski_pairwise_oracle(point, square));

  const std::vector<Point2i> empty;
  REQUIRE(convex_minkowski_sum(empty, square).empty());
  REQUIRE(convex_minkowski_sum(square, empty).empty());
}

TEST_CASE(minkowski_sum_parallel_direction_and_input_canonicalization) {
  const std::vector<Point2i> left{{0, 0}, {4, 0}, {3, 2}, {0, 3},
                                  {1, 1}, {4, 0}, {0, 0}};
  const std::vector<Point2i> right{{0, 0}, {2, -2}, {4, 0}, {1, 3},
                                   {2, 0}, {0, 0}};
  const auto actual = convex_minkowski_sum(left, right);
  REQUIRE_EQ(actual, minkowski_pairwise_oracle(left, right));
  require_canonical_convex_hull(actual);
}

TEST_CASE(minkowski_sum_exact_domain_boundary_and_rejection) {
  const std::vector<Point2i> high{{500'000'000, 500'000'000}};
  const std::vector<Point2i> low{{-500'000'000, -500'000'000}};
  REQUIRE_EQ(convex_minkowski_sum(high, high),
             (std::vector<Point2i>{{1'000'000'000, 1'000'000'000}}));
  REQUIRE_EQ(convex_minkowski_sum(low, low),
             (std::vector<Point2i>{{-1'000'000'000, -1'000'000'000}}));

  const std::vector<Point2i> invalid{{500'000'001, 0}};
  const std::vector<Point2i> origin{{0, 0}};
  const std::vector<Point2i> empty;
  REQUIRE_THROWS_AS(convex_minkowski_sum(invalid, origin), std::out_of_range);
  REQUIRE_THROWS_AS(convex_minkowski_sum(origin, invalid), std::out_of_range);
  REQUIRE(convex_minkowski_sum(empty, invalid).empty());
  REQUIRE(convex_minkowski_sum(invalid, empty).empty());
}

TEST_CASE(minkowski_sum_randomized_differential_against_pairwise_hull) {
  std::mt19937_64 rng(0x4D494E4B4F57534BULL);
  std::uniform_int_distribution<int> size_distribution(0, 14);
  std::uniform_int_distribution<int> coordinate_distribution(-70, 70);

  for (std::size_t trial = 0; trial < 2'000U; ++trial) {
    std::vector<Point2i> left;
    std::vector<Point2i> right;
    const int left_size = size_distribution(rng);
    const int right_size = size_distribution(rng);
    for (int index = 0; index < left_size; ++index) {
      left.push_back(Point2i{static_cast<std::int32_t>(coordinate_distribution(rng)),
                             static_cast<std::int32_t>(coordinate_distribution(rng))});
    }
    for (int index = 0; index < right_size; ++index) {
      right.push_back(Point2i{static_cast<std::int32_t>(coordinate_distribution(rng)),
                              static_cast<std::int32_t>(coordinate_distribution(rng))});
    }

    const auto expected = minkowski_pairwise_oracle(left, right);
    const auto actual = convex_minkowski_sum(left, right);
    REQUIRE_EQ(actual, expected);
    REQUIRE_EQ(convex_minkowski_sum(right, left), expected);
    require_canonical_convex_hull(actual);
  }
}

}  // namespace
