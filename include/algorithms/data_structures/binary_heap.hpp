#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Binary min-heap by default. Compare(a, b) means a has higher priority than b.
//
// Heap invariant: no child has higher priority than its parent.
// push/pop: O(log n), top: O(1), storage: O(n).
template <typename T, typename Compare = std::less<T>>
class BinaryHeap {
 public:
  [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

  [[nodiscard]] const T& top() const {
    if (data_.empty()) {
      throw std::out_of_range("BinaryHeap::top on empty heap");
    }
    return data_.front();
  }

  void push(const T& value) {
    data_.push_back(value);
    sift_up(data_.size() - 1);
  }

  void push(T&& value) {
    data_.push_back(std::move(value));
    sift_up(data_.size() - 1);
  }

  T pop() {
    if (data_.empty()) {
      throw std::out_of_range("BinaryHeap::pop on empty heap");
    }
    T result = std::move(data_.front());
    if (data_.size() == 1) {
      data_.pop_back();
      return result;
    }
    data_.front() = std::move(data_.back());
    data_.pop_back();
    sift_down(0);
    return result;
  }

  [[nodiscard]] bool valid_heap_property() const {
    for (std::size_t child = 1; child < data_.size(); ++child) {
      const std::size_t parent = (child - 1) / 2;
      if (compare_(data_[child], data_[parent])) {
        return false;
      }
    }
    return true;
  }

 private:
  void sift_up(std::size_t index) {
    while (index > 0) {
      const std::size_t parent = (index - 1) / 2;
      if (!compare_(data_[index], data_[parent])) {
        break;
      }
      std::swap(data_[index], data_[parent]);
      index = parent;
    }
  }

  void sift_down(std::size_t index) {
    while (true) {
      const std::size_t left = index * 2 + 1;
      if (left >= data_.size()) {
        return;
      }
      const std::size_t right = left + 1;
      std::size_t best = left;
      if (right < data_.size() && compare_(data_[right], data_[left])) {
        best = right;
      }
      if (!compare_(data_[best], data_[index])) {
        return;
      }
      std::swap(data_[index], data_[best]);
      index = best;
    }
  }

  std::vector<T> data_;
  [[no_unique_address]] Compare compare_{};
};

}  // namespace algorithms::data_structures
