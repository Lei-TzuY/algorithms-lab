#pragma once

#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Disjoint-set union with union by size and exact rollback to prior snapshots.
//
// Path compression is intentionally forbidden: find is read-only, so every
// state mutation is one successful root-link operation that can be undone from
// a compact LIFO history record.
//
// With union by size, tree depth is O(log n). snapshot() is O(1); rolling back
// each successful union is O(1).
class RollbackDisjointSetUnion {
 public:
  using Snapshot = std::size_t;

  explicit RollbackDisjointSetUnion(std::size_t element_count)
      : parent_(element_count), component_size_(element_count, 1),
        components_(element_count) {
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
  }

  [[nodiscard]] std::size_t size() const noexcept { return parent_.size(); }
  [[nodiscard]] std::size_t components() const noexcept { return components_; }
  [[nodiscard]] Snapshot snapshot() const noexcept { return history_.size(); }

  [[nodiscard]] std::size_t find(std::size_t element) const {
    validate(element);
    while (element != parent_[element]) {
      element = parent_[element];
    }
    return element;
  }

  [[nodiscard]] bool connected(std::size_t a, std::size_t b) const {
    return find(a) == find(b);
  }

  [[nodiscard]] std::size_t component_size(std::size_t element) const {
    return component_size_[find(element)];
  }

  bool unite(std::size_t a, std::size_t b) {
    std::size_t root_a = find(a);
    std::size_t root_b = find(b);
    if (root_a == root_b) {
      return false;
    }

    if (component_size_[root_a] < component_size_[root_b]) {
      std::swap(root_a, root_b);
    }
    if (component_size_[root_a] >
        std::numeric_limits<std::size_t>::max() - component_size_[root_b]) {
      throw std::overflow_error("rollback DSU component-size overflow");
    }

    // Record the exact pre-state before mutating anything. If history growth
    // throws, the partition is unchanged.
    history_.push_back(
        Change{root_b, root_a, component_size_[root_a]});
    parent_[root_b] = root_a;
    component_size_[root_a] += component_size_[root_b];
    --components_;
    return true;
  }

  void rollback(Snapshot target) {
    if (target > history_.size()) {
      throw std::out_of_range("rollback DSU snapshot is no longer reachable");
    }

    while (history_.size() > target) {
      const Change change = history_.back();
      history_.pop_back();
      parent_[change.child_root] = change.child_root;
      component_size_[change.parent_root] = change.parent_size_before;
      ++components_;
    }
  }

 private:
  struct Change {
    std::size_t child_root;
    std::size_t parent_root;
    std::size_t parent_size_before;
  };

  void validate(std::size_t element) const {
    if (element >= parent_.size()) {
      throw std::out_of_range("rollback DSU element out of range");
    }
  }

  std::vector<std::size_t> parent_;
  std::vector<std::size_t> component_size_;
  std::size_t components_;
  std::vector<Change> history_;
};

}  // namespace algorithms::data_structures
