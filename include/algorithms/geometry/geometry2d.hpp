#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::geometry {

struct Point2i {
  std::int32_t x;
  std::int32_t y;

  friend bool operator==(const Point2i&, const Point2i&) = default;
};

enum class Orientation {
  Clockwise = -1,
  Collinear = 0,
  Counterclockwise = 1,
};

// Exact for the accepted coordinate domain [-1'000'000'000, 1'000'000'000].
[[nodiscard]] Orientation orientation(Point2i a, Point2i b, Point2i c);

// Inclusive endpoint/boundary test. Returns false for a non-collinear point.
[[nodiscard]] bool on_segment(Point2i a, Point2i b, Point2i point);

// Inclusive segment intersection: proper crossings, shared endpoints, and
// collinear overlap all count as intersections.
[[nodiscard]] bool segments_intersect(Point2i a, Point2i b, Point2i c,
                                      Point2i d);

// Returns unique hull vertices in counterclockwise order, starting at the
// lexicographically smallest point. Interior collinear boundary points are
// excluded. All-collinear input returns the two extremes (or fewer if unique
// input has size < 2).
[[nodiscard]] std::vector<Point2i> convex_hull(std::span<const Point2i> points);

}  // namespace algorithms::geometry
