#pragma once

#include "algorithms/dynamic_programming/edit_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct BkTreeMatch {
  std::string value;
  std::size_t distance{0U};

  friend bool operator==(const BkTreeMatch&, const BkTreeMatch&) = default;
};

struct BkTreeQueryResult {
  std::vector<BkTreeMatch> matches;
  std::size_t visited_nodes{0U};
  std::size_t pruned_children{0U};

  friend bool operator==(const BkTreeQueryResult&, const BkTreeQueryResult&) =
      default;
};

// A first-principles BK-tree set over byte strings under sealed Levenshtein
// distance. Equal strings are stored once. Insertions are deterministic for a
// fixed insertion order; queries return matches sorted by (distance, unsigned-
// byte lexicographic value).
//
// BK invariant: for every parent node p and child edge labelled d, every value
// routed into that child subtree has metric distance exactly d from p. A radius-r
// query at p with q-distance x therefore needs only child labels in
// [x-r, x+r] by the triangle inequality.
class BkTreeStringSet {
 public:
  [[nodiscard]] bool empty() const noexcept { return nodes_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }

  bool insert(std::string value) {
    if (nodes_.empty()) {
      nodes_.push_back(Node{std::move(value), {}});
      return true;
    }

    std::size_t current = 0U;
    while (true) {
      const std::size_t distance = metric(nodes_[current].value, value);
      if (distance == 0U) {
        return false;
      }
      const auto child = nodes_[current].children.find(distance);
      if (child != nodes_[current].children.end()) {
        current = child->second;
        continue;
      }

      const std::size_t new_index = nodes_.size();
      nodes_.push_back(Node{std::move(value), {}});
      try {
        nodes_[current].children.emplace(distance, new_index);
      } catch (...) {
        nodes_.pop_back();
        throw;
      }
      return true;
    }
  }

  [[nodiscard]] BkTreeQueryResult query_within(std::string_view query,
                                                std::size_t radius) const {
    BkTreeQueryResult result;
    if (nodes_.empty()) {
      return result;
    }

    std::vector<std::size_t> stack{0U};
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      ++result.visited_nodes;

      const Node& node = nodes_[current];
      const std::size_t distance = metric(node.value, query);
      if (distance <= radius) {
        result.matches.push_back(BkTreeMatch{node.value, distance});
      }

      const std::size_t lower = distance > radius ? distance - radius : 0U;
      const std::size_t upper =
          radius > std::numeric_limits<std::size_t>::max() - distance
              ? std::numeric_limits<std::size_t>::max()
              : distance + radius;

      // Push in reverse edge-label order so the LIFO traversal visits eligible
      // children in ascending edge-label order. Result ordering does not rely on
      // traversal order, but diagnostics remain deterministic.
      for (auto iterator = node.children.rbegin();
           iterator != node.children.rend(); ++iterator) {
        if (iterator->first >= lower && iterator->first <= upper) {
          stack.push_back(iterator->second);
        } else {
          ++result.pruned_children;
        }
      }
    }

    std::sort(result.matches.begin(), result.matches.end(), match_less);
    return result;
  }

  // Expensive audit helper. Replays reachability, every explicit edge label,
  // and the complete insertion-routing path for every resident value. This is
  // intentionally verification machinery rather than a query-time operation.
  [[nodiscard]] bool valid_structure() const {
    if (nodes_.empty()) {
      return true;
    }

    std::vector<unsigned char> seen(nodes_.size(), 0U);
    std::vector<std::size_t> stack{0U};
    std::size_t reached = 0U;
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      if (current >= nodes_.size() || seen[current] != 0U) {
        return false;
      }
      seen[current] = 1U;
      ++reached;
      for (const auto& [label, child] : nodes_[current].children) {
        if (label == 0U || child >= nodes_.size() ||
            metric(nodes_[current].value, nodes_[child].value) != label) {
          return false;
        }
        stack.push_back(child);
      }
    }
    if (reached != nodes_.size()) {
      return false;
    }

    for (std::size_t target = 0U; target < nodes_.size(); ++target) {
      std::size_t current = 0U;
      std::size_t steps = 0U;
      while (current != target) {
        if (++steps > nodes_.size()) {
          return false;
        }
        const std::size_t distance =
            metric(nodes_[current].value, nodes_[target].value);
        if (distance == 0U) {
          return false;
        }
        const auto child = nodes_[current].children.find(distance);
        if (child == nodes_[current].children.end()) {
          return false;
        }
        current = child->second;
      }
    }
    return true;
  }

 private:
  struct Node {
    std::string value;
    std::map<std::size_t, std::size_t> children;
  };

  [[nodiscard]] static std::size_t metric(std::string_view first,
                                          std::string_view second) {
    return algorithms::dynamic_programming::levenshtein_edit_distance(first,
                                                                       second)
        .distance;
  }

  [[nodiscard]] static bool byte_less(std::string_view first,
                                      std::string_view second) {
    return std::lexicographical_compare(
        first.begin(), first.end(), second.begin(), second.end(),
        [](char left, char right) {
          return static_cast<unsigned char>(left) <
                 static_cast<unsigned char>(right);
        });
  }

  [[nodiscard]] static bool match_less(const BkTreeMatch& first,
                                       const BkTreeMatch& second) {
    if (first.distance != second.distance) {
      return first.distance < second.distance;
    }
    return byte_less(first.value, second.value);
  }

  std::vector<Node> nodes_;
};

}  // namespace algorithms::data_structures
