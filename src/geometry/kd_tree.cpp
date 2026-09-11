#include "algorithms/geometry/kd_tree.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace algorithms::geometry {
namespace {

constexpr std::int32_t coordinate_limit = 1'000'000'000;

void validate_point(Point2i point) {
  if (point.x < -coordinate_limit || point.x > coordinate_limit ||
      point.y < -coordinate_limit || point.y > coordinate_limit) {
    throw std::out_of_range("KD-tree point outside exact coordinate domain");
  }
}

bool lex_less(Point2i left, Point2i right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

bool axis_less(Point2i left, Point2i right, std::uint8_t axis) {
  if (axis == 0U) {
    return left.x < right.x || (left.x == right.x && left.y < right.y);
  }
  return left.y < right.y || (left.y == right.y && left.x < right.x);
}

std::int64_t squared_distance(Point2i first, Point2i second) {
  const std::int64_t dx = static_cast<std::int64_t>(first.x) - second.x;
  const std::int64_t dy = static_cast<std::int64_t>(first.y) - second.y;
  return dx * dx + dy * dy;
}

std::int64_t box_lower_bound(Point2i query, std::int32_t min_x,
                             std::int32_t max_x, std::int32_t min_y,
                             std::int32_t max_y) {
  std::int64_t dx = 0;
  std::int64_t dy = 0;
  if (query.x < min_x) {
    dx = static_cast<std::int64_t>(min_x) - query.x;
  } else if (query.x > max_x) {
    dx = static_cast<std::int64_t>(query.x) - max_x;
  }
  if (query.y < min_y) {
    dy = static_cast<std::int64_t>(min_y) - query.y;
  } else if (query.y > max_y) {
    dy = static_cast<std::int64_t>(query.y) - max_y;
  }
  return dx * dx + dy * dy;
}

bool better(Point2i point, std::int64_t distance,
            const KdNearestResult& current) {
  if (distance != current.squared_distance) {
    return distance < current.squared_distance;
  }
  return lex_less(point, current.point);
}

}  // namespace

KdTree2D::KdTree2D(std::span<const Point2i> points) {
  std::vector<Point2i> unique(points.begin(), points.end());
  for (const Point2i point : unique) {
    validate_point(point);
  }
  std::sort(unique.begin(), unique.end(), lex_less);
  unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
  nodes_.reserve(unique.size());
  root_ = build(unique, 0U, unique.size(), 0U);
}

bool KdTree2D::empty() const noexcept { return nodes_.empty(); }

std::size_t KdTree2D::size() const noexcept { return nodes_.size(); }

std::size_t KdTree2D::height() const noexcept { return compute_height(root_); }

std::size_t KdTree2D::build(std::vector<Point2i>& points, std::size_t begin,
                            std::size_t end, std::size_t depth) {
  if (begin == end) {
    return no_node;
  }

  const std::uint8_t axis = static_cast<std::uint8_t>(depth & 1U);
  std::sort(points.begin() + static_cast<std::ptrdiff_t>(begin),
            points.begin() + static_cast<std::ptrdiff_t>(end),
            [axis](Point2i left, Point2i right) {
              return axis_less(left, right, axis);
            });
  const std::size_t middle = begin + (end - begin) / 2U;
  const Point2i pivot = points[middle];

  const std::size_t index = nodes_.size();
  nodes_.push_back(Node{pivot, no_node, no_node, pivot.x, pivot.x, pivot.y,
                        pivot.y, axis});

  const std::size_t left = build(points, begin, middle, depth + 1U);
  const std::size_t right = build(points, middle + 1U, end, depth + 1U);
  nodes_[index].left = left;
  nodes_[index].right = right;

  auto include_child = [this, index](std::size_t child) {
    if (child == no_node) {
      return;
    }
    nodes_[index].min_x = std::min(nodes_[index].min_x, nodes_[child].min_x);
    nodes_[index].max_x = std::max(nodes_[index].max_x, nodes_[child].max_x);
    nodes_[index].min_y = std::min(nodes_[index].min_y, nodes_[child].min_y);
    nodes_[index].max_y = std::max(nodes_[index].max_y, nodes_[child].max_y);
  };
  include_child(left);
  include_child(right);
  return index;
}

std::size_t KdTree2D::compute_height(std::size_t node) const noexcept {
  if (node == no_node) {
    return 0U;
  }
  return 1U + std::max(compute_height(nodes_[node].left),
                       compute_height(nodes_[node].right));
}

std::optional<KdNearestResult> KdTree2D::nearest(Point2i query) const {
  validate_point(query);
  if (root_ == no_node) {
    return std::nullopt;
  }

  KdNearestResult best{nodes_[root_].point,
                       squared_distance(query, nodes_[root_].point), 0U, 0U};
  search(root_, query, best);
  return best;
}

void KdTree2D::search(std::size_t node, Point2i query,
                      KdNearestResult& best) const {
  if (node == no_node) {
    return;
  }
  ++best.visited_nodes;
  const Node& current = nodes_[node];
  const std::int64_t distance = squared_distance(query, current.point);
  if (better(current.point, distance, best)) {
    best.point = current.point;
    best.squared_distance = distance;
  }

  struct Candidate {
    std::size_t node;
    std::int64_t lower_bound;
  };
  std::array<Candidate, 2> candidates{{{current.left, 0}, {current.right, 0}}};
  for (Candidate& candidate : candidates) {
    if (candidate.node != no_node) {
      const Node& child = nodes_[candidate.node];
      candidate.lower_bound = box_lower_bound(query, child.min_x, child.max_x,
                                              child.min_y, child.max_y);
    }
  }
  if (candidates[1].node != no_node &&
      (candidates[0].node == no_node ||
       candidates[1].lower_bound < candidates[0].lower_bound)) {
    std::swap(candidates[0], candidates[1]);
  }

  for (const Candidate candidate : candidates) {
    if (candidate.node == no_node) {
      continue;
    }
    if (candidate.lower_bound > best.squared_distance) {
      ++best.pruned_subtrees;
      continue;
    }
    search(candidate.node, query, best);
  }
}

bool KdTree2D::validate_subtree(std::size_t node, std::size_t depth,
                                std::vector<bool>& seen,
                                std::size_t& count) const {
  if (node == no_node) {
    return true;
  }
  if (node >= nodes_.size() || seen[node]) {
    return false;
  }
  seen[node] = true;
  ++count;
  const Node& current = nodes_[node];
  if (current.axis != static_cast<std::uint8_t>(depth & 1U)) {
    return false;
  }

  std::int32_t min_x = current.point.x;
  std::int32_t max_x = current.point.x;
  std::int32_t min_y = current.point.y;
  std::int32_t max_y = current.point.y;

  auto check_child = [&](std::size_t child, bool should_be_less) {
    if (child == no_node) {
      return true;
    }
    if (!validate_subtree(child, depth + 1U, seen, count)) {
      return false;
    }
    std::vector<std::size_t> stack{child};
    while (!stack.empty()) {
      const std::size_t descendant = stack.back();
      stack.pop_back();
      const bool is_less = axis_less(nodes_[descendant].point, current.point,
                                     current.axis);
      if (is_less != should_be_less) {
        return false;
      }
      if (nodes_[descendant].left != no_node) {
        stack.push_back(nodes_[descendant].left);
      }
      if (nodes_[descendant].right != no_node) {
        stack.push_back(nodes_[descendant].right);
      }
    }
    min_x = std::min(min_x, nodes_[child].min_x);
    max_x = std::max(max_x, nodes_[child].max_x);
    min_y = std::min(min_y, nodes_[child].min_y);
    max_y = std::max(max_y, nodes_[child].max_y);
    return true;
  };

  if (!check_child(current.left, true) || !check_child(current.right, false)) {
    return false;
  }
  return current.min_x == min_x && current.max_x == max_x &&
         current.min_y == min_y && current.max_y == max_y;
}

bool KdTree2D::valid_structure() const {
  if (root_ == no_node) {
    return nodes_.empty();
  }
  std::vector<bool> seen(nodes_.size(), false);
  std::size_t count = 0U;
  return validate_subtree(root_, 0U, seen, count) && count == nodes_.size();
}

}  // namespace algorithms::geometry
