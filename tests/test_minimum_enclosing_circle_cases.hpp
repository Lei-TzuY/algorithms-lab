#pragma once

#include "algorithms/geometry/minimum_enclosing_circle.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace {

using algorithms::geometry::MinimumEnclosingCircle;
using algorithms::geometry::Point2i;
using algorithms::geometry::minimum_enclosing_circle;

[[nodiscard]] long double mec_test_tolerance(const long double left,
                                             const long double right) {
  return 2048.0L * std::numeric_limits<long double>::epsilon() *
         std::max({1.0L, std::fabs(left), std::fabs(right)});
}

[[nodiscard]] bool mec_test_close(const long double left,
                                  const long double right) {
  return std::fabs(left - right) <= mec_test_tolerance(left, right);
}

[[nodiscard]] bool mec_test_contains(const MinimumEnclosingCircle& circle,
                                     const Point2i point) {
  const long double dx = static_cast<long double>(point.x) - circle.center_x;
  const long double dy = static_cast<long double>(point.y) - circle.center_y;
  const long double distance_squared = dx * dx + dy * dy;
  return distance_squared <= circle.radius_squared +
                                 mec_test_tolerance(distance_squared,
                                                    circle.radius_squared);
}

[[nodiscard]] MinimumEnclosingCircle mec_test_point_circle(const Point2i point) {
  MinimumEnclosingCircle circle;
  circle.center_x = static_cast<long double>(point.x);
  circle.center_y = static_cast<long double>(point.y);
  circle.support[0] = point;
  circle.support_size = 1U;
  return circle;
}

[[nodiscard]] MinimumEnclosingCircle mec_test_diameter_circle(const Point2i a,
                                                              const Point2i b) {
  MinimumEnclosingCircle circle;
  circle.center_x =
      (static_cast<long double>(a.x) + static_cast<long double>(b.x)) / 2.0L;
  circle.center_y =
      (static_cast<long double>(a.y) + static_cast<long double>(b.y)) / 2.0L;
  const long double dx = static_cast<long double>(a.x) - circle.center_x;
  const long double dy = static_cast<long double>(a.y) - circle.center_y;
  circle.radius_squared = dx * dx + dy * dy;
  circle.support[0] = a;
  circle.support[1] = b;
  circle.support_size = a == b ? 1U : 2U;
  return circle;
}

[[nodiscard]] std::optional<MinimumEnclosingCircle> mec_test_circumcircle(
    const Point2i a, const Point2i b, const Point2i c) {
  const long double ax = static_cast<long double>(a.x);
  const long double ay = static_cast<long double>(a.y);
  const long double bx = static_cast<long double>(b.x);
  const long double by = static_cast<long double>(b.y);
  const long double cx = static_cast<long double>(c.x);
  const long double cy = static_cast<long double>(c.y);
  const long double denominator =
      2.0L * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
  if (denominator == 0.0L) return std::nullopt;

  const long double a2 = ax * ax + ay * ay;
  const long double b2 = bx * bx + by * by;
  const long double c2 = cx * cx + cy * cy;
  MinimumEnclosingCircle circle;
  circle.center_x =
      (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / denominator;
  circle.center_y =
      (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / denominator;
  const long double dx = ax - circle.center_x;
  const long double dy = ay - circle.center_y;
  circle.radius_squared = dx * dx + dy * dy;
  circle.support = {a, b, c};
  circle.support_size = 3U;
  return circle;
}

[[nodiscard]] std::optional<MinimumEnclosingCircle> mec_test_exhaustive_oracle(
    const std::span<const Point2i> points) {
  if (points.empty()) return std::nullopt;
  std::optional<MinimumEnclosingCircle> best;
  const auto consider = [&](const MinimumEnclosingCircle& candidate) {
    for (const Point2i point : points) {
      if (!mec_test_contains(candidate, point)) return;
    }
    if (!best.has_value() ||
        candidate.radius_squared <
            best->radius_squared -
                mec_test_tolerance(candidate.radius_squared,
                                   best->radius_squared)) {
      best = candidate;
    }
  };

  for (const Point2i point : points) consider(mec_test_point_circle(point));
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1U; j < points.size(); ++j) {
      consider(mec_test_diameter_circle(points[i], points[j]));
    }
  }
  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1U; j < points.size(); ++j) {
      for (std::size_t k = j + 1U; k < points.size(); ++k) {
        if (const auto circle = mec_test_circumcircle(points[i], points[j], points[k]);
            circle.has_value()) {
          consider(*circle);
        }
      }
    }
  }
  REQUIRE(best.has_value());
  return best;
}

