#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::geometry {

struct OrthogonalRangeCountResult {
  std::size_t count = 0;
  std::size_t canonical_nodes = 0;
  std::size_t secondary_binary_searches = 0;

  friend bool operator==(const OrthogonalRangeCountResult&,
                         const OrthogonalRangeCountResult&) = default;
};

class OrthogonalRangeTree2D {
 public:
  explicit OrthogonalRangeTree2D(std::vector<Point2i> points)
      : points_(std::move(points)) {
    std::sort(points_.begin(), points_.end(), point_less);
    build();
  }

  [[nodiscard]] std::size_t size() const noexcept { return points_.size(); }
  [[nodiscard]] bool empty() const noexcept { return points_.empty(); }

  // Counts input copies in the inclusive axis-aligned rectangle.
  [[nodiscard]] OrthogonalRangeCountResult count_closed(Point2i lower,
                                                         Point2i upper) const {
    if (lower.x > upper.x || lower.y > upper.y) {
      throw std::invalid_argument("orthogonal range bounds are inverted");
    }

    OrthogonalRangeCountResult result;
    if (points_.empty()) {
      return result;
    }

    const std::size_t left = lower_bound_x(lower.x);
    const std::size_t right = upper_bound_x(upper.x);
    if (left == right) {
      return result;
    }

    std::size_t l = base_ + left;
    std::size_t r = base_ + right;
    while (l < r) {
      if ((l & 1U) != 0U) {
        consume_node(l, lower.y, upper.y, result);
        ++l;
      }
      if ((r & 1U) != 0U) {
        --r;
        consume_node(r, lower.y, upper.y, result);
      }
      l /= 2U;
      r /= 2U;
    }
    return result;
  }

  [[nodiscard]] bool valid_structure() const {
    if (base_ == 0U || (base_ & (base_ - 1U)) != 0U) {
      return false;
    }
    if (catalogs_.size() != 2U * base_) {
      return false;
    }
    for (std::size_t i = 1; i < points_.size(); ++i) {
      if (point_less(points_[i], points_[i - 1])) {
        return false;
      }
    }
    for (std::size_t leaf = 0; leaf < base_; ++leaf) {
      const auto& catalog = catalogs_[base_ + leaf];
      if (leaf < points_.size()) {
        if (catalog != std::vector<std::int32_t>{points_[leaf].y}) {
          return false;
        }
      } else if (!catalog.empty()) {
        return false;
      }
    }
    for (std::size_t node = base_ - 1U; node > 0U; --node) {
      std::vector<std::int32_t> expected;
      const auto& left = catalogs_[2U * node];
      const auto& right = catalogs_[2U * node + 1U];
      expected.reserve(left.size() + right.size());
      std::merge(left.begin(), left.end(), right.begin(), right.end(),
                 std::back_inserter(expected));
      if (expected != catalogs_[node]) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<Point2i> points_;
  std::size_t base_ = 1U;
  std::vector<std::vector<std::int32_t>> catalogs_;

  static bool point_less(Point2i left, Point2i right) noexcept {
    if (left.x != right.x) {
      return left.x < right.x;
    }
    return left.y < right.y;
  }

  void build() {
    base_ = 1U;
    const std::size_t max_half = std::numeric_limits<std::size_t>::max() / 2U;
    while (base_ < points_.size()) {
      if (base_ > max_half) {
        throw std::length_error("orthogonal range tree is too large");
      }
      base_ *= 2U;
    }
    if (base_ > max_half) {
      throw std::length_error("orthogonal range tree is too large");
    }
    catalogs_.assign(2U * base_, {});
    for (std::size_t i = 0; i < points_.size(); ++i) {
      catalogs_[base_ + i].push_back(points_[i].y);
    }
    for (std::size_t node = base_ - 1U; node > 0U; --node) {
      const auto& left = catalogs_[2U * node];
      const auto& right = catalogs_[2U * node + 1U];
      auto& output = catalogs_[node];
      output.reserve(left.size() + right.size());
      std::merge(left.begin(), left.end(), right.begin(), right.end(),
                 std::back_inserter(output));
    }
  }

  [[nodiscard]] std::size_t lower_bound_x(std::int32_t x) const noexcept {
    std::size_t low = 0U;
    std::size_t high = points_.size();
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (points_[mid].x < x) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  [[nodiscard]] std::size_t upper_bound_x(std::int32_t x) const noexcept {
    std::size_t low = 0U;
    std::size_t high = points_.size();
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (points_[mid].x <= x) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  static std::size_t lower_bound_y(const std::vector<std::int32_t>& values,
                                   std::int32_t y) noexcept {
    std::size_t low = 0U;
    std::size_t high = values.size();
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (values[mid] < y) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  static std::size_t upper_bound_y(const std::vector<std::int32_t>& values,
                                   std::int32_t y) noexcept {
    std::size_t low = 0U;
    std::size_t high = values.size();
    while (low < high) {
      const std::size_t mid = low + (high - low) / 2U;
      if (values[mid] <= y) {
        low = mid + 1U;
      } else {
        high = mid;
      }
    }
    return low;
  }

  void consume_node(std::size_t node, std::int32_t lower_y,
                    std::int32_t upper_y,
                    OrthogonalRangeCountResult& result) const {
    const auto& catalog = catalogs_[node];
    const std::size_t first = lower_bound_y(catalog, lower_y);
    const std::size_t last = upper_bound_y(catalog, upper_y);
    result.count += last - first;
    ++result.canonical_nodes;
    result.secondary_binary_searches += 2U;
  }
};

}  // namespace algorithms::geometry
