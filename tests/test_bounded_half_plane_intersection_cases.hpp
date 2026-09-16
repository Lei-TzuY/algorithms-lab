#pragma once

#include "algorithms/geometry/bounded_half_plane_intersection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bounded_hpi_tests {

using algorithms::geometry::HalfPlane2i;
using algorithms::geometry::RationalPoint2;
using algorithms::geometry::bounded_half_plane_intersection;

struct ScaledPoint {
  std::int64_t x;
  std::int64_t y;
  friend bool operator==(const ScaledPoint&, const ScaledPoint&) = default;
};

[[nodiscard]] inline bool scaled_less(const ScaledPoint& lhs,
                                      const ScaledPoint& rhs) noexcept {
  return lhs.x < rhs.x || (lhs.x == rhs.x && lhs.y < rhs.y);
}

[[nodiscard]] inline std::int64_t cross(const ScaledPoint& origin,
                                        const ScaledPoint& first,
                                        const ScaledPoint& second) noexcept {
  return (first.x - origin.x) * (second.y - origin.y) -
         (first.y - origin.y) * (second.x - origin.x);
}

[[nodiscard]] inline RationalPoint2 from_half_grid(ScaledPoint point) {
  auto denominator = std::int64_t{2};
  auto x = point.x;
  auto y = point.y;
  const auto abs_x = x < 0 ? -x : x;
  const auto abs_y = y < 0 ? -y : y;
  const auto common = std::gcd(abs_x, std::gcd(abs_y, denominator));
  x /= common;
  y /= common;
  denominator /= common;
  return RationalPoint2{x, y, denominator};
}

[[nodiscard]] inline std::vector<ScaledPoint> half_grid_hull(
    std::vector<ScaledPoint> points) {
  std::sort(points.begin(), points.end(), scaled_less);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() <= 1U) {
    return points;
  }

  std::vector<ScaledPoint> lower;
  for (const auto point : points) {
    while (lower.size() >= 2U &&
           cross(lower[lower.size() - 2U], lower.back(), point) <= 0) {
      lower.pop_back();
    }
    lower.push_back(point);
  }
  std::vector<ScaledPoint> upper;
  for (auto iterator = points.rbegin(); iterator != points.rend(); ++iterator) {
    while (upper.size() >= 2U &&
           cross(upper[upper.size() - 2U], upper.back(), *iterator) <= 0) {
      upper.pop_back();
    }
    upper.push_back(*iterator);
  }

  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  if (lower.empty()) {
    lower.push_back(points.front());
  }
  std::sort(lower.begin(), lower.end(), scaled_less);
  lower.erase(std::unique(lower.begin(), lower.end()), lower.end());
  return lower;
}

[[nodiscard]] inline std::vector<RationalPoint2> half_grid_oracle(
    const std::vector<HalfPlane2i>& constraints, std::int64_t box_bound) {
  std::vector<ScaledPoint> feasible;
  const auto scaled_bound = 2 * box_bound;
  for (std::int64_t x = -scaled_bound; x <= scaled_bound; ++x) {
    for (std::int64_t y = -scaled_bound; y <= scaled_bound; ++y) {
      bool inside = true;
      for (const auto& half_plane : constraints) {
        if (half_plane.a * x + half_plane.b * y > 2 * half_plane.c) {
          inside = false;
          break;
        }
      }
      if (inside) {
        feasible.push_back(ScaledPoint{x, y});
      }
    }
  }

  const auto hull = half_grid_hull(std::move(feasible));
  std::vector<RationalPoint2> result;
  result.reserve(hull.size());
  for (const auto point : hull) {
    result.push_back(from_half_grid(point));
  }
  return result;
}

[[nodiscard]] inline bool exact_feasible(const RationalPoint2& point,
                                         const HalfPlane2i& half_plane) {
  return half_plane.a * point.x_numerator +
             half_plane.b * point.y_numerator <=
         half_plane.c * point.denominator;
}

}  // namespace bounded_hpi_tests

TEST_CASE(bounded_half_plane_intersection_known_and_degenerate_cases) {
  using namespace bounded_hpi_tests;

  REQUIRE_EQ(bounded_half_plane_intersection({}, 2),
             (std::vector<RationalPoint2>{{-2, -2, 1}, {-2, 2, 1},
                                          {2, -2, 1}, {2, 2, 1}}));

  const std::vector<HalfPlane2i> triangle{{-1, 0, 0}, {0, -1, 0},
                                         {2, 3, 5}};
  REQUIRE_EQ(bounded_half_plane_intersection(triangle, 10),
             (std::vector<RationalPoint2>{{0, 0, 1}, {0, 5, 3}, {5, 0, 2}}));

  const std::vector<HalfPlane2i> line{{1, 0, 0}, {-1, 0, 0}};
  REQUIRE_EQ(bounded_half_plane_intersection(line, 3),
             (std::vector<RationalPoint2>{{0, -3, 1}, {0, 3, 1}}));

  const std::vector<HalfPlane2i> point{{1, 0, 1}, {-1, 0, -1},
                                      {0, 1, -2}, {0, -1, 2}};
  REQUIRE_EQ(bounded_half_plane_intersection(point, 4),
             (std::vector<RationalPoint2>{{1, -2, 1}}));

  const std::vector<HalfPlane2i> empty{{1, 0, -1}, {-1, 0, -1}};
  REQUIRE(bounded_half_plane_intersection(empty, 5).empty());

  const std::vector<HalfPlane2i> redundant{{1, 0, 1}, {2, 0, 2},
                                           {-1, 0, 2}};
  REQUIRE_EQ(bounded_half_plane_intersection(redundant, 2),
             (std::vector<RationalPoint2>{{-2, -2, 1}, {-2, 2, 1},
                                          {1, -2, 1}, {1, 2, 1}}));
}

