#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
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

[[nodiscard]] std::int64_t squared_distance(Point2i left, Point2i right) {
  const std::int64_t dx = static_cast<std::int64_t>(right.x) - left.x;
  const std::int64_t dy = static_cast<std::int64_t>(right.y) - left.y;
  return dx * dx + dy * dy;
}

[[nodiscard]] ClosestPairResult make_pair_result(Point2i first, Point2i second) {
  if (lex_less(second, first)) {
    std::swap(first, second);
  }
  return ClosestPairResult{first, second, squared_distance(first, second)};
}

[[nodiscard]] bool pair_less(const ClosestPairResult& left,
                             const ClosestPairResult& right) {
  if (left.squared_distance != right.squared_distance) {
    return left.squared_distance < right.squared_distance;
  }
  if (left.first == right.first) {
    return lex_less(left.second, right.second);
  }
  return lex_less(left.first, right.first);
}

void consider_pair(Point2i first, Point2i second,
                   std::optional<ClosestPairResult>& best) {
  const ClosestPairResult candidate = make_pair_result(first, second);
  if (!best.has_value() || pair_less(candidate, *best)) {
    best = candidate;
  }
}

[[nodiscard]] bool y_then_x_less(Point2i left, Point2i right) {
  return left.y < right.y || (left.y == right.y && lex_less(left, right));
}

std::optional<ClosestPairResult> closest_pair_recursive(
    std::vector<Point2i>& points, std::vector<Point2i>& scratch,
    const std::size_t begin, const std::size_t end) {
  const std::size_t count = end - begin;
  if (count <= 3U) {
    std::optional<ClosestPairResult> best;
    for (std::size_t i = begin; i < end; ++i) {
      for (std::size_t j = i + 1U; j < end; ++j) {
        consider_pair(points[i], points[j], best);
      }
    }
    std::sort(points.begin() + static_cast<std::ptrdiff_t>(begin),
              points.begin() + static_cast<std::ptrdiff_t>(end), y_then_x_less);
    return best;
  }

  const std::size_t middle = begin + count / 2U;
  const std::int32_t middle_x = points[middle].x;
  auto left_best = closest_pair_recursive(points, scratch, begin, middle);
  auto right_best = closest_pair_recursive(points, scratch, middle, end);

  std::optional<ClosestPairResult> best = left_best;
  if (right_best.has_value() &&
      (!best.has_value() || pair_less(*right_best, *best))) {
    best = right_best;
  }

  std::merge(points.begin() + static_cast<std::ptrdiff_t>(begin),
             points.begin() + static_cast<std::ptrdiff_t>(middle),
             points.begin() + static_cast<std::ptrdiff_t>(middle),
             points.begin() + static_cast<std::ptrdiff_t>(end),
             scratch.begin() + static_cast<std::ptrdiff_t>(begin), y_then_x_less);
  std::copy(scratch.begin() + static_cast<std::ptrdiff_t>(begin),
            scratch.begin() + static_cast<std::ptrdiff_t>(end),
            points.begin() + static_cast<std::ptrdiff_t>(begin));

  std::vector<Point2i> strip;
  strip.reserve(count);
  for (std::size_t i = begin; i < end; ++i) {
    const std::int64_t dx = static_cast<std::int64_t>(points[i].x) - middle_x;
    if (!best.has_value() || dx * dx <= best->squared_distance) {
      strip.push_back(points[i]);
    }
  }

  for (std::size_t i = 0; i < strip.size(); ++i) {
    for (std::size_t j = i + 1U; j < strip.size(); ++j) {
      const std::int64_t dy =
          static_cast<std::int64_t>(strip[j].y) - strip[i].y;
      if (best.has_value() && dy * dy > best->squared_distance) {
        break;
      }
      consider_pair(strip[i], strip[j], best);
    }
  }
  return best;
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

std::optional<ClosestPairResult> closest_pair(
    std::span<const Point2i> input_points) {
  if (input_points.size() < 2U) {
    for (const Point2i point : input_points) {
      validate_point(point);
    }
    return std::nullopt;
  }

  std::vector<Point2i> points(input_points.begin(), input_points.end());
  for (const Point2i point : points) {
    validate_point(point);
  }
  std::sort(points.begin(), points.end(), lex_less);

  for (std::size_t i = 1; i < points.size(); ++i) {
    if (points[i] == points[i - 1U]) {
      return make_pair_result(points[i - 1U], points[i]);
    }
  }

  std::vector<Point2i> scratch(points.size());
  return closest_pair_recursive(points, scratch, 0U, points.size());
}

}  // namespace algorithms::geometry
