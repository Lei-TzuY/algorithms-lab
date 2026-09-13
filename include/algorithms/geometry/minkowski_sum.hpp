#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::geometry {
namespace minkowski_detail {

constexpr std::int32_t kMinkowskiInputLimit = 500'000'000;

struct EdgeVector {
  std::int64_t x;
  std::int64_t y;
};

inline void validate_input(std::span<const Point2i> points) {
  for (const Point2i point : points) {
    if (point.x < -kMinkowskiInputLimit || point.x > kMinkowskiInputLimit ||
        point.y < -kMinkowskiInputLimit || point.y > kMinkowskiInputLimit) {
      throw std::out_of_range("Minkowski input coordinate outside composable exact domain");
    }
  }
}

[[nodiscard]] inline bool lex_less(const Point2i left, const Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

[[nodiscard]] inline bool y_then_x_less(const Point2i left, const Point2i right) {
  return left.y < right.y || (left.y == right.y && left.x < right.x);
}

[[nodiscard]] inline Point2i add_points(const Point2i left, const Point2i right) {
  const std::int64_t x = static_cast<std::int64_t>(left.x) + right.x;
  const std::int64_t y = static_cast<std::int64_t>(left.y) + right.y;
  if (x < -1'000'000'000LL || x > 1'000'000'000LL ||
      y < -1'000'000'000LL || y > 1'000'000'000LL) {
    throw std::overflow_error("Minkowski output escaped exact geometry domain");
  }
  return Point2i{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

[[nodiscard]] inline int polar_half(const EdgeVector edge) {
  return (edge.y > 0 || (edge.y == 0 && edge.x >= 0)) ? 0 : 1;
}

[[nodiscard]] inline int compare_polar(const EdgeVector left,
                                       const EdgeVector right) {
  const int left_half = polar_half(left);
  const int right_half = polar_half(right);
  if (left_half != right_half) {
    return left_half < right_half ? -1 : 1;
  }
  const std::int64_t cross = left.x * right.y - left.y * right.x;
  if (cross > 0) {
    return -1;
  }
  if (cross < 0) {
    return 1;
  }
  return 0;
}

inline void rotate_to_lowest(std::vector<Point2i>& hull) {
  if (hull.empty()) {
    return;
  }
  const auto first = std::min_element(hull.begin(), hull.end(), y_then_x_less);
  std::rotate(hull.begin(), first, hull.end());
}

inline void rotate_to_lexicographic_start(std::vector<Point2i>& hull) {
  if (hull.empty()) {
    return;
  }
  const auto first = std::min_element(hull.begin(), hull.end(), lex_less);
  std::rotate(hull.begin(), first, hull.end());
}

[[nodiscard]] inline std::vector<EdgeVector> edge_vectors(
    const std::vector<Point2i>& hull) {
  std::vector<EdgeVector> edges;
  edges.reserve(hull.size());
  for (std::size_t index = 0; index < hull.size(); ++index) {
    const Point2i from = hull[index];
    const Point2i to = hull[(index + 1U) % hull.size()];
    edges.push_back(EdgeVector{static_cast<std::int64_t>(to.x) - from.x,
                               static_cast<std::int64_t>(to.y) - from.y});
  }
  return edges;
}

[[nodiscard]] inline std::vector<Point2i> degenerate_sum(
    const std::vector<Point2i>& left, const std::vector<Point2i>& right) {
  std::vector<Point2i> sums;
  if (right.size() != 0U && left.size() > static_cast<std::size_t>(-1) / right.size()) {
    throw std::length_error("Minkowski degenerate fallback size overflow");
  }
  sums.reserve(left.size() * right.size());
  for (const Point2i a : left) {
    for (const Point2i b : right) {
      sums.push_back(add_points(a, b));
    }
  }
  return convex_hull(sums);
}

}  // namespace minkowski_detail

// Returns the canonical convex hull of conv(left) + conv(right).
// Input coordinates are limited to [-500'000'000, 500'000'000] so every
// returned point remains inside the repository's exact geometry domain.
// Empty input yields the empty Minkowski sum.
[[nodiscard]] inline std::vector<Point2i> convex_minkowski_sum(
    std::span<const Point2i> left_points,
    std::span<const Point2i> right_points) {
  using namespace minkowski_detail;
  validate_input(left_points);
  validate_input(right_points);

  if (left_points.empty() || right_points.empty()) {
    return {};
  }

  std::vector<Point2i> left = convex_hull(left_points);
  std::vector<Point2i> right = convex_hull(right_points);

  if (left.size() < 3U || right.size() < 3U) {
    return degenerate_sum(left, right);
  }

  rotate_to_lowest(left);
  rotate_to_lowest(right);
  const std::vector<EdgeVector> left_edges = edge_vectors(left);
  const std::vector<EdgeVector> right_edges = edge_vectors(right);

  Point2i current = add_points(left.front(), right.front());
  std::vector<Point2i> result;
  result.reserve(left.size() + right.size());
  result.push_back(current);

  std::size_t left_index = 0;
  std::size_t right_index = 0;
  while (left_index < left_edges.size() || right_index < right_edges.size()) {
    EdgeVector step{0, 0};
    if (right_index == right_edges.size()) {
      step = left_edges[left_index++];
    } else if (left_index == left_edges.size()) {
      step = right_edges[right_index++];
    } else {
      const int ordering =
          compare_polar(left_edges[left_index], right_edges[right_index]);
      if (ordering < 0) {
        step = left_edges[left_index++];
      } else if (ordering > 0) {
        step = right_edges[right_index++];
      } else {
        step = EdgeVector{left_edges[left_index].x + right_edges[right_index].x,
                          left_edges[left_index].y + right_edges[right_index].y};
        ++left_index;
        ++right_index;
      }
    }

    const std::int64_t next_x = static_cast<std::int64_t>(current.x) + step.x;
    const std::int64_t next_y = static_cast<std::int64_t>(current.y) + step.y;
    if (next_x < -1'000'000'000LL || next_x > 1'000'000'000LL ||
        next_y < -1'000'000'000LL || next_y > 1'000'000'000LL) {
      throw std::overflow_error("Minkowski edge walk escaped exact geometry domain");
    }
    current = Point2i{static_cast<std::int32_t>(next_x),
                      static_cast<std::int32_t>(next_y)};
    if (left_index != left_edges.size() || right_index != right_edges.size()) {
      result.push_back(current);
    }
  }

  if (!(current == result.front())) {
    throw std::logic_error("Minkowski edge walk did not close");
  }
  rotate_to_lexicographic_start(result);
  return result;
}

}  // namespace algorithms::geometry
