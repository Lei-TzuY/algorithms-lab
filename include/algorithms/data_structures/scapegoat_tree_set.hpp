#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class ScapegoatTreeSet {
 public:
  using value_type = std::int64_t;

  ScapegoatTreeSet() = default;
  ScapegoatTreeSet(const ScapegoatTreeSet&) = delete;
  ScapegoatTreeSet& operator=(const ScapegoatTreeSet&) = delete;
  ScapegoatTreeSet(ScapegoatTreeSet&&) = delete;
  ScapegoatTreeSet& operator=(ScapegoatTreeSet&&) = delete;

  [[nodiscard]] bool empty() const noexcept { return size_ == 0U; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t maximum_size_since_rebuild() const noexcept {
    return max_size_since_rebuild_;
  }
  [[nodiscard]] std::size_t insertion_rebuild_count() const noexcept {
    return insertion_rebuild_count_;
  }
  [[nodiscard]] std::size_t root_rebuild_count() const noexcept {
    return root_rebuild_count_;
  }

  [[nodiscard]] bool contains(value_type key) const noexcept {
    const Node* current = root_.get();
    while (current != nullptr) {
      if (key < current->key) {
        current = current->left.get();
      } else if (current->key < key) {
        current = current->right.get();
      } else {
        return true;
      }
    }
    return false;
  }

  bool insert(value_type key) {
    if (root_ == nullptr) {
      root_ = std::make_unique<Node>(key);
      size_ = 1U;
      max_size_since_rebuild_ = 1U;
      return true;
    }

    std::vector<std::unique_ptr<Node>*> path;
    std::unique_ptr<Node>* link = &root_;
    while (*link != nullptr) {
      path.push_back(link);
      Node* const node = link->get();
      if (key < node->key) {
        link = &node->left;
      } else if (node->key < key) {
        link = &node->right;
      } else {
        return false;
      }
    }

    *link = std::make_unique<Node>(key);
    path.push_back(link);
    ++size_;
    if (size_ > max_size_since_rebuild_) {
      max_size_since_rebuild_ = size_;
    }

    for (std::size_t index = path.size(); index > 1U; --index) {
      update_size(path[index - 2U]->get());
    }

    const std::size_t depth = path.size() - 1U;
    if (depth > allowed_depth(max_size_since_rebuild_)) {
      std::optional<std::size_t> scapegoat_index;
      for (std::size_t child_index = path.size() - 1U; child_index > 0U;
           --child_index) {
        const Node* const child = path[child_index]->get();
        const Node* const parent = path[child_index - 1U]->get();
        if (child != nullptr && parent != nullptr &&
            child->subtree_size > floor_two_thirds(parent->subtree_size)) {
          scapegoat_index = child_index - 1U;
          break;
        }
      }
      if (scapegoat_index.has_value()) {
        rebuild(*path[*scapegoat_index]);
        ++insertion_rebuild_count_;
      }
    }
    return true;
  }

  bool erase(value_type key) {
    if (!erase_impl(root_, key)) {
      return false;
    }
    --size_;
    if (size_ == 0U) {
      root_.reset();
      max_size_since_rebuild_ = 0U;
      return true;
    }

    if (below_two_thirds(size_, max_size_since_rebuild_)) {
      rebuild(root_);
      max_size_since_rebuild_ = size_;
      ++root_rebuild_count_;
    }
    return true;
  }

  [[nodiscard]] std::vector<value_type> values_in_order() const {
    std::vector<value_type> values;
    values.reserve(size_);
    collect_values(root_.get(), values);
    return values;
  }

  // Height in edges. Empty trees report zero so this remains a total diagnostic.
  [[nodiscard]] std::size_t height() const noexcept { return height_of(root_.get()); }

  [[nodiscard]] bool valid_structure() const noexcept {
    const Validation validation = validate(root_.get(), std::nullopt, std::nullopt);
    return validation.valid && validation.count == size_ &&
           (size_ == 0U ? max_size_since_rebuild_ == 0U
                        : max_size_since_rebuild_ >= size_);
  }

  // Classical alpha=2/3 insertion-depth threshold floor(log_{3/2}(q)).
  [[nodiscard]] static std::size_t allowed_depth(std::size_t maximum_size) noexcept {
    if (maximum_size <= 1U) {
      return 0U;
    }
    const long double q = static_cast<long double>(maximum_size);
    const long double raw = std::log(q) / std::log(1.5L);
    if (!(raw > 0.0L)) {
      return 0U;
    }
    return static_cast<std::size_t>(std::floor(raw));
  }

 private:
  struct Node {
    explicit Node(value_type value) : key(value) {}

    value_type key;
    std::size_t subtree_size = 1U;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
  };

  struct Validation {
    bool valid;
    std::size_t count;
  };

  static std::size_t node_size(const Node* node) noexcept {
    return node == nullptr ? 0U : node->subtree_size;
  }

  static void update_size(Node* node) noexcept {
    if (node != nullptr) {
      node->subtree_size =
          1U + node_size(node->left.get()) + node_size(node->right.get());
    }
  }

  static std::size_t floor_two_thirds(std::size_t value) noexcept {
    return 2U * (value / 3U) + (2U * (value % 3U)) / 3U;
  }

  static bool below_two_thirds(std::size_t value,
                               std::size_t reference) noexcept {
    const std::size_t threshold = floor_two_thirds(reference);
    if ((reference % 3U) == 0U) {
      return value < threshold;
    }
    return value <= threshold;
  }

  static void collect_values(const Node* node, std::vector<value_type>& values) {
    if (node == nullptr) {
      return;
    }
    collect_values(node->left.get(), values);
    values.push_back(node->key);
    collect_values(node->right.get(), values);
  }

  static std::unique_ptr<Node> build_balanced(
      const std::vector<value_type>& values, std::size_t begin,
      std::size_t end) {
    if (begin >= end) {
      return nullptr;
    }
    const std::size_t middle = begin + (end - begin) / 2U;
    auto node = std::make_unique<Node>(values[middle]);
    node->left = build_balanced(values, begin, middle);
    node->right = build_balanced(values, middle + 1U, end);
    update_size(node.get());
    return node;
  }

  static void rebuild(std::unique_ptr<Node>& subtree) {
    std::vector<value_type> values;
    values.reserve(node_size(subtree.get()));
    collect_values(subtree.get(), values);
    subtree = build_balanced(values, 0U, values.size());
  }

  static bool erase_impl(std::unique_ptr<Node>& node, value_type key) {
    if (node == nullptr) {
      return false;
    }
    if (key < node->key) {
      const bool erased = erase_impl(node->left, key);
      if (erased) {
        update_size(node.get());
      }
      return erased;
    }
    if (node->key < key) {
      const bool erased = erase_impl(node->right, key);
      if (erased) {
        update_size(node.get());
      }
      return erased;
    }

    if (node->left == nullptr) {
      node = std::move(node->right);
      return true;
    }
    if (node->right == nullptr) {
      node = std::move(node->left);
      return true;
    }

    Node* successor = node->right.get();
    while (successor->left != nullptr) {
      successor = successor->left.get();
    }
    const value_type successor_key = successor->key;
    node->key = successor_key;
    const bool erased = erase_impl(node->right, successor_key);
    if (erased) {
      update_size(node.get());
    }
    return erased;
  }

  static std::size_t height_of(const Node* node) noexcept {
    if (node == nullptr) {
      return 0U;
    }
    const std::size_t left_height =
        node->left == nullptr ? 0U : 1U + height_of(node->left.get());
    const std::size_t right_height =
        node->right == nullptr ? 0U : 1U + height_of(node->right.get());
    return std::max(left_height, right_height);
  }

  static Validation validate(const Node* node,
                             std::optional<value_type> lower,
                             std::optional<value_type> upper) noexcept {
    if (node == nullptr) {
      return Validation{true, 0U};
    }
    if ((lower.has_value() && !(lower.value() < node->key)) ||
        (upper.has_value() && !(node->key < upper.value()))) {
      return Validation{false, 0U};
    }
    const Validation left = validate(node->left.get(), lower, node->key);
    if (!left.valid) {
      return Validation{false, 0U};
    }
    const Validation right = validate(node->right.get(), node->key, upper);
    if (!right.valid) {
      return Validation{false, 0U};
    }
    const std::size_t count = 1U + left.count + right.count;
    return Validation{node->subtree_size == count, count};
  }

  std::unique_ptr<Node> root_;
  std::size_t size_ = 0U;
  std::size_t max_size_since_rebuild_ = 0U;
  std::size_t insertion_rebuild_count_ = 0U;
  std::size_t root_rebuild_count_ = 0U;
};

}  // namespace algorithms::data_structures