TEST_CASE(bounded_half_plane_intersection_validation_and_exact_boundary) {
  using namespace bounded_hpi_tests;

  REQUIRE_THROWS_AS(bounded_half_plane_intersection({}, 0), std::out_of_range);
  REQUIRE_THROWS_AS(bounded_half_plane_intersection({}, 501), std::out_of_range);

  const std::vector<HalfPlane2i> zero_normal{{0, 0, 0}};
  REQUIRE_THROWS_AS(bounded_half_plane_intersection(zero_normal, 10),
                    std::invalid_argument);
  const std::vector<HalfPlane2i> coefficient_too_large{{501, 1, 0}};
  REQUIRE_THROWS_AS(bounded_half_plane_intersection(coefficient_too_large, 10),
                    std::out_of_range);

  std::vector<HalfPlane2i> too_many(
      algorithms::geometry::kHalfPlaneCountLimit + 1U, HalfPlane2i{1, 0, 500});
  REQUIRE_THROWS_AS(bounded_half_plane_intersection(too_many, 500),
                    std::length_error);

  const std::vector<HalfPlane2i> boundary{{500, 499, 500},
                                         {-500, -499, 500}};
  const auto result = bounded_half_plane_intersection(boundary, 500);
  REQUIRE(!result.empty());
  for (const auto& vertex : result) {
    REQUIRE(vertex.denominator > 0);
    for (const auto& half_plane : boundary) {
      REQUIRE(exact_feasible(vertex, half_plane));
    }
  }
}

TEST_CASE(bounded_half_plane_intersection_randomized_half_grid_differential) {
  using namespace bounded_hpi_tests;

  constexpr std::array<std::pair<std::int64_t, std::int64_t>, 8> normals{{
      {1, 0}, {-1, 0}, {0, 1}, {0, -1},
      {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
  }};

  std::mt19937_64 random(0x48414C46504C414EULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const auto box_bound = std::int64_t{1} +
                           static_cast<std::int64_t>(random() % 8U);
    const auto count = static_cast<std::size_t>(random() % 11U);
    std::vector<HalfPlane2i> constraints;
    constraints.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const auto normal = normals[static_cast<std::size_t>(random() % normals.size())];
      const auto width = static_cast<std::uint64_t>(2 * box_bound + 1);
      const auto c = static_cast<std::int64_t>(random() % width) - box_bound;
      constraints.push_back(HalfPlane2i{normal.first, normal.second, c});
    }

    const auto actual = bounded_half_plane_intersection(constraints, box_bound);
    const auto expected = half_grid_oracle(constraints, box_bound);
    REQUIRE_EQ(actual, expected);

    for (const auto& vertex : actual) {
      REQUIRE(vertex.denominator > 0);
      REQUIRE(vertex.x_numerator <= box_bound * vertex.denominator);
      REQUIRE(vertex.x_numerator >= -box_bound * vertex.denominator);
      REQUIRE(vertex.y_numerator <= box_bound * vertex.denominator);
      REQUIRE(vertex.y_numerator >= -box_bound * vertex.denominator);
      for (const auto& half_plane : constraints) {
        REQUIRE(exact_feasible(vertex, half_plane));
      }
    }

    if (trial % 17U == 0U) {
      auto shuffled = constraints;
      std::shuffle(shuffled.begin(), shuffled.end(), random);
      REQUIRE_EQ(bounded_half_plane_intersection(shuffled, box_bound), actual);
    }
  }
}

TEST_CASE(bounded_half_plane_intersection_ignored_scaling_and_rational_replay) {
  using namespace bounded_hpi_tests;

  const std::vector<HalfPlane2i> constraints{{3, 2, 7}, {-2, 3, 4},
                                             {-1, 0, 2}, {0, -1, 3}};
  const auto result = bounded_half_plane_intersection(constraints, 6);
  REQUIRE(!result.empty());
  for (const auto& vertex : result) {
    for (const auto& half_plane : constraints) {
      REQUIRE(exact_feasible(vertex, half_plane));
    }
  }

  std::vector<HalfPlane2i> with_scaled = constraints;
  with_scaled.push_back(HalfPlane2i{6, 4, 14});
  REQUIRE_EQ(bounded_half_plane_intersection(with_scaled, 6), result);
}
