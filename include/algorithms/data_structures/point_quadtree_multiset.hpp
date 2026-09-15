#pragma once

#include "algorithms/geometry/geometry2d.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

using algorithms::geometry::Point2i;

struct QuadtreePointMultiplicity {
  Point2i point;
  std::size_t count;
  friend bool operator==(const QuadtreePointMultiplicity&, const QuadtreePointMultiplicity&) = default;
};

class PointQuadtreeMultiset {
 public:
  static constexpr std::int32_t kMinCoordinate = -1'000'000'000;
  static constexpr std::int32_t kMaxCoordinate = 1'000'000'000;
  static constexpr std::size_t kLeafCapacity = 8;

  [[nodiscard]] bool empty() const noexcept { return !root_; }
  [[nodiscard]] std::size_t size() const noexcept { return root_ ? root_->total : 0; }
  [[nodiscard]] std::size_t distinct_size() const noexcept {
    return root_ ? root_->distinct : 0;
  }

  void insert(Point2i point) {
    validate_point(point);
    if (size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("quadtree multiplicity is not representable");
    }
    if (!root_) root_ = std::make_unique<Node>();
    insert_node(*root_, root_region(), point);
  }

  [[nodiscard]] bool erase_one(Point2i point) {
    validate_point(point);
    if (!root_) return false;
    const bool erased = erase_node(*root_, root_region(), point);
    if (erased && root_->total == 0) root_.reset();
    return erased;
  }

  [[nodiscard]] std::size_t count(Point2i point) const {
    validate_point(point);
    if (!root_) return 0;
    return count_node(*root_, root_region(), point);
  }

  [[nodiscard]] std::size_t count_closed(Point2i lower, Point2i upper) const {
    validate_rectangle(lower, upper);
    if (!root_) return 0;
    return count_range(*root_, root_region(), rectangle(lower, upper));
  }

