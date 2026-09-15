#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Two-pass pairing heap. Compare(a, b) means a has higher priority than b.
//
// top/insert/meld: O(1) worst-case for this representation.
// pop: O(n) worst-case for one operation and O(log n) amortized for the
// standard two-pass pairing-heap discipline. Storage: O(n).
//
// This baseline intentionally omits handles/decrease-key. Meld requires the two
// heaps to use equivalent ordering semantics; stateful comparator equivalence is
// a caller precondition and is not dynamically checked.
template <typename T, typename Compare = std::less<T>>
class PairingHeap {
 public:
  PairingHeap() = default;

  explicit PairingHeap(Compare compare) : compare_(std::move(compare)) {}

  PairingHeap(const PairingHeap&) = delete;
  PairingHeap& operator=(const PairingHeap&) = delete;

  PairingHeap(PairingHeap&& other) noexcept(
      std::is_nothrow_move_constructible_v<Compare>)
      : root_(std::move(other.root_)),
        size_(std::exchange(other.size_, 0)),
        compare_(std::move(other.compare_)) {}

  PairingHeap& operator=(PairingHeap&& other) noexcept(
      std::is_nothrow_move_assignable_v<Compare>) {
    if (this == &other) {
      return *this;
    }
    clear_nodes();
    root_ = std::move(other.root_);
    size_ = std::exchange(other.size_, 0);
    compare_ = std::move(other.compare_);
    return *this;
  }

  ~PairingHeap() { clear_nodes(); }

  [[nodiscard]] bool empty() const noexcept { return root_ == nullptr; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  [[nodiscard]] const T& top() const {
    if (!root_) {
      throw std::out_of_range("PairingHeap::top on empty heap");
    }
    return root_->value;
  }

  void push(const T& value) {
    auto singleton = std::make_unique<Node>(value);
    root_ = meld_nodes(std::move(root_), std::move(singleton));
    ++size_;
  }

  void push(T&& value) {
    auto singleton = std::make_unique<Node>(std::move(value));
    root_ = meld_nodes(std::move(root_), std::move(singleton));
    ++size_;
  }

  // Destructively melds other into *this and leaves other empty.
  void meld(PairingHeap&& other) {
    if (this == &other || other.empty()) {
      return;
    }
    root_ = meld_nodes(std::move(root_), std::move(other.root_));
    size_ += other.size_;
    other.size_ = 0;
  }

  T pop() {
    if (!root_) {
      throw std::out_of_range("PairingHeap::pop on empty heap");
    }

    // Reserve first so allocation failure while preparing the two-pass combine
    // cannot detach the existing tree.
    std::size_t child_count = 0;
    for (const Node* child = root_->first_child.get(); child != nullptr;
         child = child->next_sibling.get()) {
      ++child_count;
    }
    std::vector<std::unique_ptr<Node>> paired;
    paired.reserve((child_count + 1U) / 2U);

    T result = std::move(root_->value);
    auto children = std::move(root_->first_child);
    root_.reset();
    --size_;

    while (children) {
      auto first = detach_front(children);
      if (!children) {
        paired.push_back(std::move(first));
        break;
      }
      auto second = detach_front(children);
      paired.push_back(meld_nodes(std::move(first), std::move(second)));
    }

    std::unique_ptr<Node> rebuilt;
    while (!paired.empty()) {
      auto current = std::move(paired.back());
      paired.pop_back();
      rebuilt = meld_nodes(std::move(current), std::move(rebuilt));
    }
    root_ = std::move(rebuilt);
    return result;
  }

  [[nodiscard]] bool valid_structure() const {
    if (!root_) {
      return size_ == 0;
    }
    if (root_->next_sibling) {
      return false;
    }

    std::size_t visited = 0;
    std::vector<const Node*> stack;
    stack.push_back(root_.get());
    while (!stack.empty()) {
      const Node* node = stack.back();
      stack.pop_back();
      ++visited;
      for (const Node* child = node->first_child.get(); child != nullptr;
           child = child->next_sibling.get()) {
        if (compare_(child->value, node->value)) {
          return false;
        }
        stack.push_back(child);
      }
    }
    return visited == size_;
  }

 private:
  struct Node {
    explicit Node(const T& item) : value(item) {}
    explicit Node(T&& item) : value(std::move(item)) {}

    T value;
    std::unique_ptr<Node> first_child;
    std::unique_ptr<Node> next_sibling;
    Node* destroy_next{nullptr};
  };

  std::unique_ptr<Node> meld_nodes(std::unique_ptr<Node> first,
                                   std::unique_ptr<Node> second) {
    if (!first) {
      return second;
    }
    if (!second) {
      return first;
    }

    if (compare_(second->value, first->value)) {
      std::swap(first, second);
    }
    second->next_sibling = std::move(first->first_child);
    first->first_child = std::move(second);
    return first;
  }

  static std::unique_ptr<Node> detach_front(std::unique_ptr<Node>& siblings) {
    auto front = std::move(siblings);
    siblings = std::move(front->next_sibling);
    front->next_sibling.reset();
    return front;
  }


  void clear_nodes() noexcept {
    Node* pending = root_.release();
    if (pending != nullptr) {
      pending->destroy_next = nullptr;
    }
    while (pending != nullptr) {
      Node* node = pending;
      pending = node->destroy_next;

      if (Node* sibling = node->next_sibling.release(); sibling != nullptr) {
        sibling->destroy_next = pending;
        pending = sibling;
      }
      if (Node* child = node->first_child.release(); child != nullptr) {
        child->destroy_next = pending;
        pending = child;
      }
      delete node;
    }
    size_ = 0;
  }

  std::unique_ptr<Node> root_;
  std::size_t size_{0};
  [[no_unique_address]] Compare compare_{};
};

}  // namespace algorithms::data_structures
