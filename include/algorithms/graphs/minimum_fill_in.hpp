#pragma once

#include "algorithms/graphs/graph.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

inline constexpr std::size_t kExactMinimumFillInMaxVertices = 20U;

struct FillEdge {
  Vertex first{};
  Vertex second{};

  friend bool operator==(const FillEdge&, const FillEdge&) = default;
  friend bool operator<(const FillEdge& lhs, const FillEdge& rhs) {
    return lhs.first < rhs.first ||
           (lhs.first == rhs.first && lhs.second < rhs.second);
  }
};

struct MinimumFillInStep {
  Vertex vertex{};
  std::vector<Vertex> current_neighbors;
  std::vector<FillEdge> added_edges;

  friend bool operator==(const MinimumFillInStep&,
                         const MinimumFillInStep&) = default;
};

struct ExactMinimumFillInResult {
  std::size_t fill_edge_count{};
  std::vector<Vertex> elimination_order;
  std::vector<FillEdge> fill_edges;
  std::vector<MinimumFillInStep> steps;
};

namespace minimum_fill_in_detail {

using Mask = std::uint64_t;
inline constexpr std::size_t kUncomputed =
    std::numeric_limits<std::size_t>::max();

struct StateView {
  std::array<Mask, kExactMinimumFillInMaxVertices> torso{};
};

class Solver {
 public:
  explicit Solver(const Graph& graph)
      : vertex_count_(graph.vertex_count()),
        full_mask_(vertex_count_ == 0U
                       ? Mask{0}
                       : (Mask{1} << vertex_count_) - Mask{1}),
        adjacency_{},
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

  [[nodiscard]] ExactMinimumFillInResult solve() {
    ExactMinimumFillInResult result;
    result.fill_edge_count = solve_state(Mask{0});

    Mask eliminated = 0U;
    while (eliminated != full_mask_) {
      const StateView state = build_state(eliminated);
      Vertex chosen = vertex_count_;
      std::size_t chosen_local = 0U;
      for (Vertex candidate = 0; candidate < vertex_count_; ++candidate) {
        const Mask bit = Mask{1} << candidate;
        if ((eliminated & bit) != 0U) {
          continue;
        }
        const std::size_t local = local_fill_cost(state, candidate);
        const std::size_t suffix = solve_state(eliminated | bit);
        if (local + suffix == memo_[static_cast<std::size_t>(eliminated)]) {
          chosen = candidate;
          chosen_local = local;
          break;
        }
      }
      if (chosen >= vertex_count_) {
        throw std::logic_error(
            "minimum fill-in reconstruction lost an optimal elimination choice");
      }

      MinimumFillInStep step;
      step.vertex = chosen;
      const Mask neighbors = state.torso[chosen];
      for (Vertex other = 0; other < vertex_count_; ++other) {
        if ((neighbors & (Mask{1} << other)) != 0U) {
          step.current_neighbors.push_back(other);
        }
      }
      for (std::size_t i = 0; i < step.current_neighbors.size(); ++i) {
        const Vertex first = step.current_neighbors[i];
        for (std::size_t j = i + 1U; j < step.current_neighbors.size(); ++j) {
          const Vertex second = step.current_neighbors[j];
          if ((state.torso[first] & (Mask{1} << second)) == 0U) {
            step.added_edges.push_back(FillEdge{first, second});
            result.fill_edges.push_back(FillEdge{first, second});
          }
        }
      }
      if (step.added_edges.size() != chosen_local) {
        throw std::logic_error("minimum fill-in local witness count mismatch");
      }
      result.elimination_order.push_back(chosen);
      result.steps.push_back(std::move(step));
      eliminated |= Mask{1} << chosen;
    }

    if (result.fill_edges.size() != result.fill_edge_count) {
      throw std::logic_error("minimum fill-in witness count mismatch");
    }
    return result;
  }

 private:
  [[nodiscard]] StateView build_state(const Mask eliminated) const {
    StateView state;
    const Mask remaining = full_mask_ & ~eliminated;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      const Mask bit = Mask{1} << vertex;
      if ((remaining & bit) != 0U) {
        state.torso[vertex] = adjacency_[vertex] & remaining & ~bit;
      }
    }