  [[nodiscard]] std::vector<QuadtreePointMultiplicity> report_closed(
      Point2i lower, Point2i upper) const {
    validate_rectangle(lower, upper);
    std::vector<QuadtreePointMultiplicity> result;
    if (root_) report_range(*root_, root_region(), rectangle(lower, upper), result);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
      if (a.point.x != b.point.x) return a.point.x < b.point.x;
      return a.point.y < b.point.y;
    });
    return result;
  }

  [[nodiscard]] std::size_t node_count() const noexcept {
    return node_count(root_.get());
  }

  [[nodiscard]] std::size_t height() const noexcept { return height(root_.get()); }

  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      if (!root_) return true;
      return validate_node(*root_, root_region()).valid;
    } catch (...) {
      return false;
    }
  }

 private:
  struct Entry {
    Point2i point;
    std::size_t count = 0;
  };

  struct Node {
    std::array<std::unique_ptr<Node>, 4> children;
    std::vector<Entry> entries;
    std::size_t total = 0;
    std::size_t distinct = 0;

    [[nodiscard]] bool leaf() const noexcept {
      for (const auto& child : children) {
        if (child) return false;
      }
      return true;
    }
  };

  struct Region {
    std::int64_t x_low;
    std::int64_t x_high;
    std::int64_t y_low;
    std::int64_t y_high;
  };

  struct Validation {
    bool valid = true;
    std::size_t total = 0;
    std::size_t distinct = 0;
  };

  std::unique_ptr<Node> root_;

  [[nodiscard]] static constexpr Region root_region() noexcept {
    return {kMinCoordinate, kMaxCoordinate, kMinCoordinate, kMaxCoordinate};
  }

  [[nodiscard]] static constexpr Region rectangle(Point2i low, Point2i high) noexcept {
    return {low.x, high.x, low.y, high.y};
  }

  static void validate_point(Point2i point) {
    if (point.x < kMinCoordinate || point.x > kMaxCoordinate ||
        point.y < kMinCoordinate || point.y > kMaxCoordinate) {
      throw std::out_of_range("quadtree point outside exact coordinate domain");
    }
  }

  static void validate_rectangle(Point2i low, Point2i high) {
    validate_point(low);
    validate_point(high);
    if (low.x > high.x || low.y > high.y) {
      throw std::out_of_range("quadtree rectangle bounds are inverted");
    }
  }

  [[nodiscard]] static bool contains(const Region& region, Point2i point) noexcept {
    return static_cast<std::int64_t>(point.x) >= region.x_low &&
           static_cast<std::int64_t>(point.x) <= region.x_high &&
           static_cast<std::int64_t>(point.y) >= region.y_low &&
           static_cast<std::int64_t>(point.y) <= region.y_high;
  }

  [[nodiscard]] static bool intersects(const Region& a, const Region& b) noexcept {
    return a.x_low <= b.x_high && b.x_low <= a.x_high &&
           a.y_low <= b.y_high && b.y_low <= a.y_high;
  }

  [[nodiscard]] static bool covers(const Region& outer, const Region& inner) noexcept {
    return outer.x_low <= inner.x_low && outer.x_high >= inner.x_high &&
           outer.y_low <= inner.y_low && outer.y_high >= inner.y_high;
  }

  [[nodiscard]] static bool splittable(const Region& region) noexcept {
    return region.x_low < region.x_high || region.y_low < region.y_high;
  }

  [[nodiscard]] static std::int64_t midpoint(std::int64_t low,
                                              std::int64_t high) noexcept {
    return low + (high - low) / 2;
  }

  [[nodiscard]] static std::size_t child_index(const Region& region,
                                                Point2i point) noexcept {
    const std::int64_t x_mid = midpoint(region.x_low, region.x_high);
    const std::int64_t y_mid = midpoint(region.y_low, region.y_high);
    const std::size_t x_bit = static_cast<std::int64_t>(point.x) > x_mid ? 1U : 0U;
    const std::size_t y_bit = static_cast<std::int64_t>(point.y) > y_mid ? 2U : 0U;
    return x_bit | y_bit;
  }

  [[nodiscard]] static Region child_region(const Region& region,
                                            std::size_t index) noexcept {
    const std::int64_t x_mid = midpoint(region.x_low, region.x_high);
    const std::int64_t y_mid = midpoint(region.y_low, region.y_high);
    const bool high_x = (index & 1U) != 0U;
    const bool high_y = (index & 2U) != 0U;
    const std::int64_t x_low = high_x ? x_mid + 1 : region.x_low;
    const std::int64_t x_high = high_x ? region.x_high : x_mid;
    const std::int64_t y_low = high_y ? y_mid + 1 : region.y_low;
    const std::int64_t y_high = high_y ? region.y_high : y_mid;
    return {x_low, x_high, y_low, y_high};
  }

  static void recompute(Node& node) {
    std::size_t total = 0;
    std::size_t distinct = 0;
    if (node.leaf()) {
      for (const Entry& entry : node.entries) {
        total += entry.count;
      }
      distinct = node.entries.size();
    } else {
      for (const auto& child : node.children) {
        if (!child) continue;
        total += child->total;
        distinct += child->distinct;
      }
    }
    node.total = total;
    node.distinct = distinct;
  }

  static void insert_existing(Node& node, const Region& region, Entry entry) {
    if (node.leaf()) {
      node.entries.push_back(entry);
      node.total += entry.count;
      ++node.distinct;
      return;
    }
    const std::size_t index = child_index(region, entry.point);
    const Region child_bounds = child_region(region, index);
    if (!node.children[index]) node.children[index] = std::make_unique<Node>();
    insert_existing(*node.children[index], child_bounds, entry);
    recompute(node);
  }

  static void split_leaf(Node& node, const Region& region) {
    std::vector<Entry> entries = std::move(node.entries);
    node.entries.clear();
    node.total = 0;
    node.distinct = 0;
    for (Entry& entry : entries) {
      const std::size_t index = child_index(region, entry.point);
      const Region bounds = child_region(region, index);
      if (!node.children[index]) node.children[index] = std::make_unique<Node>();
      insert_existing(*node.children[index], bounds, std::move(entry));
    }
    recompute(node);
  }

  static void insert_node(Node& node, const Region& region, Point2i point) {
    if (node.leaf()) {
      for (Entry& entry : node.entries) {
        if (entry.point == point) {
          ++entry.count;
          ++node.total;
          return;
        }
      }
      if (node.entries.size() < kLeafCapacity || !splittable(region)) {
        node.entries.push_back({point, 1});
        ++node.total;
        ++node.distinct;
        return;
      }
      split_leaf(node, region);
    }
    const std::size_t index = child_index(region, point);
    const Region bounds = child_region(region, index);
    if (!node.children[index]) node.children[index] = std::make_unique<Node>();
    insert_node(*node.children[index], bounds, point);
    recompute(node);
  }

  static void gather_entries(const Node& node, std::vector<Entry>& entries) {
    if (node.leaf()) {
      entries.insert(entries.end(), node.entries.begin(), node.entries.end());
      return;
    }
    for (const auto& child : node.children) {
      if (child) gather_entries(*child, entries);
    }
  }

  static void compact_if_small(Node& node) {
    if (node.leaf() || node.distinct > kLeafCapacity) return;
    std::vector<Entry> entries;
    entries.reserve(node.distinct);
    gather_entries(node, entries);
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
      if (a.point.x != b.point.x) return a.point.x < b.point.x;
      return a.point.y < b.point.y;
    });
    for (auto& child : node.children) child.reset();
    node.entries = std::move(entries);
    recompute(node);
  }

  static bool erase_node(Node& node, const Region& region, Point2i point) {
    if (node.leaf()) {
      for (std::size_t i = 0; i < node.entries.size(); ++i) {
        Entry& entry = node.entries[i];
        if (!(entry.point == point)) continue;
        --entry.count;
        --node.total;
        if (entry.count == 0) {
          node.entries.erase(node.entries.begin() + static_cast<std::ptrdiff_t>(i));
          --node.distinct;
        }
        return true;
      }
      return false;
    }
    const std::size_t index = child_index(region, point);
    if (!node.children[index]) return false;
    const Region bounds = child_region(region, index);
    if (!erase_node(*node.children[index], bounds, point)) return false;
    if (node.children[index]->total == 0) node.children[index].reset();
    recompute(node);
    compact_if_small(node);
    return true;
  }

  [[nodiscard]] static std::size_t count_node(const Node& node,
                                               const Region& region,
                                               Point2i point) {
    if (node.leaf()) {
      for (const Entry& entry : node.entries) {
        if (entry.point == point) return entry.count;
      }
      return 0;
    }
    const std::size_t index = child_index(region, point);
    if (!node.children[index]) return 0;
    return count_node(*node.children[index], child_region(region, index), point);
  }

  [[nodiscard]] static std::size_t count_range(const Node& node,
                                                const Region& region,
                                                const Region& query) {
    if (!intersects(region, query)) return 0;
    if (covers(query, region)) return node.total;
    if (node.leaf()) {
      std::size_t total = 0;
      for (const Entry& entry : node.entries) {
        if (contains(query, entry.point)) total += entry.count;
      }
      return total;
    }
    std::size_t total = 0;
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      if (node.children[index]) {
        total += count_range(*node.children[index], child_region(region, index), query);
      }
    }
    return total;
  }

  static void report_range(const Node& node, const Region& region,
                           const Region& query,
                           std::vector<QuadtreePointMultiplicity>& output) {
    if (!intersects(region, query)) return;
    if (node.leaf()) {
      for (const Entry& entry : node.entries) {
        if (contains(query, entry.point)) output.push_back({entry.point, entry.count});
      }
      return;
    }
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      if (node.children[index]) {
        report_range(*node.children[index], child_region(region, index), query, output);
      }
    }
  }

  [[nodiscard]] static std::size_t node_count(const Node* node) noexcept {
    if (!node) return 0;
    std::size_t count = 1;
    for (const auto& child : node->children) count += node_count(child.get());
    return count;
  }

  [[nodiscard]] static std::size_t height(const Node* node) noexcept {
    if (!node) return 0;
    std::size_t child_height = 0;
    for (const auto& child : node->children) {
      child_height = std::max(child_height, height(child.get()));
    }
    return 1 + child_height;
  }

  [[nodiscard]] static Validation validate_node(const Node& node,
                                                const Region& region) {
    if (node.total == 0 || node.distinct == 0) return {false, 0, 0};
    if (node.leaf()) {
      if (node.entries.size() != node.distinct ||
          (node.entries.size() > kLeafCapacity && splittable(region))) {
        return {false, 0, 0};
      }
      std::size_t total = 0;
      for (std::size_t i = 0; i < node.entries.size(); ++i) {
        const Entry& entry = node.entries[i];
        if (entry.count == 0 || !contains(region, entry.point)) return {false, 0, 0};
        for (std::size_t j = i + 1; j < node.entries.size(); ++j) {
          if (entry.point == node.entries[j].point) return {false, 0, 0};
        }
        if (total > std::numeric_limits<std::size_t>::max() - entry.count) {
          return {false, 0, 0};
        }
        total += entry.count;
      }
      return {total == node.total, total, node.entries.size()};
    }
    if (!node.entries.empty() || node.distinct <= kLeafCapacity) return {false, 0, 0};
    std::size_t total = 0;
    std::size_t distinct = 0;
    bool any_child = false;
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      const auto& child = node.children[index];
      if (!child) continue;
      any_child = true;
      const Region bounds = child_region(region, index);
      if (bounds.x_low > bounds.x_high || bounds.y_low > bounds.y_high) {
        return {false, 0, 0};
      }
      const Validation child_value = validate_node(*child, bounds);
      if (!child_value.valid ||
          total > std::numeric_limits<std::size_t>::max() - child_value.total ||
          distinct > std::numeric_limits<std::size_t>::max() - child_value.distinct) {
        return {false, 0, 0};
      }
      total += child_value.total;
      distinct += child_value.distinct;
    }
    return {any_child && total == node.total && distinct == node.distinct,
            total, distinct};
  }
};

}  // namespace algorithms::data_structures
