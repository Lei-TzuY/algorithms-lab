#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// A first-principles Kaplan-Zwick-style soft heap with a dyadic error rate.
// Compare(a,b) means a has higher priority than b (std::less => min soft heap).
//
// error_rate_power p means epsilon = 2^-p. The rank threshold is r=p+5,
// matching the simplified soft-heap sizing rule. At every observable stable
// state, at most floor(total_insertions / 2^p) live items may be corrupt.
// An item is corrupt exactly when its node's current key is worse than its
// original key under Compare; keys are never artificially improved.
//
// This concrete C++ baseline deliberately scans the O(log N) root list to
// refresh its minimum-root cache after structural changes. It therefore does
// not claim the tightest canonical soft-heap operation bounds. It does preserve
// the actual ranked-tree/list-sifting corruption mechanism and error guarantee.
template <typename T, typename Compare = std::less<T>>
class SoftHeap {
  static_assert(std::is_copy_constructible_v<T>,
                "SoftHeap requires copyable keys for explicit current-key state");

 public:
  struct Extracted {
    T value;
    T current_key;
    bool corrupted;
  };

  explicit SoftHeap(std::size_t error_rate_power = 4, Compare compare = Compare{})
      : error_rate_power_(error_rate_power), compare_(std::move(compare)) {
    if (error_rate_power_ == 0U || error_rate_power_ >
        std::numeric_limits<std::size_t>::max() - kThresholdSlack) {
      throw std::invalid_argument("SoftHeap requires dyadic epsilon in (0, 1/2]");
    }
    rank_threshold_ = error_rate_power_ + kThresholdSlack;
  }

  SoftHeap(const SoftHeap&) = delete;
  SoftHeap& operator=(const SoftHeap&) = delete;
  SoftHeap(SoftHeap&& other) noexcept(
      std::is_nothrow_move_constructible_v<Compare>)
      : roots_(std::move(other.roots_)),
        live_size_(std::exchange(other.live_size_, 0U)),
        insertion_count_(std::exchange(other.insertion_count_, 0U)),
        error_rate_power_(other.error_rate_power_),
        rank_threshold_(other.rank_threshold_),
        compare_(std::move(other.compare_)),
        min_root_(std::exchange(other.min_root_, nullptr)) {}

  SoftHeap& operator=(SoftHeap&& other) noexcept(
      std::is_nothrow_move_assignable_v<Compare>) {
    if (this == &other) {
      return *this;
    }
    roots_ = std::move(other.roots_);
    live_size_ = std::exchange(other.live_size_, 0U);
    insertion_count_ = std::exchange(other.insertion_count_, 0U);
    error_rate_power_ = other.error_rate_power_;
    rank_threshold_ = other.rank_threshold_;
    compare_ = std::move(other.compare_);
    min_root_ = std::exchange(other.min_root_, nullptr);
    return *this;
  }

  [[nodiscard]] bool empty() const noexcept { return live_size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return live_size_; }
  [[nodiscard]] std::size_t insertion_count() const noexcept {
    return insertion_count_;
  }
  [[nodiscard]] std::size_t error_rate_power() const noexcept {
    return error_rate_power_;
  }
  [[nodiscard]] std::size_t rank_threshold() const noexcept {
    return rank_threshold_;
  }
  [[nodiscard]] std::size_t root_count() const noexcept { return roots_.size(); }

  [[nodiscard]] std::size_t corruption_budget() const noexcept {
    constexpr std::size_t kBits = std::numeric_limits<std::size_t>::digits;
    if (error_rate_power_ >= kBits) {
      return 0;
    }
    return insertion_count_ >> error_rate_power_;
  }

  [[nodiscard]] const T& top_current_key() const {
    const Node& root = minimum_root();
    return *root.current_key;
  }

  [[nodiscard]] const T& top_original_key() const {
    const Node& root = minimum_root();
    return root.items.front();
  }

  void push(const T& value) {
    preflight_insert_counters();
    RootTree singleton;
    singleton.root = make_leaf(value);
    singleton.rank = 0;
    roots_.push_front(std::move(singleton));
    ++live_size_;
    ++insertion_count_;
    normalize_roots();
  }

  void push(T&& value) {
    preflight_insert_counters();
    T current_key(value);
    RootTree singleton;
    singleton.root = make_leaf_with_current(std::move(value), std::move(current_key));
    singleton.rank = 0;
    roots_.push_front(std::move(singleton));
    ++live_size_;
    ++insertion_count_;
    normalize_roots();
  }