void mec_test_verify_result(const std::span<const Point2i> points,
                            const std::uint64_t seed) {
  const auto actual = minimum_enclosing_circle(points, seed);
  const auto expected = mec_test_exhaustive_oracle(points);
  REQUIRE_EQ(actual.has_value(), expected.has_value());
  if (!actual.has_value()) return;

  REQUIRE(mec_test_close(actual->radius_squared, expected->radius_squared));
  REQUIRE(mec_test_close(actual->center_x, expected->center_x));
  REQUIRE(mec_test_close(actual->center_y, expected->center_y));
  REQUIRE(actual->support_size >= 1U && actual->support_size <= 3U);

  for (const Point2i point : points) REQUIRE(mec_test_contains(*actual, point));
  for (std::size_t index = 0; index < actual->support_size; ++index) {
    const Point2i support = actual->support[index];
    REQUIRE(std::find(points.begin(), points.end(), support) != points.end());
    const long double dx = static_cast<long double>(support.x) - actual->center_x;
    const long double dy = static_cast<long double>(support.y) - actual->center_y;
    REQUIRE(mec_test_close(dx * dx + dy * dy, actual->radius_squared));
    if (index > 0U) {
      const Point2i previous = actual->support[index - 1U];
      REQUIRE(previous.x < support.x ||
              (previous.x == support.x && previous.y <= support.y));
    }
  }
}

TEST_CASE(minimum_enclosing_circle_known_shapes_and_validation) {
  const std::vector<Point2i> empty;
  REQUIRE(!minimum_enclosing_circle(empty).has_value());

  const std::vector<Point2i> singleton{{7, -9}};
  mec_test_verify_result(singleton, 1U);
  const std::vector<Point2i> obtuse{{0, 0}, {10, 0}, {1, 1}};
  mec_test_verify_result(obtuse, 2U);
  const std::vector<Point2i> acute{{0, 0}, {4, 0}, {2, 4}};
  mec_test_verify_result(acute, 3U);
  const std::vector<Point2i> collinear{{-5, 0}, {0, 0}, {3, 0}, {5, 0}};
  mec_test_verify_result(collinear, 4U);
  const std::vector<Point2i> duplicates{{1, 1}, {1, 1}, {1, 1}, {2, 2}};
  mec_test_verify_result(duplicates, 5U);

  const std::vector<Point2i> boundary{{-1'000'000'000, 0},
                                     {1'000'000'000, 0}, {0, 3}};
  mec_test_verify_result(boundary, 6U);
  const std::vector<Point2i> outside{{1'000'000'001, 0}};
  REQUIRE_THROWS_AS(minimum_enclosing_circle(outside), std::out_of_range);
}

TEST_CASE(minimum_enclosing_circle_seed_replay_and_order_independence) {
  std::vector<Point2i> points{{-6, -2}, {-4, 5}, {0, -7}, {3, 8},
                              {9, 1},   {2, 2},  {-1, 3}, {7, -4}};
  const auto first = minimum_enclosing_circle(points, 0x123456789ULL);
  const auto replay = minimum_enclosing_circle(points, 0x123456789ULL);
  REQUIRE(first.has_value());
  REQUIRE(replay.has_value());
  REQUIRE_EQ(first->support_size, replay->support_size);
  REQUIRE_EQ(first->support, replay->support);
  REQUIRE(first->center_x == replay->center_x);
  REQUIRE(first->center_y == replay->center_y);
  REQUIRE(first->radius_squared == replay->radius_squared);

  const auto alternate = minimum_enclosing_circle(points, 0xfedcba987ULL);
  REQUIRE(alternate.has_value());
  REQUIRE(mec_test_close(first->center_x, alternate->center_x));
  REQUIRE(mec_test_close(first->center_y, alternate->center_y));
  REQUIRE(mec_test_close(first->radius_squared, alternate->radius_squared));

  std::reverse(points.begin(), points.end());
  const auto reversed = minimum_enclosing_circle(points, 0x123456789ULL);
  REQUIRE(reversed.has_value());
  REQUIRE(mec_test_close(first->center_x, reversed->center_x));
  REQUIRE(mec_test_close(first->center_y, reversed->center_y));
  REQUIRE(mec_test_close(first->radius_squared, reversed->radius_squared));
}

TEST_CASE(minimum_enclosing_circle_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0x4d45435f4f524143ULL);
  std::uniform_int_distribution<int> size_distribution(1, 10);
  std::uniform_int_distribution<int> coordinate_distribution(-80, 80);
  for (std::size_t trial = 0; trial < 800U; ++trial) {
    const int count = size_distribution(rng);
    std::vector<Point2i> points;
    points.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
      points.push_back(Point2i{coordinate_distribution(rng),
                              coordinate_distribution(rng)});
    }
    mec_test_verify_result(points, static_cast<std::uint64_t>(trial + 17U));
  }
}

TEST_CASE(minimum_enclosing_circle_near_collinear_integer_stress) {
  for (std::int32_t offset = -40; offset <= 40; ++offset) {
    const std::vector<Point2i> points{{-1000, -1000}, {0, offset},
                                     {1000, 1000}, {400, 399}};
    mec_test_verify_result(points,
                           static_cast<std::uint64_t>(offset + 1000));
  }
}

}  // namespace
