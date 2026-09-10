#include "algorithms/geometry/planar_diameter.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::geometry {
namespace {

[[nodiscard]] bool lex_less(Point2i left, Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

[[nodiscard]] std::int64_t squared_distance(Point2i left, Point2i right) {
  const std::int64_t dx = static_cast<std::int64_t>(right.x) - left.x;
  const std::int64_t dy = static_cast<std::int64_t>(right.y) - left.y;
  return dx * dx + dy * dy;
}

[[nodiscard]] std::int64_t twice_triangle_area(Point2i a, Point2i b,
                                               Point2i c) {
  const std::int64_t ab_x = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t ab_y = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t ac_x = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t ac_y = static_cast<std::int64_t>(c.y) - a.y;
  return ab_x * ac_y - ab_y * ac_x;
}

[[nodiscard]] PlanarDiameterResult make_result(Point2i first, Point2i second) {
  if (lex_less(second, first)) {
    std::swap(first, second);
  }
  return PlanarDiameterResult{first, second, squared_distance(first, second)};
}

[[nodiscard]] bool result_better(const PlanarDiameterResult& candidate,
                                 const PlanarDiameterResult& incumbent) {
  if (candidate.squared_distance != incumbent.squared_distance) {
    return candidate.squared_distance > incumbent.squared_distance;
  }
  if (!(candidate.first == incumbent.first)) {
    return lex_less(candidate.first, incumbent.first);
  }
  return lex_less(candidate.second, incumbent.second);
}

void consider_pair(Point2i first, Point2i second,
                   std::optional<PlanarDiameterResult>& best) {
  const auto candidate = make_result(first, second);
  if (!best.has_value() || result_better(candidate, *best)) {
    best = candidate;
  }
}

}  // namespace

std::optional<PlanarDiameterResult> planar_diameter(
    std::span<const Point2i> points) {
  // Reuse the sealed hull implementation both for validation and for the
  // theorem that a Euclidean diameter has hull endpoints.
  const std::vector<Point2i> hull = convex_hull(points);
  if (points.size() < 2U) {
    return std::nullopt;
  }
  if (hull.size() == 1U) {
    return make_result(hull.front(), hull.front());
  }
  if (hull.size() == 2U) {
    return make_result(hull[0], hull[1]);
  }

  const std::size_t count = hull.size();
  std::size_t opposite = 1U;
  std::optional<PlanarDiameterResult> best;

  for (std::size_t edge = 0; edge < count; ++edge) {
    const std::size_t next_edge = (edge + 1U) % count;
    while (true) {
      const std::size_t next_opposite = (opposite + 1U) % count;
      const std::int64_t current_area =
          twice_triangle_area(hull[edge], hull[next_edge], hull[opposite]);
      const std::int64_t next_area = twice_triangle_area(
          hull[edge], hull[next_edge], hull[next_opposite]);
      if (next_area <= current_area) {
        break;
      }
      opposite = next_opposite;
    }

    consider_pair(hull[edge], hull[opposite], best);
    consider_pair(hull[next_edge], hull[opposite], best);

    const std::size_t next_opposite = (opposite + 1U) % count;
    const std::int64_t current_area =
        twice_triangle_area(hull[edge], hull[next_edge], hull[opposite]);
    const std::int64_t next_area =
        twice_triangle_area(hull[edge], hull[next_edge], hull[next_opposite]);
    if (next_area == current_area) {
      consider_pair(hull[edge], hull[next_opposite], best);
      consider_pair(hull[next_edge], hull[next_opposite], best);
    }
  }

  return best;
}

}  // namespace algorithms::geometry
