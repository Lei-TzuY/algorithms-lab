#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Immutable ordered set over int64_t values stored in a recursive
// van-Emde-Boas (vEB) tree layout.
//
// This is a memory-layout data structure, not the bounded-universe vEB
// predecessor set. No cache-miss or cache-optimality claim is made.
//
// Construction sorts and deduplicates the input, builds a balanced BST, then
// recursively lays out the top ceil(h/2) levels before each bottom subtree.
// Search follows explicit child indices in that layout.
class VebLayoutStaticSet64 {
 public:
  using Value = std::int64_t;

  explicit VebLayoutStaticSet64(std::span<const Value> values) {
    if (values.size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("vEB-layout static set is too large");
    }

    std::vector<Value> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    if (sorted.empty()) {
      return;
    }

    struct LogicalNode {
      Value key{};
      std::size_t left{kNoIndex};
      std::size_t right{kNoIndex};
      std::size_t subtree_size{1U};
      std::size_t height{1U};
    };

    std::vector<LogicalNode> logical;
    logical.reserve(sorted.size());

    const std::function<std::size_t(std::size_t, std::size_t)> build =
        [&](const std::size_t begin,
            const std::size_t end) -> std::size_t {
      if (begin >= end) {
        return kNoIndex;
      }

      const std::size_t middle = begin + (end - begin) / 2U;
      const std::size_t id = logical.size();
      logical.push_back(LogicalNode{sorted[middle]});

      const std::size_t left = build(begin, middle);
      const std::size_t right = build(middle + 1U, end);

      const std::size_t left_size =
          left == kNoIndex ? 0U : logical[left].subtree_size;
      const std::size_t right_size =
          right == kNoIndex ? 0U : logical[right].subtree_size;
      const std::size_t left_height =
          left == kNoIndex ? 0U : logical[left].height;
      const std::size_t right_height =
          right == kNoIndex ? 0U : logical[right].height;

      logical[id].left = left;
      logical[id].right = right;
      logical[id].subtree_size = 1U + left_size + right_size;
      logical[id].height =
          1U + std::max(left_height, right_height);
      return id;
    };

    const std::size_t logical_root = build(0U, sorted.size());

    std::vector<std::size_t> layout_order;
    layout_order.reserve(logical.size());

    const auto collect_frontier =
        [&](const std::size_t root,
            const std::size_t depth) {
      std::vector<std::size_t> frontier;
      if (root == kNoIndex) {
        return frontier;
      }

      frontier.push_back(root);
      for (std::size_t step = 0U; step < depth; ++step) {
        std::vector<std::size_t> next;
        next.reserve(frontier.size() * 2U);
        for (const std::size_t node : frontier) {
          if (logical[node].left != kNoIndex) {
            next.push_back(logical[node].left);
          }
          if (logical[node].right != kNoIndex) {
            next.push_back(logical[node].right);
          }
        }
        frontier = std::move(next);
        if (frontier.empty()) {
          break;
        }
      }
      return frontier;
    };

    std::function<void(std::size_t, std::size_t)> emit_cluster;
    emit_cluster = [&](const std::size_t root,
                       const std::size_t levels) {
      if (root == kNoIndex || levels == 0U) {
        return;
      }
      if (levels == 1U) {
        layout_order.push_back(root);
        return;
      }

      const std::size_t upper = (levels + 1U) / 2U;
      const std::size_t lower = levels - upper;

      emit_cluster(root, upper);
      for (const std::size_t bottom_root :
           collect_frontier(root, upper)) {
        emit_cluster(
            bottom_root,
            std::min(lower, logical[bottom_root].height));
      }
    };

    emit_cluster(logical_root, logical[logical_root].height);

    if (layout_order.size() != logical.size()) {
      throw std::logic_error(
          "vEB-layout construction did not emit every node");
    }

    std::vector<std::size_t> position(logical.size(), kNoIndex);
    for (std::size_t index = 0U; index < layout_order.size(); ++index) {
      const std::size_t logical_id = layout_order[index];
      if (logical_id >= logical.size() ||
          position[logical_id] != kNoIndex) {
        throw std::logic_error(
            "vEB-layout construction emitted a duplicate node");
      }
      position[logical_id] = index;
    }

    nodes_.reserve(logical.size());
    for (const std::size_t logical_id : layout_order) {
      const LogicalNode& source = logical[logical_id];
      nodes_.push_back(Node{
          source.key,
          source.left == kNoIndex
              ? kNoIndex
              : position[source.left],
          source.right == kNoIndex
              ? kNoIndex
              : position[source.right],
          source.subtree_size});
    }

    root_ = position[logical_root];
  }

  explicit VebLayoutStaticSet64(
      const std::vector<Value>& values)
      : VebLayoutStaticSet64(std::span<const Value>(values)) {}

  [[nodiscard]] std::size_t size() const noexcept {
    return nodes_.size();
  }

  [[nodiscard]] bool empty() const noexcept {
    return nodes_.empty();
  }

