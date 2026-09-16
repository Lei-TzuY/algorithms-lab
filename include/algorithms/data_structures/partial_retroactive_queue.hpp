#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace algorithms::data_structures {

class PartialRetroactiveQueue {
 public:
  using Timestamp = std::uint64_t;
  using Value = std::int64_t;

  static constexpr std::size_t kMaximumEventLimit = 1'000'000U;

  explicit PartialRetroactiveQueue(
      std::size_t max_events = kMaximumEventLimit)
      : max_events_(max_events) {
    if (max_events > kMaximumEventLimit) {
      throw std::invalid_argument("retroactive queue event limit too large");
    }
  }
  PartialRetroactiveQueue(const PartialRetroactiveQueue&) = delete;
  PartialRetroactiveQueue& operator=(const PartialRetroactiveQueue&) = delete;
  PartialRetroactiveQueue(PartialRetroactiveQueue&&) noexcept = default;
  PartialRetroactiveQueue& operator=(PartialRetroactiveQueue&&) noexcept = default;

  void insert_enqueue(Timestamp timestamp, Value value) {
    ensure_capacity_for_insert();
    ensure_timestamp_absent(timestamp);
    root_ = insert_node(std::move(root_),
                        std::make_unique<Node>(timestamp, EventKind::enqueue,
                                               value));
  }

  void insert_dequeue(Timestamp timestamp) {
    ensure_capacity_for_insert();
    ensure_timestamp_absent(timestamp);
    root_ = insert_node(std::move(root_),
                        std::make_unique<Node>(timestamp, EventKind::dequeue, 0));
    if (root_->min_prefix < 0) {
      root_ = erase_node(std::move(root_), timestamp);
      throw std::invalid_argument(
          "retroactive dequeue would make a history prefix invalid");
    }
  }

  void erase(Timestamp timestamp) {
    const Node* node = find_node(root_.get(), timestamp);
    if (node == nullptr) {
      throw std::out_of_range("retroactive queue timestamp not found");
    }

    if (node->kind == EventKind::enqueue) {
      const std::int64_t prefix_before = prefix_balance_less_than(timestamp);
      const SegmentSummary suffix = summarize_greater_than(root_.get(), timestamp);
      if (suffix.has_event && prefix_before + suffix.min_prefix < 0) {
        throw std::invalid_argument(
            "erasing enqueue would make a history prefix invalid");
      }
    }

    root_ = erase_node(std::move(root_), timestamp);
  }

  [[nodiscard]] bool contains_timestamp(Timestamp timestamp) const noexcept {
    return find_node(root_.get(), timestamp) != nullptr;
  }

  [[nodiscard]] std::size_t event_count() const noexcept {
    return node_count(root_.get());
  }

  [[nodiscard]] std::size_t size() const noexcept {
    const std::int64_t balance = subtree_sum(root_.get());
    return static_cast<std::size_t>(balance);
  }

  [[nodiscard]] bool empty() const noexcept { return size() == 0U; }

  [[nodiscard]] Value front() const {
    if (empty()) {
      throw std::out_of_range("front of empty retroactive queue");
    }
    const std::size_t enqueue_total = subtree_enqueue_count(root_.get());
    const std::size_t dequeue_total = enqueue_total - size();
    return select_enqueue(root_.get(), dequeue_total)->value;
  }

  [[nodiscard]] Value back() const {
    if (empty()) {
      throw std::out_of_range("back of empty retroactive queue");
    }
    const std::size_t enqueue_total = subtree_enqueue_count(root_.get());
    return select_enqueue(root_.get(), enqueue_total - 1U)->value;
  }

  [[nodiscard]] std::size_t tree_height() const noexcept {
    return static_cast<std::size_t>(node_height(root_.get()));
  }

  [[nodiscard]] bool valid_structure() const noexcept {
    const Validation validation = validate(root_.get(), std::nullopt, std::nullopt);
    return validation.valid && validation.min_prefix >= 0;
  }

 private:
  enum class EventKind : std::uint8_t { enqueue, dequeue };

  struct Node {
    Timestamp timestamp;
    EventKind kind;
    Value value;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    int height{1};
    std::size_t count{1U};
    std::size_t enqueue_count{};
    std::int64_t sum{};
    std::int64_t min_prefix{};

