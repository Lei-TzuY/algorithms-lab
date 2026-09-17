#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {
namespace detail {

class ExactSignedAccumulator {
 public:
  void add(std::int64_t value) { add_term(value, false); }
  void subtract(std::int64_t value) { add_term(value, true); }

  [[nodiscard]] std::int64_t narrow() const {
    if (limbs_.empty()) {
      return 0;
    }
    if (limbs_.size() != 1) {
      throw std::overflow_error("poset transform result is outside int64 range");
    }
    const std::uint64_t magnitude = limbs_.front();
    const std::uint64_t negative_limit = std::uint64_t{1} << 63U;
    if (!negative_) {
      if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("poset transform result is outside int64 range");
      }
      return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > negative_limit) {
      throw std::overflow_error("poset transform result is outside int64 range");
    }
    if (magnitude == negative_limit) {
      return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(magnitude);
  }

 private:
  bool negative_{false};
  std::vector<std::uint64_t> limbs_;

  static std::uint64_t magnitude_of(std::int64_t value) noexcept {
    const std::uint64_t converted = static_cast<std::uint64_t>(value);
    return value < 0 ? std::uint64_t{0} - converted : converted;
  }

  void add_term(std::int64_t value, bool negate) {
    const std::uint64_t magnitude = magnitude_of(value);
    if (magnitude == 0) {
      return;
    }
    bool term_negative = value < 0;
    if (negate) {
      term_negative = !term_negative;
    }
    add_magnitude(magnitude, term_negative);
  }

  void add_magnitude(std::uint64_t magnitude, bool term_negative) {
    if (limbs_.empty()) {
      limbs_.push_back(magnitude);
      negative_ = term_negative;
      return;
    }
    if (negative_ == term_negative) {
      add_small(magnitude);
      return;
    }
    const int comparison = compare_to_small(magnitude);
    if (comparison == 0) {
      limbs_.clear();
      negative_ = false;
      return;
    }
    if (comparison > 0) {
      subtract_small(magnitude);
      return;
    }
    const std::uint64_t current = limbs_.front();
    limbs_.assign(1, magnitude - current);
    negative_ = term_negative;
  }

  [[nodiscard]] int compare_to_small(std::uint64_t magnitude) const noexcept {
    if (limbs_.size() > 1) {
      return 1;
    }
    if (limbs_.front() < magnitude) {
      return -1;
    }
    if (limbs_.front() > magnitude) {
      return 1;
    }
    return 0;
  }

  void add_small(std::uint64_t value) {
    const std::uint64_t previous = limbs_.front();
    limbs_.front() += value;
    bool carry = limbs_.front() < previous;
    std::size_t index = 1;
    while (carry && index < limbs_.size()) {
      const std::uint64_t old = limbs_[index];
      ++limbs_[index];
      carry = limbs_[index] < old;
      ++index;
    }
    if (carry) {
      limbs_.push_back(1);
    }
  }

  void subtract_small(std::uint64_t value) {
    const std::uint64_t previous = limbs_.front();
    limbs_.front() -= value;
    bool borrow = previous < value;
    std::size_t index = 1;
    while (borrow) {
      const std::uint64_t old = limbs_[index];
      --limbs_[index];
      borrow = old == 0;
      ++index;
    }
    while (!limbs_.empty() && limbs_.back() == 0) {
      limbs_.pop_back();
    }
    if (limbs_.empty()) {
      negative_ = false;
    }
  }
};

}  // namespace detail

