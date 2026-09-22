#pragma once

#include "algorithms/graphs/graph.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

// Insertion-only exact transitive closure for a fixed directed vertex set.
//
// Reachability is reflexive: every vertex reaches itself through a zero-edge
// path. Explicit edges are tracked separately so inserting an edge that is
// already implied by another path still records one new graph edge.
class IncrementalTransitiveClosure {
 public:
  explicit IncrementalTransitiveClosure(const std::size_t vertex_count)
      : vertex_count_(vertex_count),
        word_count_(vertex_count / kWordBits +
                    (vertex_count % kWordBits != 0U ? 1U : 0U)),
        reachability_(
            vertex_count,
            std::vector<std::uint64_t>(word_count_, std::uint64_t{0})),
        explicit_edges_(
            vertex_count,
            std::vector<std::uint64_t>(word_count_, std::uint64_t{0})) {
    for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
      set_bit(reachability_[vertex], vertex);
    }
  }

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return vertex_count_;
  }

  [[nodiscard]] std::size_t edge_count() const noexcept { return edge_count_; }

  [[nodiscard]] bool reachable(const Vertex from, const Vertex to) const {
    validate_vertex(from);
    validate_vertex(to);
    return test_bit(reachability_[from], to);
  }

  [[nodiscard]] bool explicit_edge(const Vertex from, const Vertex to) const {
    validate_vertex(from);
    validate_vertex(to);
    return test_bit(explicit_edges_[from], to);
  }

  [[nodiscard]] bool same_strong_component(const Vertex first,
                                           const Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);
    return test_bit(reachability_[first], second) &&
           test_bit(reachability_[second], first);
  }

  // Returns whether inserting from -> to into the current graph would leave a
  // directed cycle containing that new edge. Existing duplicate-edge status is
  // intentionally ignored; this is a reachability predicate.
  [[nodiscard]] bool would_create_cycle(const Vertex from,
                                        const Vertex to) const {
    validate_vertex(from);
    validate_vertex(to);
    return from == to || test_bit(reachability_[to], from);
  }

  [[nodiscard]] std::size_t reachable_count_from(const Vertex from) const {
    validate_vertex(from);
    std::size_t total = 0U;
    for (const std::uint64_t word : reachability_[from]) {
      total += static_cast<std::size_t>(std::popcount(word));
    }
    return total;
  }

  // Inserts one unique explicit directed edge. Returns false only when that
  // exact explicit edge was already present. A transitively implied edge is
  // still a new explicit edge and therefore returns true.
  [[nodiscard]] bool add_edge(const Vertex from, const Vertex to) {
    validate_vertex(from);
    validate_vertex(to);

    if (test_bit(explicit_edges_[from], to)) {
      return false;
    }
    if (edge_count_ == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error(
          "incremental transitive closure edge count exhausted");
    }

    if (test_bit(reachability_[from], to)) {
      set_bit(explicit_edges_[from], to);
      ++edge_count_;
      return true;
    }

    // Every newly created path must be an old predecessor of 'from', followed
    // by the new edge, followed by an old successor of 'to'. Snapshot both
    // sides before mutating so allocation failure cannot leave a partial edit.
    const std::vector<std::uint64_t> successors = reachability_[to];
    std::vector<Vertex> predecessors;
    predecessors.reserve(vertex_count_);
    for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
      if (test_bit(reachability_[vertex], from)) {
        predecessors.push_back(vertex);
      }
    }

    set_bit(explicit_edges_[from], to);
    ++edge_count_;
    for (const Vertex predecessor : predecessors) {
      for (std::size_t word = 0U; word < word_count_; ++word) {
        reachability_[predecessor][word] |= successors[word];
      }
    }
    return true;
  }

  // Expensive diagnostic: independently recomputes reflexive transitive
  // closure from the explicit-edge matrix with a Floyd-Warshall bitset
  // recurrence and compares every stored reachability bit.
  [[nodiscard]] bool valid_invariants() const noexcept {
    try {
      if (reachability_.size() != vertex_count_ ||
          explicit_edges_.size() != vertex_count_) {
        return false;
      }

      std::size_t counted_edges = 0U;
      for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
        if (reachability_[vertex].size() != word_count_ ||
            explicit_edges_[vertex].size() != word_count_ ||
            !test_bit(reachability_[vertex], vertex)) {
          return false;
        }
        for (const std::uint64_t word : explicit_edges_[vertex]) {
          const std::size_t bits =
              static_cast<std::size_t>(std::popcount(word));
          if (bits > std::numeric_limits<std::size_t>::max() -
                         counted_edges) {
            return false;
          }
          counted_edges += bits;
        }
      }
      if (counted_edges != edge_count_) {
        return false;
      }

      std::vector<std::vector<std::uint64_t>> expected = explicit_edges_;
      for (Vertex vertex = 0U; vertex < vertex_count_; ++vertex) {
        set_bit(expected[vertex], vertex);
      }

      for (Vertex middle = 0U; middle < vertex_count_; ++middle) {
        for (Vertex from = 0U; from < vertex_count_; ++from) {
          if (!test_bit(expected[from], middle)) {
            continue;
          }
          for (std::size_t word = 0U; word < word_count_; ++word) {
            expected[from][word] |= expected[middle][word];
          }
        }
      }

      return expected == reachability_;
    } catch (...) {
      return false;
    }
  }

 private:
  static constexpr std::size_t kWordBits = 64U;

  std::size_t vertex_count_{};
  std::size_t word_count_{};
  std::size_t edge_count_{};
  std::vector<std::vector<std::uint64_t>> reachability_;
  std::vector<std::vector<std::uint64_t>> explicit_edges_;

  void validate_vertex(const Vertex vertex) const {
    if (vertex >= vertex_count_) {
      throw std::out_of_range(
          "incremental transitive closure vertex out of range");
    }
  }

  [[nodiscard]] static bool test_bit(
      const std::vector<std::uint64_t>& row,
      const Vertex vertex) noexcept {
    return (row[vertex / kWordBits] &
            (std::uint64_t{1} << (vertex % kWordBits))) != 0U;
  }

  static void set_bit(std::vector<std::uint64_t>& row,
                      const Vertex vertex) noexcept {
    row[vertex / kWordBits] |=
        std::uint64_t{1} << (vertex % kWordBits);
  }
};

}  // namespace algorithms::graphs
