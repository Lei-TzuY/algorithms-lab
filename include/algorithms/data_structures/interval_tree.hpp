#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

using IntervalHandle = std::size_t;

struct IntervalRecord {
  IntervalHandle handle{};
  std::int64_t low{};
  std::int64_t high{};

  friend bool operator==(const IntervalRecord&, const IntervalRecord&) = default;
};

class IntervalTree {
 public:
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  IntervalHandle insert(std::int64_t low, std::int64_t high) {
    if (low > high) {
      throw std::invalid_argument("interval low endpoint exceeds high endpoint");
    }
    if (records_.size() == records_.max_size()) {
      throw std::length_error("interval handle space exhausted");
    }

    const IntervalHandle handle = records_.size();
    const IntervalRecord record{handle, low, high};
    std::unique_ptr<Node> pending = std::make_unique<Node>(record);
    records_.push_back(record);
    root_ = insert_node(std::move(root_), std::move(pending));
    ++size_;
    return handle;
  }

  bool erase(IntervalHandle handle) {
    if (handle >= records_.size() || !records_[handle].has_value()) {
      return false;
    }
    const IntervalRecord record = *records_[handle];
    bool erased = false;
    root_ = erase_node(std::move(root_), record.low, record.handle, erased);
    if (!erased) {
      throw std::logic_error("interval registry/tree invariant violated");
    }
    records_[handle].reset();
    --size_;
    return true;
  }

  [[nodiscard]] std::optional<IntervalRecord> get(IntervalHandle handle) const {
    if (handle >= records_.size()) {
      return std::nullopt;
    }
    return records_[handle];
  }

  [[nodiscard]] std::vector<IntervalRecord> intervals() const {
    std::vector<IntervalRecord> result;
    result.reserve(size_);
    inorder(root_.get(), result);
    return result;
  }

  [[nodiscard]] std::vector<IntervalRecord> stab(std::int64_t point) const {
    std::vector<IntervalRecord> result;
    stab_node(root_.get(), point, result);
    return result;
  }

  [[nodiscard]] std::vector<IntervalRecord> overlaps(std::int64_t low,
                                                      std::int64_t high) const {
    if (low > high) {
      throw std::invalid_argument("query interval low endpoint exceeds high endpoint");
    }
    std::vector<IntervalRecord> result;
    overlap_node(root_.get(), low, high, result);
    return result;
  }

  [[nodiscard]] bool valid_structure() const {
    std::vector<bool> seen(records_.size(), false);
    const Validation validation = validate_node(root_.get(), nullptr, nullptr, seen);
    if (!validation.valid || validation.count != size_) {
      return false;
    }

    std::size_t active = 0;
    for (std::size_t handle = 0; handle < records_.size(); ++handle) {
      if (records_[handle].has_value()) {
        ++active;
        if (!seen[handle] || records_[handle]->handle != handle) {
          return false;
        }
      } else if (seen[handle]) {
        return false;
      }
    }
    return active == size_;
  }

 private:
  struct Node {
    explicit Node(const IntervalRecord& record)
        : handle(record.handle), low(record.low), high(record.high),
          max_high(record.high) {}

    IntervalHandle handle;
    std::int64_t low;
    std::int64_t high;
    std::int64_t max_high;
    int height{1};
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
  };

  struct Validation {
    bool valid{true};
    std::size_t count{0};
    int height{0};
    std::optional<std::int64_t> max_high;
  };

  struct Key {
    std::int64_t low;
    IntervalHandle handle;
  };

  static bool key_less(const Key& first, const Key& second) noexcept {
    if (first.low != second.low) {
      return first.low < second.low;
    }
    return first.handle < second.handle;
  }

  static int node_height(const std::unique_ptr<Node>& node) noexcept {
    return node ? node->height : 0;
  }

  static void update(Node& node) noexcept {
    node.height = 1 + std::max(node_height(node.left), node_height(node.right));
    node.max_high = node.high;
    if (node.left) {
      node.max_high = std::max(node.max_high, node.left->max_high);
    }
    if (node.right) {
      node.max_high = std::max(node.max_high, node.right->max_high);
    }
  }

  static int balance_factor(const std::unique_ptr<Node>& node) noexcept {
    if (!node) {
      return 0;
    }
    return node_height(node->left) - node_height(node->right);
  }

