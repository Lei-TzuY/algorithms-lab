#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::geometry {

struct MinimumEnclosingCircle {
  long double center_x = 0.0L;
  long double center_y = 0.0L;
  long double radius_squared = 0.0L;
  std::array<Point2i, 3> support{};
  std::size_t support_size = 0;
};

namespace minimum_enclosing_circle_detail {

constexpr std::int32_t kCoordinateLimit = 1'000'000'000;

inline void validate_point(const Point2i point) {
  if (point.x < -kCoordinateLimit || point.x > kCoordinateLimit ||
      point.y < -kCoordinateLimit || point.y > kCoordinateLimit) {
    throw std::out_of_range("minimum enclosing circle coordinate outside exact-input domain");
  }
}

[[nodiscard]] inline bool lex_less(const Point2i left, const Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

[[nodiscard]] inline std::int64_t cross(const Point2i a, const Point2i b,
                                        const Point2i c) {
  const std::int64_t ab_x = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t ab_y = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t ac_x = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t ac_y = static_cast<std::int64_t>(c.y) - a.y;
  return ab_x * ac_y - ab_y * ac_x;
}

inline void canonicalize_support(MinimumEnclosingCircle& circle) {
  std::sort(circle.support.begin(),
            circle.support.begin() + static_cast<std::ptrdiff_t>(circle.support_size),
            lex_less);
}

[[nodiscard]] inline MinimumEnclosingCircle point_circle(const Point2i point) {
  MinimumEnclosingCircle circle;
  circle.center_x = static_cast<long double>(point.x);
  circle.center_y = static_cast<long double>(point.y);
  circle.support[0] = point;
  circle.support_size = 1U;
  return circle;
}

[[nodiscard]] inline MinimumEnclosingCircle diameter_circle(const Point2i a,
                                                             const Point2i b) {
  MinimumEnclosingCircle circle;
  circle.center_x = (static_cast<long double>(a.x) + static_cast<long double>(b.x)) / 2.0L;
  circle.center_y = (static_cast<long double>(a.y) + static_cast<long double>(b.y)) / 2.0L;
  const long double dx = static_cast<long double>(a.x) - circle.center_x;
  const long double dy = static_cast<long double>(a.y) - circle.center_y;
  circle.radius_squared = dx * dx + dy * dy;
  circle.support[0] = a;
  circle.support[1] = b;
  circle.support_size = (a == b) ? 1U : 2U;
  canonicalize_support(circle);
  return circle;
}

[[nodiscard]] inline MinimumEnclosingCircle circumcircle(const Point2i a,
                                                          const Point2i b,
                                                          const Point2i c) {
  if (cross(a, b, c) == 0) {
    const auto ab = diameter_circle(a, b);
    const auto ac = diameter_circle(a, c);
    const auto bc = diameter_circle(b, c);
    const MinimumEnclosingCircle* best = &ab;
    if (ac.radius_squared > best->radius_squared) best = &ac;
    if (bc.radius_squared > best->radius_squared) best = &bc;
    return *best;
  }

  const long double ax = static_cast<long double>(a.x);
  const long double ay = static_cast<long double>(a.y);
  const long double bx = static_cast<long double>(b.x);
  const long double by = static_cast<long double>(b.y);
  const long double cx = static_cast<long double>(c.x);
  const long double cy = static_cast<long double>(c.y);
  const long double a2 = ax * ax + ay * ay;
  const long double b2 = bx * bx + by * by;
  const long double c2 = cx * cx + cy * cy;
  const long double denominator =
      2.0L * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
  if (denominator == 0.0L) {
    throw std::overflow_error("minimum enclosing circle lost non-collinear determinant");
  }

  MinimumEnclosingCircle circle;
  circle.center_x =
      (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / denominator;
  circle.center_y =
      (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / denominator;
  const long double dx = ax - circle.center_x;
  const long double dy = ay - circle.center_y;
  circle.radius_squared = dx * dx + dy * dy;
  if (!std::isfinite(circle.center_x) || !std::isfinite(circle.center_y) ||
      !std::isfinite(circle.radius_squared)) {
    throw std::overflow_error("minimum enclosing circle produced non-finite arithmetic");
  }
  circle.support[0] = a;
  circle.support[1] = b;
  circle.support[2] = c;
  circle.support_size = 3U;
  canonicalize_support(circle);
  return circle;
}

[[nodiscard]] inline long double containment_slack(const long double lhs,
                                                   const long double rhs) {
  const long double scale = std::max({1.0L, std::fabs(lhs), std::fabs(rhs)});
  return 256.0L * std::numeric_limits<long double>::epsilon() * scale;
}

[[nodiscard]] inline bool contains(const MinimumEnclosingCircle& circle,
                                   const Point2i point) {
  const long double dx = static_cast<long double>(point.x) - circle.center_x;
  const long double dy = static_cast<long double>(point.y) - circle.center_y;
  const long double distance_squared = dx * dx + dy * dy;
  return distance_squared <= circle.radius_squared +
                                 containment_slack(distance_squared, circle.radius_squared);
}

[[nodiscard]] inline long double center_side(const Point2i a, const Point2i b,
                                             const MinimumEnclosingCircle& circle) {
  const long double ab_x = static_cast<long double>(b.x) - a.x;
  const long double ab_y = static_cast<long double>(b.y) - a.y;
  const long double ac_x = circle.center_x - static_cast<long double>(a.x);
  const long double ac_y = circle.center_y - static_cast<long double>(a.y);
  return ab_x * ac_y - ab_y * ac_x;
}

[[nodiscard]] inline MinimumEnclosingCircle circle_two_points(
    const std::span<const Point2i> prefix, const Point2i p, const Point2i q) {
  const MinimumEnclosingCircle diameter = diameter_circle(p, q);
  bool diameter_contains_all = true;
  for (const Point2i r : prefix) {
    if (!contains(diameter, r)) {
      diameter_contains_all = false;
      break;
    }
  }
  if (diameter_contains_all) return diameter;

  std::optional<MinimumEnclosingCircle> left;
  std::optional<MinimumEnclosingCircle> right;
  long double left_side = 0.0L;
  long double right_side = 0.0L;
  for (const Point2i r : prefix) {
    if (contains(diameter, r)) continue;
    const std::int64_t direction = cross(p, q, r);
    if (direction == 0) continue;
    const MinimumEnclosingCircle candidate = circumcircle(p, q, r);
    const long double side = center_side(p, q, candidate);
    if (direction > 0) {
      if (!left.has_value() || side > left_side) {
        left = candidate;
        left_side = side;
      }
    } else if (!right.has_value() || side < right_side) {
      right = candidate;
      right_side = side;
    }
  }
  if (!left.has_value() && !right.has_value()) return diameter;
  if (!left.has_value()) return *right;
  if (!right.has_value()) return *left;
  return left->radius_squared <= right->radius_squared ? *left : *right;
}

[[nodiscard]] inline std::uint64_t bounded_random(std::mt19937_64& engine,
                                                  const std::uint64_t bound) {
  if (bound == 0U) throw std::logic_error("zero random bound");
  const std::uint64_t threshold = static_cast<std::uint64_t>(-bound) % bound;
  for (;;) {
    const std::uint64_t value = engine();
    if (value >= threshold) return value % bound;
  }
}

inline void replayable_shuffle(std::vector<Point2i>& points, const std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  for (std::size_t remaining = points.size(); remaining > 1U; --remaining) {
    const std::uint64_t bound = static_cast<std::uint64_t>(remaining);
    const std::size_t chosen = static_cast<std::size_t>(bounded_random(engine, bound));
    std::swap(points[remaining - 1U], points[chosen]);
  }
}

}  // namespace minimum_enclosing_circle_detail

[[nodiscard]] inline std::optional<MinimumEnclosingCircle> minimum_enclosing_circle(
    const std::span<const Point2i> input, const std::uint64_t seed = 0x4d45435f53454544ULL) {
  using namespace minimum_enclosing_circle_detail;
  if (input.empty()) return std::nullopt;
  std::vector<Point2i> points(input.begin(), input.end());
  for (const Point2i point : points) validate_point(point);
  replayable_shuffle(points, seed);

  MinimumEnclosingCircle circle = point_circle(points[0]);
  for (std::size_t i = 1U; i < points.size(); ++i) {
    if (contains(circle, points[i])) continue;
    circle = point_circle(points[i]);
    for (std::size_t j = 0U; j < i; ++j) {
      if (contains(circle, points[j])) continue;
      circle = circle_two_points(
          std::span<const Point2i>(points.data(), j), points[i], points[j]);
    }
  }
  return circle;
}

}  // namespace algorithms::geometry
