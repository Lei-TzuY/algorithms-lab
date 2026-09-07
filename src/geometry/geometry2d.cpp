#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace algorithms::geometry {
namespace {

constexpr std::int32_t kCoordinateLimit = 1'000'000'000;

void validate_point(Point2i point) {
  if (point.x < -kCoordinateLimit || point.x > kCoordinateLimit ||
      point.y < -kCoordinateLimit || point.y > kCoordinateLimit) {
    throw std::out_of_range("geometry coordinate outside exact domain");
  }
}

[[nodiscard]] std::int64_t cross(Point2i a, Point2i b, Point2i c) {
  const std::int64_t ab_x = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t ab_y = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t ac_x = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t ac_y = static_cast<std::int64_t>(c.y) - a.y;
  return ab_x * ac_y - ab_y * ac_x;
}

[[nodiscard]] bool lex_less(Point2i left, Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

}  // namespace

Orientation orientation(Point2i a, Point2i b, Point2i c) {
  validate_point(a);
  validate_point(b);
  validate_point(c);
  const std::int64_t value = cross(a, b, c);
  if (value > 0) {
    return Orientation::Counterclockwise;
  }
  if (value < 0) {
    return Orientation::Clockwise;
  }
  return Orientation::Collinear;
}

bool on_segment(Point2i a, Point2i b, Point2i point) {
  validate_point(a);
  validate_point(b);
  validate_point(point);
  if (cross(a, b, point) != 0) {
    return false;
  }
  return point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x) &&
         point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

bool segments_intersect(Point2i a, Point2i b, Point2i c, Point2i d) {
  validate_point(a);
  validate_point(b);
  validate_point(c);
  validate_point(d);

  const std::int64_t abc = cross(a, b, c);
  const std::int64_t abd = cross(a, b, d);
  const std::int64_t cda = cross(c, d, a);
  const std::int64_t cdb = cross(c, d, b);

  const bool ab_straddles = (abc < 0 && abd > 0) || (abc > 0 && abd < 0);
  const bool cd_straddles = (cda < 0 && cdb > 0) || (cda > 0 && cdb < 0);
  if (ab_straddles && cd_straddles) {
    return true;
  }
  if (abc == 0 && on_segment(a, b, c)) {
    return true;
  }
  if (abd == 0 && on_segment(a, b, d)) {
    return true;
  }
  if (cda == 0 && on_segment(c, d, a)) {
    return true;
  }
  return cdb == 0 && on_segment(c, d, b);
}

std::vector<Point2i> convex_hull(std::span<const Point2i> points) {
  std::vector<Point2i> sorted(points.begin(), points.end());
  for (const Point2i point : sorted) {
    validate_point(point);
  }
  std::sort(sorted.begin(), sorted.end(), lex_less);
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  if (sorted.size() <= 1U) {
    return sorted;
  }

  std::vector<Point2i> lower;
  lower.reserve(sorted.size());
  for (const Point2i point : sorted) {
    while (lower.size() >= 2U &&
           cross(lower[lower.size() - 2U], lower.back(), point) <= 0) {
      lower.pop_back();
    }
    lower.push_back(point);
  }

  std::vector<Point2i> upper;
  upper.reserve(sorted.size());
  for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
    const Point2i point = *it;
    while (upper.size() >= 2U &&
           cross(upper[upper.size() - 2U], upper.back(), point) <= 0) {
      upper.pop_back();
    }
    upper.push_back(point);
  }

  lower.pop_back();
  upper.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}

}  // namespace algorithms::geometry
