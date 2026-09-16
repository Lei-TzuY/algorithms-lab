#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::geometry {

struct HalfPlane2i {
  std::int64_t a;
  std::int64_t b;
  std::int64_t c;

  friend bool operator==(const HalfPlane2i&, const HalfPlane2i&) = default;
};

struct RationalPoint2 {
  std::int64_t x_numerator;
  std::int64_t y_numerator;
  std::int64_t denominator;

  friend bool operator==(const RationalPoint2&, const RationalPoint2&) = default;
};

inline constexpr std::int64_t kHalfPlaneCoefficientLimit = 500;
inline constexpr std::size_t kHalfPlaneCountLimit = 128;

namespace bounded_half_plane_detail {

[[nodiscard]] inline std::int64_t magnitude(std::int64_t value) noexcept {
  return value < 0 ? -value : value;
}

[[nodiscard]] inline RationalPoint2 normalize_point(std::int64_t x,
                                                     std::int64_t y,
                                                     std::int64_t denominator) {
  if (denominator == 0) {
    throw std::logic_error("cannot normalize a point with zero denominator");
  }
  if (denominator < 0) {
    x = -x;
    y = -y;
    denominator = -denominator;
  }
  const auto common = std::gcd(
      magnitude(x), std::gcd(magnitude(y), magnitude(denominator)));
  const auto divisor = common == 0 ? std::int64_t{1} : common;
  return RationalPoint2{x / divisor, y / divisor, denominator / divisor};
}

struct RationalPointLess {
  [[nodiscard]] bool operator()(const RationalPoint2& lhs,
                                const RationalPoint2& rhs) const noexcept {
    const auto left_x = lhs.x_numerator * rhs.denominator;
    const auto right_x = rhs.x_numerator * lhs.denominator;
    if (left_x != right_x) {
      return left_x < right_x;
    }
    const auto left_y = lhs.y_numerator * rhs.denominator;
    const auto right_y = rhs.y_numerator * lhs.denominator;
    return left_y < right_y;
  }
};

inline void validate_half_plane(const HalfPlane2i& half_plane) {
  const auto limit = kHalfPlaneCoefficientLimit;
  if (half_plane.a < -limit || half_plane.a > limit ||
      half_plane.b < -limit || half_plane.b > limit ||
      half_plane.c < -limit || half_plane.c > limit) {
    throw std::out_of_range("half-plane coefficient outside exact domain");
  }
  if (half_plane.a == 0 && half_plane.b == 0) {
    throw std::invalid_argument("half-plane normal must be nonzero");
  }
}

[[nodiscard]] inline bool satisfies(const HalfPlane2i& half_plane,
                                    const RationalPoint2& point) noexcept {
  const auto lhs = half_plane.a * point.x_numerator +
                   half_plane.b * point.y_numerator;
  const auto rhs = half_plane.c * point.denominator;
  return lhs <= rhs;
}

[[nodiscard]] inline bool feasible(
    std::span<const HalfPlane2i> half_planes,
    const RationalPoint2& point) noexcept {
  for (const auto& half_plane : half_planes) {
    if (!satisfies(half_plane, point)) {
      return false;
    }
  }
  return true;
}

}  // namespace bounded_half_plane_detail

// Returns the exact extreme points of the intersection of `half_planes` with
// the explicit box [-box_bound, box_bound]^2. Points are unique and sorted
// lexicographically by exact rational (x,y) value. Empty intersections return
// an empty vector; lower-dimensional intersections return their one or two
// extreme points.
//
// Exact arithmetic is guaranteed by the public coefficient/box bounds. This
// direct educational baseline enumerates every pair of boundary lines and
// therefore performs O(H^3) work for H = half_planes.size() + 4.
[[nodiscard]] inline std::vector<RationalPoint2> bounded_half_plane_intersection(
    std::span<const HalfPlane2i> half_planes, std::int64_t box_bound) {
  if (half_planes.size() > kHalfPlaneCountLimit) {
    throw std::length_error("too many half-planes for bounded baseline");
  }
  if (box_bound <= 0 || box_bound > kHalfPlaneCoefficientLimit) {
    throw std::out_of_range("box bound outside exact domain");
  }

  std::vector<HalfPlane2i> constraints;
  constraints.reserve(half_planes.size() + 4U);
  for (const auto& half_plane : half_planes) {
    bounded_half_plane_detail::validate_half_plane(half_plane);
    constraints.push_back(half_plane);
  }
  constraints.push_back(HalfPlane2i{1, 0, box_bound});
  constraints.push_back(HalfPlane2i{-1, 0, box_bound});
  constraints.push_back(HalfPlane2i{0, 1, box_bound});
  constraints.push_back(HalfPlane2i{0, -1, box_bound});

  std::set<RationalPoint2, bounded_half_plane_detail::RationalPointLess>
      vertices;
  for (std::size_t first = 0; first < constraints.size(); ++first) {
    for (std::size_t second = first + 1; second < constraints.size(); ++second) {
      const auto& lhs = constraints[first];
      const auto& rhs = constraints[second];
      const auto determinant = lhs.a * rhs.b - rhs.a * lhs.b;
      if (determinant == 0) {
        continue;
      }
      const auto x_numerator = lhs.c * rhs.b - rhs.c * lhs.b;
      const auto y_numerator = lhs.a * rhs.c - rhs.a * lhs.c;
      const auto point = bounded_half_plane_detail::normalize_point(
          x_numerator, y_numerator, determinant);
      if (bounded_half_plane_detail::feasible(constraints, point)) {
        vertices.insert(point);
      }
    }
  }

  return std::vector<RationalPoint2>(vertices.begin(), vertices.end());
}

}  // namespace algorithms::geometry