  [[nodiscard]] bool contains(const Value value) const noexcept {
    std::size_t node = root_;
    while (node != kNoIndex) {
      const Node& current = nodes_[node];
      if (value == current.key) {
        return true;
      }
      node = value < current.key
                 ? current.left
                 : current.right;
    }
    return false;
  }

  // Smallest stored value >= query.
  [[nodiscard]] std::optional<Value> lower_bound(
      const Value value) const noexcept {
    std::optional<Value> answer;
    std::size_t node = root_;
    while (node != kNoIndex) {
      const Node& current = nodes_[node];
      if (current.key >= value) {
        answer = current.key;
        node = current.left;
      } else {
        node = current.right;
      }
    }
    return answer;
  }

  // Greatest stored value <= query.
  [[nodiscard]] std::optional<Value> predecessor(
      const Value value) const noexcept {
    std::optional<Value> answer;
    std::size_t node = root_;
    while (node != kNoIndex) {
      const Node& current = nodes_[node];
      if (current.key <= value) {
        answer = current.key;
        node = current.right;
      } else {
        node = current.left;
      }
    }
    return answer;
  }

  // Number of stored values strictly smaller than query.
  [[nodiscard]] std::size_t rank(
      const Value value) const noexcept {
    std::size_t answer = 0U;
    std::size_t node = root_;
    while (node != kNoIndex) {
      const Node& current = nodes_[node];
      if (value <= current.key) {
        node = current.left;
      } else {
        answer += 1U + subtree_size(current.left);
        node = current.right;
      }
    }
    return answer;
  }

  // Zero-based sorted selection.
  [[nodiscard]] std::optional<Value> select(
      std::size_t order) const noexcept {
    if (order >= nodes_.size()) {
      return std::nullopt;
    }

    std::size_t node = root_;
    while (node != kNoIndex) {
      const Node& current = nodes_[node];
      const std::size_t left_size =
          subtree_size(current.left);

      if (order < left_size) {
        node = current.left;
      } else if (order == left_size) {
        return current.key;
      } else {
        order -= left_size + 1U;
        node = current.right;
      }
    }
    return std::nullopt;
  }

  // Diagnostic exposure of physical key order. This is useful for verifying
  // the recursive layout itself; search semantics do not depend on callers
  // inspecting it.
  [[nodiscard]] std::vector<Value> layout_values() const {
    std::vector<Value> values;
    values.reserve(nodes_.size());
    for (const Node& node : nodes_) {
      values.push_back(node.key);
    }
    return values;
  }

  // Expensive structural replay, excluded from operation-complexity claims.
  [[nodiscard]] bool valid_structure() const {
    if (nodes_.empty()) {
      return root_ == kNoIndex;
    }
    if (root_ >= nodes_.size()) {
      return false;
    }

    std::vector<unsigned char> color(nodes_.size(), 0U);
    std::vector<Value> inorder;
    inorder.reserve(nodes_.size());

    struct Frame {
      std::size_t node;
      bool expanded;
    };
    std::vector<Frame> stack;
    stack.push_back(Frame{root_, false});

    while (!stack.empty()) {
      const Frame frame = stack.back();
      stack.pop_back();

      if (frame.node == kNoIndex) {
        continue;
      }
      if (frame.node >= nodes_.size()) {
        return false;
      }

      const Node& current = nodes_[frame.node];
      if (!frame.expanded) {
        if (color[frame.node] != 0U) {
          return false;
        }
        if ((current.left != kNoIndex &&
             current.left >= nodes_.size()) ||
            (current.right != kNoIndex &&
             current.right >= nodes_.size())) {
          return false;
        }
        color[frame.node] = 1U;
        stack.push_back(Frame{current.right, false});
        stack.push_back(Frame{frame.node, true});
        stack.push_back(Frame{current.left, false});
      } else {
        if (color[frame.node] != 1U) {
          return false;
        }
        color[frame.node] = 2U;
        inorder.push_back(current.key);

        const std::size_t expected_size =
            1U + subtree_size(current.left) +
            subtree_size(current.right);
        if (current.subtree_size != expected_size) {
          return false;
        }
      }
    }

    if (inorder.size() != nodes_.size() ||
        !std::is_sorted(inorder.begin(), inorder.end())) {
      return false;
    }
    if (std::adjacent_find(inorder.begin(), inorder.end()) !=
        inorder.end()) {
      return false;
    }
    return std::all_of(
        color.begin(), color.end(),
        [](const unsigned char state) { return state == 2U; });
  }

 private:
  static constexpr std::size_t kNoIndex =
      std::numeric_limits<std::size_t>::max();

  struct Node {
    Value key{};
    std::size_t left{kNoIndex};
    std::size_t right{kNoIndex};
    std::size_t subtree_size{1U};
  };

  std::vector<Node> nodes_;
  std::size_t root_{kNoIndex};

  [[nodiscard]] std::size_t subtree_size(
      const std::size_t node) const noexcept {
    return node == kNoIndex ? 0U : nodes_[node].subtree_size;
  }
};

}  // namespace algorithms::data_structures
