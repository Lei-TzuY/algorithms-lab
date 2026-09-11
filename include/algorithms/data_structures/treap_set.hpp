#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

class TreapSet {
 public:
  using Key = std::int64_t;

  explicit TreapSet(std::uint64_t seed = 0x243f6a8885a308d3ULL) noexcept
      : rng_state_(seed) {}

  TreapSet(const TreapSet&) = delete;
  TreapSet& operator=(const TreapSet&) = delete;
  TreapSet(TreapSet&&) noexcept = default;
  TreapSet& operator=(TreapSet&&) noexcept = default;
  ~TreapSet() = default;

  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::optional<Key> root_key() const noexcept {
    return root_ ? std::optional<Key>{root_->key} : std::nullopt;
  }

  [[nodiscard]] bool contains(Key key) const noexcept {
    const Node* current = root_.get();
    while (current != nullptr) {
      if (key == current->key) {
        return true;
      }
      current = key < current->key ? current->left.get() : current->right.get();
    }
    return false;
  }

  bool insert(Key key) {
    if (contains(key)) {
      return false;
    }

    auto node = std::make_unique<Node>();
    node->key = key;
    node->priority = next_priority();

    auto [left, right] = split(std::move(root_), key);
    root_ = merge(merge(std::move(left), std::move(node)), std::move(right));
    ++size_;
    return true;
  }

  bool erase(Key key) noexcept {
    bool erased = false;
    root_ = erase_node(std::move(root_), key, erased);
    if (erased) {
      --size_;
    }
    return erased;
  }

  [[nodiscard]] std::size_t rank(Key key) const noexcept {
    std::size_t result = 0;
    const Node* current = root_.get();
    while (current != nullptr) {
      if (key <= current->key) {
        current = current->left.get();
      } else {
        result += 1 + node_size(current->left);
        current = current->right.get();
      }
    }
    return result;
  }

  [[nodiscard]] std::optional<Key> kth(std::size_t index) const noexcept {
    if (index >= size_) {
      return std::nullopt;
    }
    const Node* current = root_.get();
    while (current != nullptr) {
      const std::size_t left_size = node_size(current->left);
      if (index < left_size) {
        current = current->left.get();
      } else if (index == left_size) {
        return current->key;
      } else {
        index -= left_size + 1;
        current = current->right.get();
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] std::vector<Key> inorder_keys() const {
    std::vector<Key> result;
    result.reserve(size_);
    inorder(root_.get(), result);
    return result;
  }

  [[nodiscard]] std::vector<Key> preorder_keys() const {
    std::vector<Key> result;
    result.reserve(size_);
    preorder(root_.get(), result);
    return result;
  }

  [[nodiscard]] bool valid_invariants() const noexcept {
    struct Frame {
      const Node* node;
      std::optional<Key> lower;
      std::optional<Key> upper;
      bool expanded;
    };

    if (!root_) {
      return size_ == 0;
    }

    std::vector<Frame> stack;
    try {
      stack.push_back(Frame{root_.get(), std::nullopt, std::nullopt, false});
      std::size_t visited = 0;
      while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const Node* node = frame.node;
        if (node == nullptr) {
          continue;
        }
        if (!frame.expanded) {
          if ((frame.lower && node->key <= *frame.lower) ||
              (frame.upper && node->key >= *frame.upper)) {
            return false;
          }
          if (node->left && !higher_priority(*node, *node->left)) {
            return false;
          }
          if (node->right && !higher_priority(*node, *node->right)) {
            return false;
          }
          stack.push_back(Frame{node, frame.lower, frame.upper, true});
          stack.push_back(Frame{node->right.get(), std::optional<Key>{node->key},
                                frame.upper, false});
          stack.push_back(Frame{node->left.get(), frame.lower,
                                std::optional<Key>{node->key}, false});
          continue;
        }
        const std::size_t expected =
            1 + node_size(node->left) + node_size(node->right);
        if (node->subtree_size != expected) {
          return false;
        }
        ++visited;
      }
      return visited == size_ && root_->subtree_size == size_;
    } catch (...) {
      return false;
    }
  }

 private:
  struct Node {
    Key key = 0;
    std::uint64_t priority = 0;
    std::size_t subtree_size = 1;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
  };

  using NodePtr = std::unique_ptr<Node>;

  NodePtr root_;
  std::size_t size_ = 0;
  std::uint64_t rng_state_;

  [[nodiscard]] static std::size_t node_size(const NodePtr& node) noexcept {
    return node ? node->subtree_size : 0;
  }

  static void refresh(Node& node) noexcept {
    node.subtree_size = 1 + node_size(node.left) + node_size(node.right);
  }

  [[nodiscard]] static bool higher_priority(const Node& parent,
                                            const Node& child) noexcept {
    if (parent.priority != child.priority) {
      return parent.priority > child.priority;
    }
    return parent.key > child.key;
  }

  static std::pair<NodePtr, NodePtr> split(NodePtr root, Key key) noexcept {
    if (!root) {
      return {nullptr, nullptr};
    }
    if (root->key < key) {
      auto [middle, right] = split(std::move(root->right), key);
      root->right = std::move(middle);
      refresh(*root);
      return {std::move(root), std::move(right)};
    }
    auto [left, middle] = split(std::move(root->left), key);
    root->left = std::move(middle);
    refresh(*root);
    return {std::move(left), std::move(root)};
  }

  static NodePtr merge(NodePtr left, NodePtr right) noexcept {
    if (!left) {
      return right;
    }
    if (!right) {
      return left;
    }
    if (higher_priority(*left, *right)) {
      left->right = merge(std::move(left->right), std::move(right));
      refresh(*left);
      return left;
    }
    right->left = merge(std::move(left), std::move(right->left));
    refresh(*right);
    return right;
  }

  static NodePtr erase_node(NodePtr root, Key key, bool& erased) noexcept {
    if (!root) {
      return nullptr;
    }
    if (key < root->key) {
      root->left = erase_node(std::move(root->left), key, erased);
      refresh(*root);
      return root;
    }
    if (key > root->key) {
      root->right = erase_node(std::move(root->right), key, erased);
      refresh(*root);
      return root;
    }
    erased = true;
    return merge(std::move(root->left), std::move(root->right));
  }

  [[nodiscard]] std::uint64_t next_priority() noexcept {
    rng_state_ += 0x9e3779b97f4a7c15ULL;
    std::uint64_t value = rng_state_;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
  }

  static void inorder(const Node* node, std::vector<Key>& out) {
    if (node == nullptr) {
      return;
    }
    inorder(node->left.get(), out);
    out.push_back(node->key);
    inorder(node->right.get(), out);
  }

  static void preorder(const Node* node, std::vector<Key>& out) {
    if (node == nullptr) {
      return;
    }
    out.push_back(node->key);
    preorder(node->left.get(), out);
    preorder(node->right.get(), out);
  }
};

}  // namespace algorithms::data_structures