  static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> node) noexcept {
    std::unique_ptr<Node> new_root = std::move(node->right);
    node->right = std::move(new_root->left);
    update(*node);
    new_root->left = std::move(node);
    update(*new_root);
    return new_root;
  }

  static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> node) noexcept {
    std::unique_ptr<Node> new_root = std::move(node->left);
    node->left = std::move(new_root->right);
    update(*node);
    new_root->right = std::move(node);
    update(*new_root);
    return new_root;
  }

  static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> node) noexcept {
    if (!node) {
      return nullptr;
    }
    update(*node);
    const int balance = balance_factor(node);
    if (balance > 1) {
      if (balance_factor(node->left) < 0) {
        node->left = rotate_left(std::move(node->left));
      }
      return rotate_right(std::move(node));
    }
    if (balance < -1) {
      if (balance_factor(node->right) > 0) {
        node->right = rotate_right(std::move(node->right));
      }
      return rotate_left(std::move(node));
    }
    return node;
  }

  static std::unique_ptr<Node> insert_node(std::unique_ptr<Node> node,
                                           std::unique_ptr<Node> pending) noexcept {
    if (!node) {
      return pending;
    }
    const Key key{pending->low, pending->handle};
    const Key node_key{node->low, node->handle};
    if (key_less(key, node_key)) {
      node->left = insert_node(std::move(node->left), std::move(pending));
    } else {
      node->right = insert_node(std::move(node->right), std::move(pending));
    }
    return rebalance(std::move(node));
  }

  static const Node* minimum_node(const Node* node) noexcept {
    while (node != nullptr && node->left) {
      node = node->left.get();
    }
    return node;
  }

  static std::unique_ptr<Node> erase_node(std::unique_ptr<Node> node,
                                          std::int64_t low,
                                          IntervalHandle handle,
                                          bool& erased) noexcept {
    if (!node) {
      return nullptr;
    }
    const Key key{low, handle};
    const Key node_key{node->low, node->handle};
    if (key_less(key, node_key)) {
      node->left = erase_node(std::move(node->left), low, handle, erased);
    } else if (key_less(node_key, key)) {
      node->right = erase_node(std::move(node->right), low, handle, erased);
    } else {
      erased = true;
      if (!node->left) {
        return std::move(node->right);
      }
      if (!node->right) {
        return std::move(node->left);
      }

      const Node* successor = minimum_node(node->right.get());
      node->handle = successor->handle;
      node->low = successor->low;
      node->high = successor->high;
      bool successor_erased = false;
      node->right = erase_node(std::move(node->right), successor->low,
                               successor->handle, successor_erased);
    }
    return rebalance(std::move(node));
  }

  static void inorder(const Node* node, std::vector<IntervalRecord>& result) {
    if (!node) {
      return;
    }
    inorder(node->left.get(), result);
    result.push_back(IntervalRecord{node->handle, node->low, node->high});
    inorder(node->right.get(), result);
  }

  static void stab_node(const Node* node, std::int64_t point,
                        std::vector<IntervalRecord>& result) {
    if (!node) {
      return;
    }
    if (node->left && node->left->max_high >= point) {
      stab_node(node->left.get(), point, result);
    }
    if (node->low <= point && point <= node->high) {
      result.push_back(IntervalRecord{node->handle, node->low, node->high});
    }
    if (node->low <= point) {
      stab_node(node->right.get(), point, result);
    }
  }

  static void overlap_node(const Node* node, std::int64_t low, std::int64_t high,
                           std::vector<IntervalRecord>& result) {
    if (!node) {
      return;
    }
    if (node->left && node->left->max_high >= low) {
      overlap_node(node->left.get(), low, high, result);
    }
    if (node->low <= high && node->high >= low) {
      result.push_back(IntervalRecord{node->handle, node->low, node->high});
    }
    if (node->low <= high) {
      overlap_node(node->right.get(), low, high, result);
    }
  }

  static Validation validate_node(const Node* node, const Key* lower,
                                  const Key* upper, std::vector<bool>& seen) {
    if (!node) {
      return {};
    }
    const Key key{node->low, node->handle};
    if ((lower != nullptr && !key_less(*lower, key)) ||
        (upper != nullptr && !key_less(key, *upper)) ||
        node->handle >= seen.size() || seen[node->handle]) {
      return Validation{false, 0, 0, std::nullopt};
    }
    seen[node->handle] = true;

    const Validation left = validate_node(node->left.get(), lower, &key, seen);
    const Validation right = validate_node(node->right.get(), &key, upper, seen);
    if (!left.valid || !right.valid) {
      return Validation{false, 0, 0, std::nullopt};
    }

    const int expected_height = 1 + std::max(left.height, right.height);
    const int balance = left.height - right.height;
    std::int64_t expected_max = node->high;
    if (left.max_high.has_value()) {
      expected_max = std::max(expected_max, *left.max_high);
    }
    if (right.max_high.has_value()) {
      expected_max = std::max(expected_max, *right.max_high);
    }
    const bool valid = node->height == expected_height && balance >= -1 &&
                       balance <= 1 && node->max_high == expected_max;
    return Validation{valid, left.count + right.count + 1U, expected_height,
                      expected_max};
  }

  std::unique_ptr<Node> root_;
  std::vector<std::optional<IntervalRecord>> records_;
  std::size_t size_{0};
};

}  // namespace algorithms::data_structures
