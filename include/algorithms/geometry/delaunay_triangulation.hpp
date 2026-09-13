#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::geometry {

inline constexpr std::int32_t kDelaunayCoordinateLimit = 10'000;

struct DelaunayTriangle {
  std::array<std::size_t, 3> vertices{};
  friend bool operator==(const DelaunayTriangle&, const DelaunayTriangle&) = default;
};

struct DelaunayEdge {
  std::size_t first{};
  std::size_t second{};
  friend bool operator==(const DelaunayEdge&, const DelaunayEdge&) = default;
};

struct DelaunayTriangulation {
  std::vector<DelaunayTriangle> triangles;
  std::vector<DelaunayEdge> edges;
  friend bool operator==(const DelaunayTriangulation&, const DelaunayTriangulation&) = default;
};

namespace delaunay_detail {

[[nodiscard]] inline std::int64_t orientation_det(Point2i a, Point2i b,
                                                   Point2i c) noexcept {
  const std::int64_t ab_x = static_cast<std::int64_t>(b.x) - a.x;
  const std::int64_t ab_y = static_cast<std::int64_t>(b.y) - a.y;
  const std::int64_t ac_x = static_cast<std::int64_t>(c.x) - a.x;
  const std::int64_t ac_y = static_cast<std::int64_t>(c.y) - a.y;
  return ab_x * ac_y - ab_y * ac_x;
}

// For counterclockwise a,b,c, positive means d lies strictly inside the
// circumcircle, negative means strictly outside, and zero means cocircular.
// With every public coordinate in [-10'000,10'000], translated coordinate
// magnitudes are <= 20'000. Each squared norm is <= 8e8 and each 2D cross
// product is <= 8e8, so each product below is <= 6.4e17 and the sum of three
// signed terms is bounded by 1.92e18 < INT64_MAX.
[[nodiscard]] inline std::int64_t incircle_det(Point2i a, Point2i b,
                                                Point2i c,
                                                Point2i d) noexcept {
  const std::int64_t ax = static_cast<std::int64_t>(a.x) - d.x;
  const std::int64_t ay = static_cast<std::int64_t>(a.y) - d.y;
  const std::int64_t bx = static_cast<std::int64_t>(b.x) - d.x;
  const std::int64_t by = static_cast<std::int64_t>(b.y) - d.y;
  const std::int64_t cx = static_cast<std::int64_t>(c.x) - d.x;
  const std::int64_t cy = static_cast<std::int64_t>(c.y) - d.y;

  const std::int64_t a_norm = ax * ax + ay * ay;
  const std::int64_t b_norm = bx * bx + by * by;
  const std::int64_t c_norm = cx * cx + cy * cy;
  const std::int64_t bc_cross = bx * cy - by * cx;
  const std::int64_t ac_cross = ax * cy - ay * cx;
  const std::int64_t ab_cross = ax * by - ay * bx;
  return a_norm * bc_cross - b_norm * ac_cross + c_norm * ab_cross;
}

[[nodiscard]] inline DelaunayTriangle canonical_ccw_triangle(
    std::size_t first, std::size_t second, std::size_t third,
    std::span<const Point2i> points) {
  if (orientation_det(points[first], points[second], points[third]) > 0) {
    return DelaunayTriangle{{first, second, third}};
  }
  return DelaunayTriangle{{first, third, second}};
}

inline void validate_input(std::span<const Point2i> points) {
  for (const Point2i point : points) {
    if (point.x < -kDelaunayCoordinateLimit ||
        point.x > kDelaunayCoordinateLimit ||
        point.y < -kDelaunayCoordinateLimit ||
        point.y > kDelaunayCoordinateLimit) {
      throw std::out_of_range("Delaunay point exceeds exact coordinate domain");
    }
  }

  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      if (points[i] == points[j]) {
        throw std::invalid_argument("Delaunay input contains duplicate points");
      }
    }
  }

  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      for (std::size_t k = j + 1; k < points.size(); ++k) {
        if (orientation_det(points[i], points[j], points[k]) == 0) {
          throw std::invalid_argument(
              "Delaunay general-position contract rejects collinear triples");
        }
      }
    }
  }

  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      for (std::size_t k = j + 1; k < points.size(); ++k) {
        const DelaunayTriangle triangle =
            canonical_ccw_triangle(i, j, k, points);
        const Point2i a = points[triangle.vertices[0]];
        const Point2i b = points[triangle.vertices[1]];
        const Point2i c = points[triangle.vertices[2]];
        for (std::size_t l = k + 1; l < points.size(); ++l) {
          if (incircle_det(a, b, c, points[l]) == 0) {
            throw std::invalid_argument(
                "Delaunay general-position contract rejects cocircular quadruples");
          }
        }
      }
    }
  }
}

}  // namespace delaunay_detail

// Exact general-position Delaunay triangulation over the bounded integer domain.
// Returned triangle vertices are counterclockwise input indices. For a fixed
// input order, triangles follow increasing index triples. Returned undirected
// edges are canonical (first < second), sorted, and unique.
//
// This direct educational baseline enumerates every point triple and tests its
// circumcircle against every other point. It therefore uses O(n^4) time. The
// output and temporary edge list use O(n^2) storage in this conservative code-
// local bound. No incremental / divide-and-conquer / optimal O(n log n) claim is
// made.
[[nodiscard]] inline DelaunayTriangulation exact_delaunay_triangulation(
    std::span<const Point2i> points) {
  delaunay_detail::validate_input(points);

  DelaunayTriangulation result;
  if (points.size() < 2) {
    return result;
  }
  if (points.size() == 2) {
    result.edges.push_back(DelaunayEdge{0, 1});
    return result;
  }

  for (std::size_t i = 0; i < points.size(); ++i) {
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      for (std::size_t k = j + 1; k < points.size(); ++k) {
        const DelaunayTriangle triangle =
            delaunay_detail::canonical_ccw_triangle(i, j, k, points);
        const Point2i a = points[triangle.vertices[0]];
        const Point2i b = points[triangle.vertices[1]];
        const Point2i c = points[triangle.vertices[2]];

        bool empty_circle = true;
        for (std::size_t p = 0; p < points.size(); ++p) {
          if (p == i || p == j || p == k) {
            continue;
          }
          if (delaunay_detail::incircle_det(a, b, c, points[p]) > 0) {
            empty_circle = false;
            break;
          }
        }
        if (!empty_circle) {
          continue;
        }

        result.triangles.push_back(triangle);
        for (const auto& edge : std::array<std::pair<std::size_t, std::size_t>, 3>{
                 std::pair{triangle.vertices[0], triangle.vertices[1]},
                 std::pair{triangle.vertices[1], triangle.vertices[2]},
                 std::pair{triangle.vertices[2], triangle.vertices[0]}}) {
          const auto [first, second] = std::minmax(edge.first, edge.second);
          result.edges.push_back(DelaunayEdge{first, second});
        }
      }
    }
  }

  std::sort(result.edges.begin(), result.edges.end(),
            [](const DelaunayEdge& first, const DelaunayEdge& second) {
              return std::pair{first.first, first.second} <
                     std::pair{second.first, second.second};
            });
  result.edges.erase(
      std::unique(result.edges.begin(), result.edges.end()), result.edges.end());
  return result;
}

}  // namespace algorithms::geometry
