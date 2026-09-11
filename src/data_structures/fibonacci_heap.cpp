#include "algorithms/data_structures/fibonacci_heap.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {
namespace {

std::atomic<FibonacciMinHeap::Handle> next_handle{1U};

}  // namespace

FibonacciMinHeap::FibonacciMinHeap(FibonacciMinHeap&& other) noexcept {
  meld(std::move(other));
}

FibonacciMinHeap& FibonacciMinHeap::operator=(FibonacciMinHeap&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  nodes_.clear();
  min_ = nullptr;
  root_count_ = 0U;
  marked_count_ = 0U;
  meld(std::move(other));
  return *this;
}

bool FibonacciMinHeap::empty() const noexcept { return nodes_.empty(); }

std::size_t FibonacciMinHeap::size() const noexcept { return nodes_.size(); }

bool FibonacciMinHeap::contains(Handle handle) const noexcept {
  return nodes_.find(handle) != nodes_.end();
}

FibonacciMinHeap::Entry FibonacciMinHeap::minimum() const {
  if (min_ == nullptr) {
    throw std::out_of_range("FibonacciMinHeap::minimum on empty heap");
  }
  return Entry{min_->handle, min_->key};
}

FibonacciMinHeap::Handle FibonacciMinHeap::allocate_handle() {
  Handle current = next_handle.load(std::memory_order_relaxed);
  while (true) {
    if (current == 0U) {
      throw std::overflow_error("FibonacciMinHeap handle space exhausted");
    }
    const Handle next =
        current == std::numeric_limits<Handle>::max() ? 0U : current + 1U;
    if (next_handle.compare_exchange_weak(current, next,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
      return current;
    }
  }
}

FibonacciMinHeap::Handle FibonacciMinHeap::insert(Key key) {
  const Handle handle = allocate_handle();
  auto owned = std::make_unique<Node>();
  Node* node = owned.get();
  node->handle = handle;
  node->key = key;
  make_singleton(node);
  nodes_.emplace(handle, std::move(owned));
  add_root(node);
  return handle;
}

void FibonacciMinHeap::decrease_key(Handle handle, Key new_key) {
  Node* node = find_node(handle);
  if (new_key > node->key) {
    throw std::invalid_argument("FibonacciMinHeap::decrease_key increased key");
  }

  node->key = new_key;
  Node* parent = node->parent;
  if (parent != nullptr && root_better(node, parent)) {
    cut(node, parent);
    cascading_cut(parent);
  }
  if (min_ == nullptr || root_better(node, min_)) {
    min_ = node;
  }
}

FibonacciMinHeap::Entry FibonacciMinHeap::extract_min() {
  if (min_ == nullptr) {
    throw std::out_of_range("FibonacciMinHeap::extract_min on empty heap");
  }

  Node* removed = min_;
  const Entry result{removed->handle, removed->key};

  std::vector<Node*> children;
  if (removed->child != nullptr) {
    children.reserve(removed->degree);
    Node* current = removed->child;
    do {
      children.push_back(current);
      current = current->right;
    } while (current != removed->child);
  }
  for (Node* child : children) {
    child->parent = nullptr;
    if (child->mark) {
      child->mark = false;
      --marked_count_;
    }
    make_singleton(child);
  }
  removed->child = nullptr;
  removed->degree = 0U;

  if (root_count_ == 1U) {
    min_ = nullptr;
  } else {
    removed->left->right = removed->right;
    removed->right->left = removed->left;
    min_ = removed->right;
  }
  --root_count_;
  make_singleton(removed);

  for (Node* child : children) {
    add_root(child);
  }

  nodes_.erase(result.handle);
  if (min_ != nullptr) {
    consolidate();
  }
  return result;
}

void FibonacciMinHeap::meld(FibonacciMinHeap&& other) {
  if (this == &other || other.empty()) {
    return;
  }
  if (empty()) {
    nodes_ = std::move(other.nodes_);
    min_ = other.min_;
    root_count_ = other.root_count_;
    marked_count_ = other.marked_count_;
  } else {
    // Complete all potentially allocating ownership bookkeeping before
    // mutating either circular root list. With globally unique handles,
    // unordered_map::merge then transfers the already-allocated nodes.
    nodes_.reserve(nodes_.size() + other.nodes_.size());
    nodes_.merge(other.nodes_);

    Node* first_right = min_->right;
    Node* second_left = other.min_->left;

    min_->right = other.min_;
    other.min_->left = min_;
    first_right->left = second_left;
    second_left->right = first_right;

    if (root_better(other.min_, min_)) {
      min_ = other.min_;
    }
    root_count_ += other.root_count_;
    marked_count_ += other.marked_count_;
  }

  other.min_ = nullptr;
  other.root_count_ = 0U;
  other.marked_count_ = 0U;
}

std::size_t FibonacciMinHeap::root_count() const noexcept {
  return root_count_;
}

std::size_t FibonacciMinHeap::marked_count() const noexcept {
  return marked_count_;
}

std::size_t FibonacciMinHeap::potential() const noexcept {
  return root_count_ + 2U * marked_count_;
}

FibonacciMinHeap::Node* FibonacciMinHeap::find_node(Handle handle) {
  const auto it = nodes_.find(handle);
  if (it == nodes_.end()) {
    throw std::out_of_range("FibonacciMinHeap handle is not active");
  }
  return it->second.get();
}

const FibonacciMinHeap::Node* FibonacciMinHeap::find_node(Handle handle) const {
  const auto it = nodes_.find(handle);
  if (it == nodes_.end()) {
    throw std::out_of_range("FibonacciMinHeap handle is not active");
  }
  return it->second.get();
}

bool FibonacciMinHeap::root_better(const Node* first,
                                   const Node* second) noexcept {
  return first->key < second->key ||
         (first->key == second->key && first->handle < second->handle);
}

void FibonacciMinHeap::make_singleton(Node* node) noexcept {
  node->left = node;
  node->right = node;
}

void FibonacciMinHeap::splice_after(Node* position, Node* node) noexcept {
  node->right = position->right;
  node->left = position;
  position->right->left = node;
  position->right = node;
}

void FibonacciMinHeap::add_root(Node* node) noexcept {
  node->parent = nullptr;
  if (node->mark) {
    node->mark = false;
    --marked_count_;
  }
  if (min_ == nullptr) {
    make_singleton(node);
    min_ = node;
  } else {
    splice_after(min_, node);
    if (root_better(node, min_)) {
      min_ = node;
    }
  }
  ++root_count_;
}

void FibonacciMinHeap::add_child(Node* child, Node* parent) noexcept {
  child->parent = parent;
  if (child->mark) {
    child->mark = false;
    --marked_count_;
  }
  if (parent->child == nullptr) {
    make_singleton(child);
    parent->child = child;
  } else {
    splice_after(parent->child, child);
  }
  ++parent->degree;
}

void FibonacciMinHeap::remove_child(Node* child, Node* parent) noexcept {
  if (child->right == child) {
    parent->child = nullptr;
  } else {
    if (parent->child == child) {
      parent->child = child->right;
    }
    child->left->right = child->right;
    child->right->left = child->left;
  }
  --parent->degree;
  make_singleton(child);
}

void FibonacciMinHeap::cut(Node* child, Node* parent) noexcept {
  remove_child(child, parent);
  child->parent = nullptr;
  add_root(child);
}

void FibonacciMinHeap::cascading_cut(Node* node) noexcept {
  Node* parent = node->parent;
  if (parent == nullptr) {
    return;
  }
  if (!node->mark) {
    node->mark = true;
    ++marked_count_;
    return;
  }
  cut(node, parent);
  cascading_cut(parent);
}

void FibonacciMinHeap::consolidate() {
  std::vector<Node*> roots;
  roots.reserve(root_count_);
  Node* current = min_;
  for (std::size_t index = 0U; index < root_count_; ++index) {
    roots.push_back(current);
    current = current->right;
  }

  for (Node* root : roots) {
    make_singleton(root);
  }
  min_ = nullptr;
  root_count_ = 0U;

  std::vector<Node*> by_degree;
  for (Node* root : roots) {
    Node* node = root;
    std::size_t degree = node->degree;
    while (true) {
      if (degree >= by_degree.size()) {
        by_degree.resize(degree + 1U, nullptr);
      }
      if (by_degree[degree] == nullptr) {
        by_degree[degree] = node;
        break;
      }

      Node* peer = by_degree[degree];
      by_degree[degree] = nullptr;
      if (root_better(peer, node)) {
        std::swap(node, peer);
      }
      add_child(peer, node);
      degree = node->degree;
    }
  }

  for (Node* root : by_degree) {
    if (root != nullptr) {
      add_root(root);
    }
  }
}

bool FibonacciMinHeap::validate_subtree(
    const Node* node, const Node* expected_parent,
    std::unordered_map<Handle, bool>& seen, std::size_t& marked) const {
  if (node == nullptr || node->parent != expected_parent ||
      node->left == nullptr || node->right == nullptr ||
      node->left->right != node || node->right->left != node) {
    return false;
  }

  const auto owned = nodes_.find(node->handle);
  if (owned == nodes_.end() || owned->second.get() != node || seen[node->handle]) {
    return false;
  }
  seen[node->handle] = true;
  if (node->mark) {
    ++marked;
  }

  if (expected_parent != nullptr && root_better(node, expected_parent)) {
    return false;
  }

  std::size_t child_count = 0U;
  if (node->child != nullptr) {
    const Node* child = node->child;
    do {
      if (!validate_subtree(child, node, seen, marked)) {
        return false;
      }
      ++child_count;
      child = child->right;
      if (child_count > nodes_.size()) {
        return false;
      }
    } while (child != node->child);
  }
  return child_count == node->degree;
}

bool FibonacciMinHeap::valid_structure() const {
  if (nodes_.empty()) {
    return min_ == nullptr && root_count_ == 0U && marked_count_ == 0U;
  }
  if (min_ == nullptr || root_count_ == 0U || min_->parent != nullptr) {
    return false;
  }

  std::unordered_map<Handle, bool> seen;
  seen.reserve(nodes_.size());
  for (const auto& [handle, node] : nodes_) {
    static_cast<void>(node);
    seen.emplace(handle, false);
  }

  const Node* best = nullptr;
  const Node* root = min_;
  std::size_t roots = 0U;
  std::size_t marked = 0U;
  do {
    if (root->parent != nullptr || root->mark ||
        !validate_subtree(root, nullptr, seen, marked)) {
      return false;
    }
    if (best == nullptr || root_better(root, best)) {
      best = root;
    }
    ++roots;
    if (roots > nodes_.size()) {
      return false;
    }
    root = root->right;
  } while (root != min_);

  if (roots != root_count_ || marked != marked_count_ || best != min_) {
    return false;
  }
  return std::all_of(seen.begin(), seen.end(),
                     [](const auto& entry) { return entry.second; });
}

}  // namespace algorithms::data_structures