class FinitePosetIndex {
 public:
  explicit FinitePosetIndex(const graphs::Graph& relation_dag)
      : reachable_(relation_dag.vertex_count(),
                   std::vector<unsigned char>(relation_dag.vertex_count(), 0)),
        adjacency_(relation_dag.vertex_count()) {
    if (!relation_dag.directed()) {
      throw std::invalid_argument("finite poset relation graph must be directed");
    }

    const std::size_t n = relation_dag.vertex_count();
    std::vector<std::size_t> indegree(n, 0);
    for (graphs::Vertex from = 0; from < n; ++from) {
      for (const auto& edge : relation_dag.neighbors(from)) {
        if (edge.to == from) {
          throw std::invalid_argument("finite poset relation graph must be acyclic");
        }
        if (reachable_[from][edge.to] == 0) {
          reachable_[from][edge.to] = 1;
          adjacency_[from].push_back(edge.to);
          ++indegree[edge.to];
        }
      }
      std::sort(adjacency_[from].begin(), adjacency_[from].end());
    }

    std::priority_queue<graphs::Vertex, std::vector<graphs::Vertex>,
                        std::greater<graphs::Vertex>> ready;
    for (graphs::Vertex vertex = 0; vertex < n; ++vertex) {
      if (indegree[vertex] == 0) {
        ready.push(vertex);
      }
    }
    while (!ready.empty()) {
      const graphs::Vertex vertex = ready.top();
      ready.pop();
      linear_extension_.push_back(vertex);
      for (const graphs::Vertex next : adjacency_[vertex]) {
        --indegree[next];
        if (indegree[next] == 0) {
          ready.push(next);
        }
      }
    }
    if (linear_extension_.size() != n) {
      throw std::invalid_argument("finite poset relation graph must be acyclic");
    }

    for (auto iterator = linear_extension_.rbegin();
         iterator != linear_extension_.rend(); ++iterator) {
      const graphs::Vertex vertex = *iterator;
      reachable_[vertex][vertex] = 1;
      for (const graphs::Vertex next : adjacency_[vertex]) {
        for (graphs::Vertex target = 0; target < n; ++target) {
          if (reachable_[next][target] != 0) {
            reachable_[vertex][target] = 1;
          }
        }
      }
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return reachable_.size(); }

  [[nodiscard]] const std::vector<graphs::Vertex>& linear_extension() const noexcept {
    return linear_extension_;
  }

  [[nodiscard]] bool less_equal(graphs::Vertex lower, graphs::Vertex upper) const {
    validate_vertex(lower);
    validate_vertex(upper);
    return reachable_[lower][upper] != 0;
  }

  [[nodiscard]] std::vector<std::int64_t> zeta_transform(
      std::span<const std::int64_t> values) const {
    validate_values(values.size());
    const std::size_t n = size();
    std::vector<std::int64_t> transformed(n, 0);
    for (graphs::Vertex upper = 0; upper < n; ++upper) {
      detail::ExactSignedAccumulator total;
      for (graphs::Vertex lower = 0; lower < n; ++lower) {
        if (reachable_[lower][upper] != 0) {
          total.add(values[lower]);
        }
      }
      transformed[upper] = total.narrow();
    }
    return transformed;
  }

  [[nodiscard]] std::vector<std::int64_t> mobius_invert(
      std::span<const std::int64_t> zeta_values) const {
    validate_values(zeta_values.size());
    std::vector<std::int64_t> values(size(), 0);
    for (const graphs::Vertex upper : linear_extension_) {
      detail::ExactSignedAccumulator value;
      value.add(zeta_values[upper]);
      for (const graphs::Vertex lower : linear_extension_) {
        if (lower == upper) {
          break;
        }
        if (reachable_[lower][upper] != 0) {
          value.subtract(values[lower]);
        }
      }
      values[upper] = value.narrow();
    }
    return values;
  }

 private:
  std::vector<std::vector<unsigned char>> reachable_;
  std::vector<std::vector<graphs::Vertex>> adjacency_;
  std::vector<graphs::Vertex> linear_extension_;

  void validate_vertex(graphs::Vertex vertex) const {
    if (vertex >= size()) {
      throw std::out_of_range("finite poset vertex out of range");
    }
  }

  void validate_values(std::size_t count) const {
    if (count != size()) {
      throw std::invalid_argument("finite poset transform vector size mismatch");
    }
  }
};

}  // namespace algorithms::combinatorial
