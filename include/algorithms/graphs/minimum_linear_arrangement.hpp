#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct MinimumLinearArrangementResult {
  std::size_t cost{};
  std::vector<std::size_t> ordering;

  friend bool operator==(const MinimumLinearArrangementResult&,
                         const MinimumLinearArrangementResult&) = default;
};

// Exact minimum linear arrangement for a simple undirected graph.
//
// For an ordering pi, the objective is
//   sum_{ {u,v} in E } |position(u) - position(v)|.
//
// The implementation uses the prefix-cut identity and subset dynamic
// programming. It is intentionally bounded to at most 22 vertices so the
// exponential state space remains explicit and verifiable.
//
// Among all minimum-cost arrangements, the lexicographically smallest ordering
// is returned.
//
// Throws:
//   * std::length_error when vertex_count > 22;
//   * std::invalid_argument for an out-of-range endpoint, self-loop, or
//     duplicate undirected edge.
[[nodiscard]] inline MinimumLinearArrangementResult
minimum_linear_arrangement(
    const std::size_t vertex_count,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges) {
  constexpr std::size_t kMaximumVertices = 22U;
  if (vertex_count > kMaximumVertices) {
    throw std::length_error(
        "exact minimum linear arrangement supports at most 22 vertices");
  }

  std::vector<std::uint32_t> adjacency(vertex_count, 0U);
  std::vector<std::size_t> degree(vertex_count, 0U);

  for (const auto& [first, second] : edges) {
    if (first >= vertex_count || second >= vertex_count) {
      throw std::invalid_argument(
          "minimum linear arrangement edge endpoint out of range");
    }
    if (first == second) {
      throw std::invalid_argument(
          "minimum linear arrangement requires a simple graph");
    }

    const std::uint32_t second_bit =
        std::uint32_t{1} << static_cast<unsigned>(second);
    if ((adjacency[first] & second_bit) != 0U) {
      throw std::invalid_argument(
          "minimum linear arrangement duplicate edge");
    }

    const std::uint32_t first_bit =
        std::uint32_t{1} << static_cast<unsigned>(first);
    adjacency[first] |= second_bit;
    adjacency[second] |= first_bit;
    ++degree[first];
    ++degree[second];
  }

  const std::size_t state_count =
      std::size_t{1} << static_cast<unsigned>(vertex_count);
  const std::size_t full_mask = state_count - 1U;

  // cut[mask] is the number of graph edges with exactly one endpoint in mask.
  std::vector<std::size_t> cut(state_count, 0U);
  for (std::size_t mask = 1U; mask < state_count; ++mask) {
    const std::size_t rest = mask & (mask - 1U);
    const unsigned vertex = static_cast<unsigned>(
        std::countr_zero(static_cast<std::uint64_t>(mask)));
    const std::size_t inside_neighbors = static_cast<std::size_t>(
        std::popcount(
            adjacency[vertex] & static_cast<std::uint32_t>(rest)));

    const std::size_t before_removal = cut[rest] + degree[vertex];
    const std::size_t twice_inside = 2U * inside_neighbors;
    if (before_removal < twice_inside) {
      throw std::logic_error(
          "minimum linear arrangement cut recurrence invariant violated");
    }
    cut[mask] = before_removal - twice_inside;
  }

  // dp[remaining] is the minimum future prefix-cut sum when exactly the
  // vertices in 'remaining' have not yet been placed.
  //
  // Choosing v next leaves 'rest'. The newly formed placed prefix is the
  // complement of rest, whose cut equals cut[rest] by cut symmetry.
  std::vector<std::size_t> dp(state_count, 0U);
  std::vector<std::uint8_t> first_choice(
      state_count, std::numeric_limits<std::uint8_t>::max());

  for (std::size_t mask = 1U; mask < state_count; ++mask) {
    std::size_t best = std::numeric_limits<std::size_t>::max();
    std::uint8_t choice = std::numeric_limits<std::uint8_t>::max();

    for (std::size_t vertex = 0U; vertex < vertex_count; ++vertex) {
      const std::size_t bit =
          std::size_t{1} << static_cast<unsigned>(vertex);
      if ((mask & bit) == 0U) {
        continue;
      }

      const std::size_t rest = mask ^ bit;
      if (dp[rest] >
          std::numeric_limits<std::size_t>::max() - cut[rest]) {
        throw std::length_error(
            "minimum linear arrangement cost overflows size_t");
      }
      const std::size_t candidate = dp[rest] + cut[rest];

      // Vertices are considered in increasing order, so retaining the first
      // equal-cost choice yields the lexicographically smallest optimal suffix.
      if (candidate < best) {
        best = candidate;
        choice = static_cast<std::uint8_t>(vertex);
      }
    }

    if (choice == std::numeric_limits<std::uint8_t>::max()) {
      throw std::logic_error(
          "minimum linear arrangement DP has no transition");
    }
    dp[mask] = best;
    first_choice[mask] = choice;
  }

  std::vector<std::size_t> ordering;
  ordering.reserve(vertex_count);
  std::size_t remaining = full_mask;
  while (remaining != 0U) {
    const std::uint8_t choice = first_choice[remaining];
    if (choice == std::numeric_limits<std::uint8_t>::max() ||
        static_cast<std::size_t>(choice) >= vertex_count) {
      throw std::logic_error(
          "minimum linear arrangement reconstruction invariant violated");
    }

    const std::size_t vertex = static_cast<std::size_t>(choice);
    ordering.push_back(vertex);
    remaining ^=
        std::size_t{1} << static_cast<unsigned>(vertex);
  }

  return MinimumLinearArrangementResult{dp[full_mask], std::move(ordering)};
}

}  // namespace algorithms::graphs
