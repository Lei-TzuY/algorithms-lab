#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Mutable 2D R-tree index over closed int32 axis-aligned rectangles.
//
// Nodes have a fixed maximum occupancy of 8 entries and a minimum non-root
// occupancy of 4 entries. Insertion descends by minimum bounding-rectangle
// enlargement and uses a deterministic area-spread split on overflow.
//
// This slice intentionally provides insertion and overlap search only. It does
// not claim R*-tree reinsertion, deletion/condensation, bulk loading,
// persistence, serialization compatibility, or benchmark-backed performance.
class RTree2D {
 public:
  struct Rect {
    std::int32_t min_x{};
    std::int32_t min_y{};
    std::int32_t max_x{};
    std::int32_t max_y{};

    friend bool operator==(const Rect&, const Rect&) = default;
  };

  using Id = std::size_t;

  static constexpr std::size_t kMaxEntries = 8U;
  static constexpr std::size_t kMinEntries = 4U;

  RTree2D();

  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }

  // Number of node levels. Empty tree has height 0; a non-empty root leaf has
  // height 1.
  [[nodiscard]] std::size_t height() const noexcept {
    if (size_ == 0U || root_ == nullptr) {
      return 0U;
    }
    std::size_t result = 1U;
    const Node* node = root_.get();
    while (!node->leaf) {
      if (node->children.empty() || node->children.front() == nullptr) {
        return 0U;
      }
      node = node->children.front().get();
      ++result;
    }
    return result;
  }

  // Inserts one rectangle and returns its stable insertion id.
  Id insert(const Rect rect) {
    validate_rect_or_throw(rect);
    if (size_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("R-tree insertion id space exhausted");
    }

    const Id id = size_;
    std::unique_ptr<Node> sibling =
        insert_recursive(*root_, rect, id);

    if (sibling != nullptr) {
      auto new_root = std::make_unique<Node>(false);
      new_root->boxes.reserve(2U);
      new_root->children.reserve(2U);

      new_root->boxes.push_back(bounds(*root_));
      new_root->children.push_back(std::move(root_));
      new_root->boxes.push_back(bounds(*sibling));
      new_root->children.push_back(std::move(sibling));
      root_ = std::move(new_root);
    }

    ++size_;
    return id;
  }

  // Returns insertion ids of all indexed rectangles that intersect query.
  // Results are sorted by id for deterministic public behavior.
  [[nodiscard]] std::vector<Id> query_intersect(
      const Rect query) const {
    validate_rect_or_throw(query);

    std::vector<Id> result;
    if (size_ == 0U) {
      return result;
    }

    query_recursive(*root_, query, result);
    std::sort(result.begin(), result.end());
    return result;
  }

  // Expensive structural replay, excluded from operation-complexity claims.
  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      if (root_ == nullptr) {
        return false;
      }

      if (size_ == 0U) {
        return root_->leaf && root_->boxes.empty() &&
               root_->ids.empty() && root_->children.empty();
      }

      std::vector<bool> seen(size_, false);
      std::optional<std::size_t> leaf_depth;
      std::size_t counted = 0U;
      if (!validate_node(*root_, true, 0U, leaf_depth, seen, counted)) {
        return false;
      }
      if (counted != size_) {
        return false;
      }
      return std::all_of(
          seen.begin(), seen.end(),
          [](const bool value) { return value; });
    } catch (...) {
      return false;
    }
  }

 private:
  struct Node {
    explicit Node(const bool is_leaf) : leaf(is_leaf) {}

    bool leaf{};
    std::vector<Rect> boxes;
    std::vector<Id> ids;
    std::vector<std::unique_ptr<Node>> children;
  };

  struct SplitEntry {
    Rect box;
    Id id{};
    std::unique_ptr<Node> child;
  };

  std::unique_ptr<Node> root_;
  std::size_t size_{};

  [[nodiscard]] static bool valid_rect(const Rect rect) noexcept {
    return rect.min_x <= rect.max_x && rect.min_y <= rect.max_y;
  }

  static void validate_rect_or_throw(const Rect rect) {
    if (!valid_rect(rect)) {
      throw std::invalid_argument(
          "R-tree rectangle has reversed bounds");
    }
  }

  [[nodiscard]] static bool intersects(
      const Rect a, const Rect b) noexcept {
    return a.min_x <= b.max_x && b.min_x <= a.max_x &&
           a.min_y <= b.max_y && b.min_y <= a.max_y;
  }

  [[nodiscard]] static Rect combine(
      const Rect a, const Rect b) noexcept {
    return Rect{
        std::min(a.min_x, b.min_x),
        std::min(a.min_y, b.min_y),
        std::max(a.max_x, b.max_x),
        std::max(a.max_y, b.max_y)};
  }

  [[nodiscard]] static std::uint64_t area(
      const Rect rect) noexcept {
    const std::uint64_t width = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(rect.max_x) -
        static_cast<std::int64_t>(rect.min_x));
    const std::uint64_t height = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(rect.max_y) -
        static_cast<std::int64_t>(rect.min_y));
    return width * height;
  }

  [[nodiscard]] static std::uint64_t enlargement(
      const Rect base, const Rect added) noexcept {
    return area(combine(base, added)) - area(base);
  }

  [[nodiscard]] static Rect bounds(const Node& node) {
    if (node.boxes.empty()) {
      throw std::logic_error("R-tree node has no bounding entries");
    }

    Rect result = node.boxes.front();
    for (std::size_t index = 1U; index < node.boxes.size(); ++index) {
      result = combine(result, node.boxes[index]);
    }
    return result;
  }

  [[nodiscard]] static std::size_t choose_subtree(
      const Node& node, const Rect rect) {
    if (node.leaf || node.children.empty() ||
        node.boxes.size() != node.children.size()) {
      throw std::logic_error("R-tree internal node shape invalid");
    }

    std::size_t best = 0U;
    std::uint64_t best_enlargement =
        enlargement(node.boxes[0U], rect);
    std::uint64_t best_area = area(node.boxes[0U]);
    std::size_t best_size = node.children[0U]->boxes.size();

    for (std::size_t index = 1U; index < node.boxes.size(); ++index) {
      const std::uint64_t candidate_enlargement =
          enlargement(node.boxes[index], rect);
      const std::uint64_t candidate_area = area(node.boxes[index]);
      const std::size_t candidate_size =
          node.children[index]->boxes.size();

      if (candidate_enlargement < best_enlargement ||
          (candidate_enlargement == best_enlargement &&
           (candidate_area < best_area ||
            (candidate_area == best_area &&
             candidate_size < best_size)))) {
        best = index;
        best_enlargement = candidate_enlargement;
        best_area = candidate_area;
        best_size = candidate_size;
      }
    }
    return best;
  }

  static void append_entry(
      Node& node, SplitEntry entry) {
    node.boxes.push_back(entry.box);
    if (node.leaf) {
      node.ids.push_back(entry.id);
    } else {
      if (entry.child == nullptr) {
        throw std::logic_error(
            "R-tree internal split entry has no child");
      }
      node.children.push_back(std::move(entry.child));
    }
  }

  [[nodiscard]] static std::unique_ptr<Node> split_node(
      Node& node) {
    const bool leaf = node.leaf;
    const std::size_t count = node.boxes.size();
    if (count != kMaxEntries + 1U) {
      throw std::logic_error(
          "R-tree split requires exactly one overflow entry");
    }
    if ((leaf && node.ids.size() != count) ||
        (!leaf && node.children.size() != count)) {
      throw std::logic_error("R-tree split source shape invalid");
    }

    std::vector<SplitEntry> entries;
    entries.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
      SplitEntry entry;
      entry.box = node.boxes[index];
      if (leaf) {
        entry.id = node.ids[index];
      } else {
        entry.child = std::move(node.children[index]);
      }
      entries.push_back(std::move(entry));
    }

    node.boxes.clear();
    node.ids.clear();
    node.children.clear();

    std::size_t first_seed = 0U;
    std::size_t second_seed = 1U;
    std::uint64_t largest_combined =
        area(combine(entries[0U].box, entries[1U].box));

    for (std::size_t first = 0U; first < count; ++first) {
      for (std::size_t second = first + 1U; second < count; ++second) {
        const std::uint64_t candidate =
            area(combine(entries[first].box, entries[second].box));
        if (candidate > largest_combined) {
          largest_combined = candidate;
          first_seed = first;
          second_seed = second;
        }
      }
    }

    auto sibling = std::make_unique<Node>(leaf);
    std::vector<bool> used(count, false);

    append_entry(node, std::move(entries[first_seed]));
    used[first_seed] = true;
    append_entry(*sibling, std::move(entries[second_seed]));
    used[second_seed] = true;

    std::size_t remaining = count - 2U;
    while (remaining != 0U) {
      if (node.boxes.size() + remaining == kMinEntries) {
        for (std::size_t index = 0U; index < count; ++index) {
          if (!used[index]) {
            append_entry(node, std::move(entries[index]));
            used[index] = true;
            --remaining;
          }
        }
        break;
      }
      if (sibling->boxes.size() + remaining == kMinEntries) {
        for (std::size_t index = 0U; index < count; ++index) {
          if (!used[index]) {
            append_entry(*sibling, std::move(entries[index]));
            used[index] = true;
            --remaining;
          }
        }
        break;
      }

      const Rect first_bounds = bounds(node);
      const Rect second_bounds = bounds(*sibling);

      std::size_t selected = count;
      std::uint64_t selected_difference = 0U;
      for (std::size_t index = 0U; index < count; ++index) {
        if (used[index]) {
          continue;
        }

        const std::uint64_t first_enlargement =
            enlargement(first_bounds, entries[index].box);
        const std::uint64_t second_enlargement =
            enlargement(second_bounds, entries[index].box);
        const std::uint64_t difference =
            first_enlargement >= second_enlargement
                ? first_enlargement - second_enlargement
                : second_enlargement - first_enlargement;

        if (selected == count ||
            difference > selected_difference) {
          selected = index;
          selected_difference = difference;
        }
      }

      if (selected == count) {
        throw std::logic_error(
            "R-tree split could not choose next entry");
      }

      const std::uint64_t first_enlargement =
          enlargement(first_bounds, entries[selected].box);
      const std::uint64_t second_enlargement =
          enlargement(second_bounds, entries[selected].box);
      const std::uint64_t first_area = area(first_bounds);
      const std::uint64_t second_area = area(second_bounds);

      bool choose_first = false;
      if (first_enlargement != second_enlargement) {
        choose_first = first_enlargement < second_enlargement;
      } else if (first_area != second_area) {
        choose_first = first_area < second_area;
      } else if (node.boxes.size() != sibling->boxes.size()) {
        choose_first =
            node.boxes.size() < sibling->boxes.size();
      } else {
        choose_first = true;
      }

      if (choose_first) {
        append_entry(node, std::move(entries[selected]));
      } else {
        append_entry(*sibling, std::move(entries[selected]));
      }
      used[selected] = true;
      --remaining;
    }

    return sibling;
  }

  [[nodiscard]] static std::unique_ptr<Node> insert_recursive(
      Node& node, const Rect rect, const Id id) {
    if (node.leaf) {
      node.boxes.push_back(rect);
      node.ids.push_back(id);
    } else {
      const std::size_t chosen = choose_subtree(node, rect);
      std::unique_ptr<Node> sibling =
          insert_recursive(*node.children[chosen], rect, id);
      node.boxes[chosen] = bounds(*node.children[chosen]);

      if (sibling != nullptr) {
        node.boxes.push_back(bounds(*sibling));
        node.children.push_back(std::move(sibling));
      }
    }

    if (node.boxes.size() <= kMaxEntries) {
      return nullptr;
    }
    return split_node(node);
  }

  static void query_recursive(
      const Node& node, const Rect query,
      std::vector<Id>& result) {
    if (node.leaf) {
      for (std::size_t index = 0U; index < node.boxes.size(); ++index) {
        if (intersects(node.boxes[index], query)) {
          result.push_back(node.ids[index]);
        }
      }
      return;
    }

    for (std::size_t index = 0U; index < node.boxes.size(); ++index) {
      if (intersects(node.boxes[index], query)) {
        query_recursive(*node.children[index], query, result);
      }
    }
  }

  [[nodiscard]] static bool validate_node(
      const Node& node, const bool is_root,
      const std::size_t depth,
      std::optional<std::size_t>& leaf_depth,
      std::vector<bool>& seen,
      std::size_t& counted) {
    const std::size_t entries = node.boxes.size();

    if (is_root) {
      if (node.leaf) {
        if (entries == 0U || entries > kMaxEntries) {
          return false;
        }
      } else if (entries < 2U || entries > kMaxEntries) {
        return false;
      }
    } else if (entries < kMinEntries || entries > kMaxEntries) {
      return false;
    }

    for (const Rect rect : node.boxes) {
      if (!valid_rect(rect)) {
        return false;
      }
    }

    if (node.leaf) {
      if (!node.children.empty() || node.ids.size() != entries) {
        return false;
      }
      if (!leaf_depth.has_value()) {
        leaf_depth = depth;
      } else if (*leaf_depth != depth) {
        return false;
      }

      for (const Id id : node.ids) {
        if (id >= seen.size() || seen[id]) {
          return false;
        }
        seen[id] = true;
        ++counted;
      }
      return true;
    }

    if (!node.ids.empty() || node.children.size() != entries) {
      return false;
    }

    for (std::size_t index = 0U; index < entries; ++index) {
      if (node.children[index] == nullptr) {
        return false;
      }
      if (bounds(*node.children[index]) != node.boxes[index]) {
        return false;
      }
      if (!validate_node(
              *node.children[index], false, depth + 1U,
              leaf_depth, seen, counted)) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace algorithms::data_structures

inline RTree2D::RTree2D()
    : root_(std::make_unique<Node>(true)) {}