    Node(Timestamp timestamp_in, EventKind kind_in, Value value_in)
        : timestamp(timestamp_in), kind(kind_in), value(value_in) {
      const std::int64_t delta = kind == EventKind::enqueue ? 1 : -1;
      enqueue_count = kind == EventKind::enqueue ? 1U : 0U;
      sum = delta;
      min_prefix = delta;
    }
  };

  struct SegmentSummary {
    bool has_event{};
    std::int64_t sum{};
    std::int64_t min_prefix{};
  };

  struct Validation {
    bool valid{true};
    int height{};
    std::size_t count{};
    std::size_t enqueue_count{};
    std::int64_t sum{};
    std::int64_t min_prefix{};
    bool has_event{};
  };

  std::size_t max_events_;
  std::unique_ptr<Node> root_;

  static int node_height(const Node* node) noexcept {
    return node == nullptr ? 0 : node->height;
  }

  static std::size_t node_count(const Node* node) noexcept {
    return node == nullptr ? 0U : node->count;
  }

  static std::size_t subtree_enqueue_count(const Node* node) noexcept {
    return node == nullptr ? 0U : node->enqueue_count;
  }

  static std::int64_t subtree_sum(const Node* node) noexcept {
    return node == nullptr ? 0 : node->sum;
  }

  static SegmentSummary empty_summary() noexcept { return {}; }

  static SegmentSummary singleton_summary(EventKind kind) noexcept {
    const std::int64_t delta = kind == EventKind::enqueue ? 1 : -1;
    return SegmentSummary{true, delta, delta};
  }

  static SegmentSummary subtree_summary(const Node* node) noexcept {
    if (node == nullptr) {
      return empty_summary();
    }
    return SegmentSummary{true, node->sum, node->min_prefix};
  }

  static SegmentSummary combine(SegmentSummary first,
                                SegmentSummary second) noexcept {
    if (!first.has_event) {
      return second;
    }
    if (!second.has_event) {
      return first;
    }
    return SegmentSummary{true, first.sum + second.sum,
                          std::min(first.min_prefix,
                                   first.sum + second.min_prefix)};
  }

  static void refresh(Node* node) noexcept {
    const SegmentSummary all =
        combine(combine(subtree_summary(node->left.get()),
                        singleton_summary(node->kind)),
                subtree_summary(node->right.get()));
    node->height = 1 + std::max(node_height(node->left.get()),
                                node_height(node->right.get()));
    node->count = 1U + node_count(node->left.get()) + node_count(node->right.get());
    node->enqueue_count =
        (node->kind == EventKind::enqueue ? 1U : 0U) +
        subtree_enqueue_count(node->left.get()) +
        subtree_enqueue_count(node->right.get());
    node->sum = all.sum;
    node->min_prefix = all.min_prefix;
  }

  static int balance_factor(const Node* node) noexcept {
    return node_height(node->left.get()) - node_height(node->right.get());
  }