  // Destructive meld. Both heaps must use the same dyadic error rate. Stateful
  // comparator equivalence is a caller precondition and is not dynamically checked.
  void meld(SoftHeap&& other) {
    if (this == &other || other.empty()) {
      return;
    }
    if (error_rate_power_ != other.error_rate_power_) {
      throw std::invalid_argument("SoftHeap meld requires equal error rates");
    }
    if (insertion_count_ >
        std::numeric_limits<std::size_t>::max() - other.insertion_count_ ||
        live_size_ > std::numeric_limits<std::size_t>::max() - other.live_size_) {
      throw std::overflow_error("SoftHeap meld size overflow");
    }

    roots_.merge(other.roots_, [](const RootTree& first, const RootTree& second) {
      return first.rank < second.rank;
    });
    insertion_count_ += other.insertion_count_;
    live_size_ += other.live_size_;
    other.insertion_count_ = 0;
    other.live_size_ = 0;
    other.min_root_ = nullptr;
    normalize_roots();
  }

  Extracted pop() {
    if (empty()) {
      throw std::out_of_range("SoftHeap::pop on empty heap");
    }

    auto tree_it = minimum_root_iterator();
    Node& root = *tree_it->root;
    if (root.items.empty() || !root.current_key) {
      throw std::logic_error("SoftHeap internal empty minimum root");
    }

    T current_key(*root.current_key);
    const bool corrupted = compare_(root.items.front(), current_key);
    T value = std::move(root.items.front());
    root.items.pop_front();
    --live_size_;

    if (root.items.size() <= root.target_size / 2U) {
      if (!leaf(root)) {
        sift(root);
      } else if (root.items.empty()) {
        roots_.erase(tree_it);
      }
    }

    refresh_min_root();
    return Extracted{std::move(value), std::move(current_key), corrupted};
  }

  [[nodiscard]] std::size_t corrupted_count() const {
    std::size_t count = 0;
    for (const auto& tree : roots_) {
      count += corrupted_count(tree.root.get());
    }
    return count;
  }

  [[nodiscard]] bool valid_structure() const {
    if (live_size_ > insertion_count_) {
      return false;
    }
    if (roots_.empty()) {
      return live_size_ == 0 && min_root_ == nullptr;
    }
    if (live_size_ == 0 || min_root_ == nullptr) {
      return false;
    }

    std::size_t item_count = 0;
    std::size_t corrupt_count = 0;
    bool first_root = true;
    std::size_t previous_rank = 0;
    const Node* expected_min = nullptr;

    for (const auto& tree : roots_) {
      if (!tree.root || tree.rank != tree.root->rank) {
        return false;
      }
      if (!first_root && tree.rank <= previous_rank) {
        return false;
      }
      first_root = false;
      previous_rank = tree.rank;
      if (!valid_node(tree.root.get(), item_count, corrupt_count)) {
        return false;
      }
      if (expected_min == nullptr ||
          compare_(*tree.root->current_key, *expected_min->current_key)) {
        expected_min = tree.root.get();
      }
    }

    return item_count == live_size_ && corrupt_count <= corruption_budget() &&
           expected_min == min_root_;
  }

 private:
  static constexpr std::size_t kThresholdSlack = 5U;

  struct Node {
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    std::list<T> items;
    std::optional<T> current_key;
    std::size_t rank{0};
    std::size_t target_size{1};
  };

  struct RootTree {
    std::unique_ptr<Node> root;
    std::size_t rank{0};
  };

  [[nodiscard]] bool leaf(const Node& node) const noexcept {
    return !node.left && !node.right;
  }

  void preflight_insert_counters() const {
    if (live_size_ == std::numeric_limits<std::size_t>::max() ||
        insertion_count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("SoftHeap insertion counter overflow");
    }
  }

  std::unique_ptr<Node> make_leaf(const T& value) {
    auto node = std::make_unique<Node>();
    node->items.push_back(value);
    node->current_key = value;
    return node;
  }

  std::unique_ptr<Node> make_leaf_with_current(T&& value, T&& current_key) {
    auto node = std::make_unique<Node>();
    node->items.push_back(std::move(value));
    node->current_key = std::move(current_key);
    return node;
  }

  std::size_t next_target_size(std::size_t new_rank,
                               std::size_t child_target) const {
    if (new_rank <= rank_threshold_) {
      return 1U;
    }
    if (child_target > (std::numeric_limits<std::size_t>::max() - 1U) / 3U) {
      throw std::overflow_error("SoftHeap node target-size overflow");
    }
    return (3U * child_target + 1U) / 2U;
  }

  std::size_t target_size_for_rank(std::size_t rank) const {
    std::size_t target = 1U;
    if (rank <= rank_threshold_) {
      return target;
    }
    for (std::size_t current = rank_threshold_; current < rank; ++current) {
      target = next_target_size(current + 1U, target);
    }
    return target;
  }

