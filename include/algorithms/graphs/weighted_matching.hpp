#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

struct WeightedMatchingEdge {
  Vertex first{0U};
  Vertex second{0U};
  std::int64_t weight{0};
  friend bool operator==(const WeightedMatchingEdge&,
                         const WeightedMatchingEdge&) = default;
};

struct BoundedWeightedMatchingResult {
  std::int64_t total_weight{0};
  std::vector<std::optional<Vertex>> mate;
  std::vector<std::size_t> edge_indices;
};

inline constexpr std::size_t kBoundedWeightedMatchingMaxVertices = 20U;

namespace weighted_matching_detail {

struct PairChoice {
  std::int64_t weight{0};
  std::size_t edge_index{0U};
};

enum class CellKind : unsigned char { unseen, finite, positive_overflow };

struct Cell {
  std::int64_t value{0};
  CellKind kind{CellKind::unseen};
};

[[nodiscard]] inline std::size_t first_set_vertex(
    const std::uint64_t mask, const std::size_t vertex_count) {
  for (std::size_t vertex = 0U; vertex < vertex_count; ++vertex) {
    if ((mask & (std::uint64_t{1} << vertex)) != 0U) {
      return vertex;
    }
  }
  throw std::logic_error("weighted matching state unexpectedly has no vertex");
}

class Solver {
 public:
  Solver(const std::size_t vertex_count,
         const std::span<const WeightedMatchingEdge> edges)
      : vertex_count_(vertex_count),
        pair_choices_(vertex_count * vertex_count),
        cells_(std::size_t{1} << vertex_count) {
    for (std::size_t edge_index = 0U; edge_index < edges.size(); ++edge_index) {
      const WeightedMatchingEdge& edge = edges[edge_index];
      if (edge.first >= vertex_count_ || edge.second >= vertex_count_) {
        throw std::out_of_range("weighted matching edge endpoint out of range");
      }
      if (edge.first == edge.second) {
        continue;
      }
      const std::size_t low =
          edge.first < edge.second ? edge.first : edge.second;
      const std::size_t high =
          edge.first < edge.second ? edge.second : edge.first;
      std::optional<PairChoice>& choice =
          pair_choices_[low * vertex_count_ + high];
      if (!choice.has_value() || edge.weight > choice->weight ||
          (edge.weight == choice->weight &&
           edge_index < choice->edge_index)) {
        choice = PairChoice{edge.weight, edge_index};
      }
    }
    cells_[0U] = Cell{0, CellKind::finite};
  }

  [[nodiscard]] BoundedWeightedMatchingResult run() {
    const std::uint64_t full_mask =
        vertex_count_ == 0U
            ? std::uint64_t{0}
            : ((std::uint64_t{1} << vertex_count_) - std::uint64_t{1});
    const Cell optimum = solve(full_mask);
    if (optimum.kind == CellKind::positive_overflow) {
      throw std::overflow_error(
          "weighted matching optimum is outside int64 range");
    }

    BoundedWeightedMatchingResult result;
    result.total_weight = optimum.value;
    result.mate.resize(vertex_count_);

    std::uint64_t mask = full_mask;
    while (mask != 0U) {
      const std::size_t first = first_set_vertex(mask, vertex_count_);
      const std::uint64_t first_bit = std::uint64_t{1} << first;
      const std::uint64_t without_first = mask & ~first_bit;
      const Cell unmatched = solve(without_first);
      const Cell current = solve(mask);
      if (unmatched.kind == CellKind::finite &&
          unmatched.value == current.value) {
        mask = without_first;
        continue;
      }

      bool reconstructed = false;
      for (std::size_t second = first + 1U; second < vertex_count_; ++second) {
        const std::uint64_t second_bit = std::uint64_t{1} << second;
        if ((without_first & second_bit) == 0U) {
          continue;
        }
        const std::optional<PairChoice>& choice = pair_choice(first, second);
        if (!choice.has_value()) {
          continue;
        }
        const Cell rest = solve(without_first & ~second_bit);
        if (rest.kind != CellKind::finite) {
          continue;
        }
        const std::optional<std::int64_t> candidate =
            finite_sum(rest.value, choice->weight);
        if (candidate.has_value() && *candidate == current.value) {
          result.mate[first] = second;
          result.mate[second] = first;
          result.edge_indices.push_back(choice->edge_index);
          mask = without_first & ~second_bit;
          reconstructed = true;
          break;
        }
      }
      if (!reconstructed) {
        throw std::logic_error("weighted matching optimum reconstruction failed");
      }
    }
    return result;
  }