  static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> root) noexcept {
    std::unique_ptr<Node> pivot = std::move(root->left);
    root->left = std::move(pivot->right);
    refresh(root.get());
    pivot->right = std::move(root);
    refresh(pivot.get());
    return pivot;
  }

  static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> root) noexcept {
    std::unique_ptr<Node> pivot = std::move(root->right);
    root->right = std::move(pivot->left);
    refresh(root.get());
    pivot->left = std::move(root);
    refresh(pivot.get());
    return pivot;
  }

  static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> root) noexcept {
    refresh(root.get());
    const int factor = balance_factor(root.get());
    if (factor > 1) {
      if (balance_factor(root->left.get()) < 0) {
        root->left = rotate_left(std::move(root->left));
      }
      return rotate_right(std::move(root));
    }
    if (factor < -1) {
      if (balance_factor(root->right.get()) > 0) {
        root->right = rotate_right(std::move(root->right));
      }
      return rotate_left(std::move(root));
    }
    return root;
  }

  static std::unique_ptr<Node> insert_node(std::unique_ptr<Node> root,
                                          std::unique_ptr<Node> inserted) {
    if (root == nullptr) {
      return inserted;
    }
    if (inserted->timestamp < root->timestamp) {
      root->left = insert_node(std::move(root->left), std::move(inserted));
    } else {
      root->right = insert_node(std::move(root->right), std::move(inserted));
    }
    return rebalance(std::move(root));
  }

  static Node* minimum_node(Node* node) noexcept {
    while (node->left != nullptr) {
      node = node->left.get();
    }
    return node;
  }

  static std::unique_ptr<Node> erase_node(std::unique_ptr<Node> root,
                                         Timestamp timestamp) noexcept {
    if (timestamp < root->timestamp) {
      root->left = erase_node(std::move(root->left), timestamp);
    } else if (timestamp > root->timestamp) {
      root->right = erase_node(std::move(root->right), timestamp);
    } else {
      if (root->left == nullptr) {
        return std::move(root->right);
      }
      if (root->right == nullptr) {
        return std::move(root->left);
      }
      Node* successor = minimum_node(root->right.get());
      root->timestamp = successor->timestamp;
      root->kind = successor->kind;
      root->value = successor->value;
      root->right = erase_node(std::move(root->right), successor->timestamp);
    }
    return rebalance(std::move(root));
  }

  static const Node* find_node(const Node* node, Timestamp timestamp) noexcept {
    while (node != nullptr) {
      if (timestamp < node->timestamp) {
        node = node->left.get();
      } else if (timestamp > node->timestamp) {
        node = node->right.get();
      } else {
        return node;
      }
    }
    return nullptr;
  }

  void ensure_capacity_for_insert() const {
    if (event_count() >= max_events_) {
      throw std::length_error("retroactive queue event limit exceeded");
    }
  }

  void ensure_timestamp_absent(Timestamp timestamp) const {
    if (contains_timestamp(timestamp)) {
      throw std::invalid_argument("retroactive queue timestamp already exists");
    }
  }

  [[nodiscard]] std::int64_t prefix_balance_less_than(
      Timestamp timestamp) const noexcept {
    const Node* node = root_.get();
    std::int64_t result = 0;
    while (node != nullptr) {
      if (timestamp <= node->timestamp) {
        node = node->left.get();
      } else {
        result += subtree_sum(node->left.get());
        result += node->kind == EventKind::enqueue ? 1 : -1;
        node = node->right.get();
      }
    }
    return result;
  }

  static SegmentSummary summarize_greater_than(const Node* node,
                                                Timestamp timestamp) noexcept {
    if (node == nullptr) {
      return empty_summary();
    }
    if (node->timestamp <= timestamp) {
      return summarize_greater_than(node->right.get(), timestamp);
    }
    return combine(
        summarize_greater_than(node->left.get(), timestamp),
        combine(singleton_summary(node->kind), subtree_summary(node->right.get())));
  }

  static const Node* select_enqueue(const Node* node,
                                    std::size_t rank) noexcept {
    while (node != nullptr) {
      const std::size_t left_count = subtree_enqueue_count(node->left.get());
      if (rank < left_count) {
        node = node->left.get();
        continue;
      }
      rank -= left_count;
      if (node->kind == EventKind::enqueue) {
        if (rank == 0U) {
          return node;
        }
        --rank;
      }
      node = node->right.get();
    }
    return nullptr;
  }

  static Validation validate(const Node* node,
                             std::optional<Timestamp> lower,
                             std::optional<Timestamp> upper) noexcept {
    if (node == nullptr) {
      return {};
    }
    if ((lower.has_value() && node->timestamp <= *lower) ||
        (upper.has_value() && node->timestamp >= *upper)) {
      Validation invalid;
      invalid.valid = false;
      return invalid;
    }

    const Validation left = validate(node->left.get(), lower, node->timestamp);
    const Validation right = validate(node->right.get(), node->timestamp, upper);
    Validation result;
    result.valid = left.valid && right.valid;
    result.height = 1 + std::max(left.height, right.height);
    result.count = 1U + left.count + right.count;
    result.enqueue_count = left.enqueue_count + right.enqueue_count +
                           (node->kind == EventKind::enqueue ? 1U : 0U);
    const SegmentSummary expected =
        combine(combine(left.has_event
                            ? SegmentSummary{true, left.sum, left.min_prefix}
                            : empty_summary(),
                        singleton_summary(node->kind)),
                right.has_event
                    ? SegmentSummary{true, right.sum, right.min_prefix}
                    : empty_summary());
    result.sum = expected.sum;
    result.min_prefix = expected.min_prefix;
    result.has_event = true;

    if (node->height != result.height || node->count != result.count ||
        node->enqueue_count != result.enqueue_count || node->sum != result.sum ||
        node->min_prefix != result.min_prefix ||
        left.height - right.height < -1 || left.height - right.height > 1) {
      result.valid = false;
    }
    return result;
  }
};

}  // namespace algorithms::data_structures