    Mask unseen = eliminated;
    while (unseen != 0U) {
      const Vertex seed = static_cast<Vertex>(std::countr_zero(unseen));
      Mask frontier = Mask{1} << seed;
      Mask component = 0U;
      Mask boundary = 0U;
      while (frontier != 0U) {
        const Vertex vertex = static_cast<Vertex>(std::countr_zero(frontier));
        const Mask bit = Mask{1} << vertex;
        frontier &= ~bit;
        if ((component & bit) != 0U) {
          continue;
        }
        component |= bit;
        unseen &= ~bit;
        boundary |= adjacency_[vertex] & remaining;
        frontier |= adjacency_[vertex] & eliminated & ~component;
      }

      Mask boundary_scan = boundary;
      while (boundary_scan != 0U) {
        const Vertex vertex =
            static_cast<Vertex>(std::countr_zero(boundary_scan));
        const Mask bit = Mask{1} << vertex;
        boundary_scan &= ~bit;
        state.torso[vertex] |= boundary & ~bit;
      }
    }
    return state;
  }

  [[nodiscard]] static std::size_t local_fill_cost(const StateView& state,
                                                   const Vertex vertex) {
    const Mask neighbors = state.torso[vertex];
    const std::size_t degree =
        static_cast<std::size_t>(std::popcount(neighbors));
    const std::size_t all_pairs =
        degree < 2U ? 0U : degree * (degree - 1U) / 2U;

    std::size_t existing_twice = 0U;
    Mask scan = neighbors;
    while (scan != 0U) {
      const Vertex other = static_cast<Vertex>(std::countr_zero(scan));
      const Mask bit = Mask{1} << other;
      scan &= ~bit;
      existing_twice +=
          static_cast<std::size_t>(std::popcount(state.torso[other] & neighbors));
    }
    return all_pairs - existing_twice / 2U;
  }

  [[nodiscard]] std::size_t solve_state(const Mask eliminated) {
    const std::size_t index = static_cast<std::size_t>(eliminated);
    if (memo_[index] != kUncomputed) {
      return memo_[index];
    }

    const StateView state = build_state(eliminated);
    std::size_t best = kUncomputed;
    for (Vertex vertex = 0; vertex < vertex_count_; ++vertex) {
      const Mask bit = Mask{1} << vertex;
      if ((eliminated & bit) != 0U) {
        continue;
      }
      const std::size_t local = local_fill_cost(state, vertex);
      const std::size_t suffix = solve_state(eliminated | bit);
      const std::size_t candidate = local + suffix;
      if (candidate < best) {
        best = candidate;
      }
    }
    memo_[index] = best;
    return best;
  }

  std::size_t vertex_count_{};
  Mask full_mask_{};
  std::array<Mask, kExactMinimumFillInMaxVertices> adjacency_{};
  std::vector<std::size_t> memo_;
};

}  // namespace minimum_fill_in_detail

// Exact minimum chordal fill-in through elimination-order subset DP.
//
// Preconditions:
// - graph must be undirected;
// - graph.vertex_count() <= kExactMinimumFillInMaxVertices.
//
// The repository multigraph is projected to its underlying simple graph:
// self-loops and weights are ignored and parallel copies collapse.  The result
// returns the minimum number of added edges, the lexicographically smallest
// optimal elimination order, every added fill edge, and a per-step replay
// witness.  Replaying those edges makes the returned elimination order a perfect
// elimination order of the completed graph.
//
// Direct bounded baseline: O(n^2 * 2^n) bitset-style work and O(2^n + n)
// auxiliary DP state, plus the returned O(n^2) witness in the worst case.
[[nodiscard]] inline ExactMinimumFillInResult exact_minimum_fill_in(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "exact minimum fill-in requires an undirected graph");
  }
  if (graph.vertex_count() > kExactMinimumFillInMaxVertices) {
    throw std::length_error("exact minimum fill-in vertex limit exceeded");
  }
  minimum_fill_in_detail::Solver solver(graph);
  return solver.solve();
}

}  // namespace algorithms::graphs