  std::unique_ptr<Node> combine(std::unique_ptr<Node> first,
                                std::unique_ptr<Node> second) {
    if (!first || !second || first->rank != second->rank) {
      throw std::logic_error("SoftHeap combine rank mismatch");
    }
    if (first->rank == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("SoftHeap rank overflow");
    }
    const std::size_t new_rank = first->rank + 1U;
    const std::size_t target = next_target_size(new_rank, first->target_size);

    auto parent = std::make_unique<Node>();
    parent->rank = new_rank;
    parent->target_size = target;
    parent->left = std::move(first);
    parent->right = std::move(second);
    sift(*parent);
    return parent;
  }

  void move_items(Node& source, Node& destination) {
    if (source.items.empty() || !source.current_key) {
      throw std::logic_error("SoftHeap sift source is empty");
    }
    destination.items.splice(destination.items.end(), source.items);
  }

  void sift(Node& node) {
    while (node.items.size() < node.target_size && !leaf(node)) {
      if (!node.left ||
          (node.right && compare_(*node.right->current_key,
                                  *node.left->current_key))) {
        std::swap(node.left, node.right);
      }
      if (!node.left || !node.left->current_key || node.left->items.empty()) {
        throw std::logic_error("SoftHeap malformed sift child");
      }

      move_items(*node.left, node);
      node.current_key = *node.left->current_key;

      if (leaf(*node.left)) {
        node.left.reset();
      } else {
        sift(*node.left);
      }
    }
  }

  void normalize_roots() {
    auto current = roots_.begin();
    while (current != roots_.end()) {
      auto next = std::next(current);
      if (next == roots_.end()) {
        break;
      }
      if (current->rank != next->rank) {
        ++current;
        continue;
      }

      auto third = std::next(next);
      const bool three =
          third != roots_.end() && third->rank == current->rank;
      if (three) {
        current = next;
        continue;
      }

      current->root = combine(std::move(current->root), std::move(next->root));
      current->rank = current->root->rank;
      roots_.erase(next);
    }
    refresh_min_root();
  }

  typename std::list<RootTree>::iterator minimum_root_iterator() {
    if (min_root_ == nullptr) {
      throw std::out_of_range("SoftHeap minimum on empty heap");
    }
    for (auto it = roots_.begin(); it != roots_.end(); ++it) {
      if (it->root.get() == min_root_) {
        return it;
      }
    }
    throw std::logic_error("SoftHeap minimum-root cache is stale");
  }

  const Node& minimum_root() const {
    if (min_root_ == nullptr) {
      throw std::out_of_range("SoftHeap minimum on empty heap");
    }
    return *min_root_;
  }

  void refresh_min_root() {
    min_root_ = nullptr;
    for (auto& tree : roots_) {
      if (!tree.root || !tree.root->current_key || tree.root->items.empty()) {
        continue;
      }
      if (min_root_ == nullptr ||
          compare_(*tree.root->current_key, *min_root_->current_key)) {
        min_root_ = tree.root.get();
      }
    }
  }

  std::size_t corrupted_count(const Node* node) const {
    if (node == nullptr) {
      return 0U;
    }
    if (!node->current_key) {
      throw std::logic_error("SoftHeap node lacks current key");
    }
    std::size_t count = 0;
    for (const T& item : node->items) {
      if (compare_(item, *node->current_key)) {
        ++count;
      }
    }
    return count + corrupted_count(node->left.get()) +
           corrupted_count(node->right.get());
  }

  bool valid_node(const Node* node, std::size_t& item_count,
                  std::size_t& corrupt_count) const {
    if (node == nullptr || !node->current_key || node->items.empty()) {
      return false;
    }
    if (node->target_size == 0U) {
      return false;
    }
    if (node->target_size != target_size_for_rank(node->rank)) {
      return false;
    }

    for (const T& item : node->items) {
      if (compare_(*node->current_key, item)) {
        return false;
      }
      const bool corrupt = compare_(item, *node->current_key);
      if (node->rank <= rank_threshold_ && corrupt) {
        return false;
      }
      item_count += 1U;
      if (corrupt) {
        corrupt_count += 1U;
      }
    }

    for (const Node* child : {node->left.get(), node->right.get()}) {
      if (child == nullptr) {
        continue;
      }
      if (child->rank + 1U != node->rank || !child->current_key) {
        return false;
      }
      if (compare_(*child->current_key, *node->current_key)) {
        return false;
      }
      if (!valid_node(child, item_count, corrupt_count)) {
        return false;
      }
    }
    return true;
  }

  std::list<RootTree> roots_;
  std::size_t live_size_{0};
  std::size_t insertion_count_{0};
  std::size_t error_rate_power_{4};
  std::size_t rank_threshold_{9};
  [[no_unique_address]] Compare compare_{};
  Node* min_root_{nullptr};
};

}  // namespace algorithms::data_structures
