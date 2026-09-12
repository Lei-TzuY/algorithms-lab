#pragma once

#include "algorithms/graphs/graph.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

inline constexpr std::size_t kExactTreewidthMaxVertices = 20U;

struct TreewidthEliminationStep {
  Vertex vertex{};
  std::vector<Vertex> bag;

  friend bool operator==(const TreewidthEliminationStep&,
                         const TreewidthEliminationStep&) = default;
};

struct ExactTreewidthResult {
  std::size_t width{};
  std::vector<Vertex> elimination_order;
  std::vector<TreewidthEliminationStep> steps;
};

namespace treewidth_detail {

using Mask = std::uint64_t;
inline constexpr std::size_t kUncomputed =
    std::numeric_limits<std::size_t>::max();

class ExactTreewidthSolver {
 public:
  explicit ExactTreewidthSolver(const Graph& graph)
      : vertex_count_(graph.vertex_count()),
        full_mask_(vertex_count_ == 0U
                       ? Mask{0}
                       : (Mask{1} << vertex_count_) - Mask{1}),
        adjacency_(vertex_count_, Mask{0}),
        memo_(std::size_t{1} << vertex_count_, kUncomputed) {
    for (Vertex from = 0; from < vertex_count_; ++from) {
      for (const Edge& edge : graph.neighbors(from)) {
        if (edge.to != from) {
          adjacency_[from] |= Mask{1} << edge.to;
        }
      }
    }
    memo_[static_cast<std::size_t>(full_mask_)] = 0U;
  }

  [[nodiscard]] ExactTreewidthResult solve() {
    ExactTreewidthResult result;
    result.width = solve_state(Mask{0});

    Mask eliminated = 0;
    while (eliminated != full_mask_) {
      Vertex vertex = vertex_count_;
      for (Vertex candidate = 0; candidate < vertex_count_; ++candidate) {
        const Mask candidate_bit = Mask{1} << candidate;
        if ((eliminated & candidate_bit) != 0U) {
          continue;
        }
        const std::size_t local_width = static_cast<std::size_t>(
            std::popcount(torso_neighbors(eliminated, candidate)));
        const std::size_t suffix_width = solve_state(eliminated | candidate_bit);
        if (local_width <= result.width && suffix_width <= result.width) {
          vertex = candidate;
          break;
        }
      }
      if (vertex >= vertex_count_) {
        throw std::logic_error(
            "treewidth reconstruction lost an optimal elimination choice");
      }

      const Mask neighbors = torso_neighbors(eliminated, vertex);
      TreewidthEliminationStep step;
      step.vertex = vertex;
      step.bag.push_back(vertex);
      for (Vertex other = 0; other < vertex_count_; ++other) {
        if ((neighbors & (Mask{1} << other)) != 0U) {
          step.bag.push_back(other);
        }
      }

      result.elimination_order.push_back(vertex);
      result.steps.push_back(std::move(step));
      eliminated |= Mask{1} << vertex;
    }
    return result;
  }

 private:
  [[nodiscard]] Mask torso_neighbors(const Mask eliminated,
                                     const Vertex vertex) const {
    const Mask vertex_bit = Mask{1} << vertex;
    const Mask remaining = full_mask_ & ~eliminated;
    const Mask eligible_neighbors = remaining & ~vertex_bit;

    Mask neighbors = adjacency_[vertex] & eligible_neighbors;
    Mask frontier = adjacency_[vertex] & eliminated;
    Mask reached = 0;
    while (frontier != 0U) {
      const Vertex through =
          static_cast<Vertex>(std::countr_zero(frontier));
      const Mask bit = Mask{1} << through;
      frontier &= ~bit;
      if ((reached & bit) != 0U) {
        continue;
      }
      reached |= bit;
      neighbors |= adjacency_[through] & eligible_neighbors;
      frontier |= adjacency_[through] & eliminated & ~reached;
    }
    return neighbors;
  }

  [[nodiscard]] std::size_t solve_state(const Mask eliminated) {
    const std::size_t state_index = static_cast<std::size_t>(eliminated);
    if (memo_[state_index] != kUncomputed) {
      return memo_[state_index];
    }

    std::size_t best_width = vertex_count_;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      const Mask vertex_bit = Mask{1} << vertex;
      if ((eliminated & vertex_bit) != 0U) {
        continue;
      }
      const std::size_t local_width = static_cast<std::size_t>(
          std::popcount(torso_neighbors(eliminated, vertex)));
      const std::size_t suffix_width = solve_state(eliminated | vertex_bit);
      const std::size_t candidate_width =
          local_width > suffix_width ? local_width : suffix_width;
      if (candidate_width < best_width) {
        best_width = candidate_width;
      }
    }
    memo_[state_index] = best_width;
    return best_width;
  }

  std::size_t vertex_count_;
  Mask full_mask_;
  std::vector<Mask> adjacency_;
  std::vector<std::size_t> memo_;
};

}  // namespace treewidth_detail

// Exact treewidth through elimination-order subset dynamic programming.
//
// Preconditions:
// - graph must be undirected;
// - graph.vertex_count() <= kExactTreewidthMaxVertices.
//
// Self-loops are ignored, parallel edges collapse to one simple adjacency, and
// edge weights do not affect treewidth. The result contains the exact width,
// the lexicographically smallest optimal elimination order, and each replayable
// elimination bag (eliminated vertex followed by its sorted filled neighbors).
//
// Direct bounded baseline: O(n^2 * 2^n) time and O(2^n + n) auxiliary DP state,
// excluding the returned O(n^2) worst-case bags.
[[nodiscard]] inline ExactTreewidthResult exact_treewidth(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("exact treewidth requires an undirected graph");
  }
  if (graph.vertex_count() > kExactTreewidthMaxVertices) {
    throw std::length_error("exact treewidth vertex limit exceeded");
  }
  treewidth_detail::ExactTreewidthSolver solver(graph);
  return solver.solve();
}

}  // namespace algorithms::graphs