 private:
  [[nodiscard]] static std::optional<std::int64_t> finite_sum(
      const std::int64_t nonnegative, const std::int64_t delta) {
    if (nonnegative < 0) {
      throw std::logic_error("weighted matching DP lost nonnegative invariant");
    }
    if (delta > 0 &&
        nonnegative > std::numeric_limits<std::int64_t>::max() - delta) {
      return std::nullopt;
    }
    return nonnegative + delta;
  }

  [[nodiscard]] const std::optional<PairChoice>& pair_choice(
      const std::size_t first, const std::size_t second) const {
    const std::size_t low = first < second ? first : second;
    const std::size_t high = first < second ? second : first;
    return pair_choices_[low * vertex_count_ + high];
  }

  [[nodiscard]] Cell solve(const std::uint64_t mask) {
    Cell& cell = cells_[static_cast<std::size_t>(mask)];
    if (cell.kind != CellKind::unseen) {
      return cell;
    }

    const std::size_t first = first_set_vertex(mask, vertex_count_);
    const std::uint64_t first_bit = std::uint64_t{1} << first;
    const std::uint64_t without_first = mask & ~first_bit;
    const Cell unmatched = solve(without_first);
    if (unmatched.kind == CellKind::positive_overflow) {
      cell = unmatched;
      return cell;
    }

    cell = unmatched;
    for (std::size_t second = first + 1U; second < vertex_count_; ++second) {
      const std::uint64_t second_bit = std::uint64_t{1} << second;
      if ((without_first & second_bit) == 0U) {
        continue;
      }
      const std::optional<PairChoice>& choice = pair_choice(first, second);
      if (!choice.has_value()) {
        continue;
      }
      const Cell rest = solve(without_first & ~second_bit);
      if (rest.kind == CellKind::positive_overflow) {
        cell = rest;
        return cell;
      }
      const std::optional<std::int64_t> candidate =
          finite_sum(rest.value, choice->weight);
      if (!candidate.has_value()) {
        cell = Cell{0, CellKind::positive_overflow};
        return cell;
      }
      if (*candidate > cell.value) {
        cell.value = *candidate;
      }
    }
    return cell;
  }

  std::size_t vertex_count_;
  std::vector<std::optional<PairChoice>> pair_choices_;
  std::vector<Cell> cells_;
};

}  // namespace weighted_matching_detail

// Exact maximum-weight matching for a deliberately bounded general graph.
// Vertices may remain unmatched, so the empty matching is always feasible.
// Self-loops are ignored. Parallel edges retain input identity; for equal-weight
// parallel choices, the smallest input index is preferred. Endpoint validation
// is strict. The implementation rejects vertex_count > 20 because its subset
// dynamic program is exponential rather than a polynomial weighted-blossom
// algorithm.
//
// The result is deterministic. Ties first prefer leaving the lowest available
// vertex unmatched, then the lowest partner vertex. If the exact optimum is
// greater than INT64_MAX, std::overflow_error is thrown.
[[nodiscard]] inline BoundedWeightedMatchingResult
bounded_exact_maximum_weight_matching(
    const std::size_t vertex_count,
    const std::span<const WeightedMatchingEdge> edges) {
  if (vertex_count > kBoundedWeightedMatchingMaxVertices) {
    throw std::length_error("weighted matching vertex bound exceeded");
  }
  return weighted_matching_detail::Solver(vertex_count, edges).run();
}

}  // namespace algorithms::graphs
