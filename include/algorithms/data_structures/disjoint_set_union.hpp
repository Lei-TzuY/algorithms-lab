#pragma once

#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Disjoint-set union with path compression and union by size.
//
// Representative invariant: every node follows parent links to exactly one
// root r with parent[r] == r. component_size_ is meaningful only at roots.
//
// Amortized complexity of find/unite: O(alpha(n)); storage: O(n).
class DisjointSetUnion {
 public:
  explicit DisjointSetUnion(std::size_t element_count)
      : parent_(element_count), component_size_(element_count, 1),
        components_(element_count) {
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
  }

  [[nodiscard]] std::size_t size() const noexcept { return parent_.size(); }
  [[nodiscard]] std::size_t components() const noexcept { return components_; }

  std::size_t find(std::size_t element) {
    validate(element);
    std::size_t root = element;
    while (root != parent_[root]) {
      root = parent_[root];
    }
    while (element != root) {
      const std::size_t next = parent_[element];
      parent_[element] = root;
      element = next;
    }
    return root;
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
    parent_[root_b] = root_a;
    component_size_[root_a] += component_size_[root_b];
    --components_;
    return true;
  }

  bool connected(std::size_t a, std::size_t b) { return find(a) == find(b); }

  std::size_t component_size(std::size_t element) {
    return component_size_[find(element)];
  }

 private:
  void validate(std::size_t element) const {
    if (element >= parent_.size()) {
      throw std::out_of_range("DSU element out of range");
    }
  }

  std::vector<std::size_t> parent_;
  std::vector<std::size_t> component_size_;
  std::size_t components_;
};

}  // namespace algorithms::data_structures
