#include "algorithms/data_structures/splay_set.hpp"

namespace algorithms::data_structures {

SplaySet::~SplaySet() { clear(); }

std::optional<SplaySet::Key> SplaySet::root_key() const noexcept {
  if (root_ == nullptr) {
    return std::nullopt;
  }
  return root_->key;
}

void SplaySet::rotate_left(Node* node) noexcept {
  Node* pivot = node->right;
  Node* parent = node->parent;

  node->right = pivot->left;
  if (pivot->left != nullptr) {
    pivot->left->parent = node;
  }

  pivot->left = node;
  node->parent = pivot;
  pivot->parent = parent;

  if (parent == nullptr) {
    root_ = pivot;
  } else if (parent->left == node) {
    parent->left = pivot;
  } else {
    parent->right = pivot;
  }
}

void SplaySet::rotate_right(Node* node) noexcept {
  Node* pivot = node->left;
  Node* parent = node->parent;

  node->left = pivot->right;
  if (pivot->right != nullptr) {
    pivot->right->parent = node;
  }

  pivot->right = node;
  node->parent = pivot;
  pivot->parent = parent;

  if (parent == nullptr) {
    root_ = pivot;
  } else if (parent->left == node) {
    parent->left = pivot;
  } else {
    parent->right = pivot;
  }
}

void SplaySet::splay(Node* node) noexcept {
  while (node->parent != nullptr) {
    Node* parent = node->parent;
    Node* grand = parent->parent;
    if (grand == nullptr) {
      if (parent->left == node) {
        rotate_right(parent);
      } else {
        rotate_left(parent);
      }
      continue;
    }

    if (grand->left == parent && parent->left == node) {
      rotate_right(grand);
      rotate_right(parent);
    } else if (grand->right == parent && parent->right == node) {
      rotate_left(grand);
      rotate_left(parent);
    } else if (grand->left == parent && parent->right == node) {
      rotate_left(parent);
      rotate_right(grand);
    } else {
      rotate_right(parent);
      rotate_left(grand);
    }
  }
}

SplaySet::Node* SplaySet::access(Key key) noexcept {
  Node* current = root_;
  Node* last = nullptr;
  while (current != nullptr) {
    last = current;
    if (key == current->key) {
      splay(current);
      return current;
    }
    current = key < current->key ? current->left : current->right;
  }
  if (last != nullptr) {
    splay(last);
  }
  return nullptr;
}

bool SplaySet::contains(Key key) { return access(key) != nullptr; }

bool SplaySet::insert(Key key) {
  if (root_ == nullptr) {
    root_ = new Node{key};
    size_ = 1;
    return true;
  }

  Node* current = root_;
  Node* parent = nullptr;
  while (current != nullptr) {
    parent = current;
    if (key == current->key) {
      splay(current);
      return false;
    }
    current = key < current->key ? current->left : current->right;
  }

  Node* node = new Node{key, parent};
  if (key < parent->key) {
    parent->left = node;
  } else {
    parent->right = node;
  }
  ++size_;
  splay(node);
  return true;
}

bool SplaySet::erase(Key key) {
  Node* target = access(key);
  if (target == nullptr) {
    return false;
  }

  Node* left = target->left;
  Node* right = target->right;
  if (left != nullptr) {
    left->parent = nullptr;
  }
  if (right != nullptr) {
    right->parent = nullptr;
  }
  target->left = nullptr;
  target->right = nullptr;
  delete target;
  --size_;

  if (left == nullptr) {
    root_ = right;
    return true;
  }

  root_ = left;
  Node* maximum = left;
  while (maximum->right != nullptr) {
    maximum = maximum->right;
  }
  splay(maximum);
  root_->right = right;
  if (right != nullptr) {
    right->parent = root_;
  }
  return true;
}

std::vector<SplaySet::Key> SplaySet::inorder_keys() const {
  std::vector<Key> keys;
  keys.reserve(size_);
  std::vector<Node*> stack;
  Node* current = root_;
  while (current != nullptr || !stack.empty()) {
    while (current != nullptr) {
      stack.push_back(current);
      current = current->left;
    }
    current = stack.back();
    stack.pop_back();
    keys.push_back(current->key);
    current = current->right;
  }
  return keys;
}

bool SplaySet::valid_invariants() const {
  if (root_ == nullptr) {
    return size_ == 0;
  }
  if (root_->parent != nullptr) {
    return false;
  }

  struct Frame {
    Node* node;
    std::optional<Key> lower;
    std::optional<Key> upper;
  };
  std::vector<Frame> stack;
  stack.push_back(Frame{root_, std::nullopt, std::nullopt});
  std::size_t count = 0;
  while (!stack.empty()) {
    const Frame frame = stack.back();
    stack.pop_back();
    Node* node = frame.node;
    ++count;
    if (count > size_) {
      return false;
    }
    if ((frame.lower.has_value() && node->key <= *frame.lower) ||
        (frame.upper.has_value() && node->key >= *frame.upper)) {
      return false;
    }
    if (node->left != nullptr) {
      if (node->left->parent != node) {
        return false;
      }
      stack.push_back(Frame{node->left, frame.lower, node->key});
    }
    if (node->right != nullptr) {
      if (node->right->parent != node) {
        return false;
      }
      stack.push_back(Frame{node->right, node->key, frame.upper});
    }
  }
  return count == size_;
}

void SplaySet::clear() noexcept {
  if (root_ == nullptr) {
    return;
  }
  std::vector<Node*> stack;
  stack.push_back(root_);
  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();
    if (node->left != nullptr) {
      stack.push_back(node->left);
    }
    if (node->right != nullptr) {
      stack.push_back(node->right);
    }
    delete node;
  }
  root_ = nullptr;
  size_ = 0;
}

}  // namespace algorithms::data_structures
