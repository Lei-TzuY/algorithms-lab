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

struct TournamentFeedbackArcSetResult {
  std::size_t feedback_arc_count{};
  std::vector<Vertex> ordering;
  // Each pair (from, to) is an original tournament arc that points backward
  // in `ordering`: `from` occurs after `to`.
  std::vector<std::pair<Vertex, Vertex>> feedback_arcs;
};

namespace tournament_feedback_arc_detail {

constexpr std::size_t kMaximumExactVertices = 20;

struct TournamentBits {
  std::vector<std::uint64_t> incoming;
};

inline TournamentBits validate_and_encode(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument(
        "tournament feedback-arc solver requires a directed graph");
  }

  const std::size_t n = graph.vertex_count();
  if (n > kMaximumExactVertices) {
    throw std::length_error(
        "exact tournament feedback-arc solver supports at most 20 vertices");
  }

  std::vector<std::vector<unsigned char>> multiplicity(
      n, std::vector<unsigned char>(n, 0));
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        throw std::invalid_argument("a tournament cannot contain self-loops");
      }
      if (multiplicity[from][edge.to] ==
          std::numeric_limits<unsigned char>::max()) {
        throw std::invalid_argument("tournament edge multiplicity overflow");
      }
      ++multiplicity[from][edge.to];
      if (multiplicity[from][edge.to] > 1) {
        throw std::invalid_argument("a tournament cannot contain parallel arcs");
      }
    }
  }

  TournamentBits bits{std::vector<std::uint64_t>(n, 0)};
  for (Vertex u = 0; u < n; ++u) {
    for (Vertex v = u + 1; v < n; ++v) {
      const unsigned int uv = multiplicity[u][v];
      const unsigned int vu = multiplicity[v][u];
      if (uv + vu != 1U) {
        throw std::invalid_argument(
            "every unordered vertex pair must have exactly one directed arc");
      }
      if (uv == 1U) {
        bits.incoming[v] |= (std::uint64_t{1} << u);
      } else {
        bits.incoming[u] |= (std::uint64_t{1} << v);
      }
    }
  }
  return bits;
}

}  // namespace tournament_feedback_arc_detail

// Exact minimum feedback-arc set / minimum-upset ordering for a simple directed
// tournament. Edge weights are intentionally ignored. This is an exponential
// educational baseline: O(n 2^n + n^2) time, O(2^n + n^2) storage, n <= 20.
[[nodiscard]] inline TournamentFeedbackArcSetResult
minimum_feedback_arc_set_tournament(const Graph& tournament) {
  using tournament_feedback_arc_detail::validate_and_encode;

  const auto bits = validate_and_encode(tournament);
  const std::size_t n = tournament.vertex_count();
  if (n == 0) {
    return {};
  }

  const std::uint64_t state_count_u64 = std::uint64_t{1} << n;
  const std::size_t state_count = static_cast<std::size_t>(state_count_u64);
  const std::size_t inf = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> dp(state_count, inf);
  std::vector<Vertex> first(state_count, n);
  dp[0] = 0;

  for (std::size_t mask = 1; mask < state_count; ++mask) {
    const std::uint64_t mask_bits = static_cast<std::uint64_t>(mask);
    for (Vertex candidate = 0; candidate < n; ++candidate) {
      const std::uint64_t candidate_bit = std::uint64_t{1} << candidate;
      if ((mask_bits & candidate_bit) == 0) {
        continue;
      }
      const std::uint64_t rest_bits = mask_bits ^ candidate_bit;
      const std::size_t rest = static_cast<std::size_t>(rest_bits);
      const std::size_t added = static_cast<std::size_t>(
          std::popcount(bits.incoming[candidate] & rest_bits));
      const std::size_t value = dp[rest] + added;
      if (value < dp[mask] ||
          (value == dp[mask] && candidate < first[mask])) {
        dp[mask] = value;
        first[mask] = candidate;
      }
    }
  }

  TournamentFeedbackArcSetResult result;
  result.feedback_arc_count = dp[state_count - 1];
  result.ordering.reserve(n);
  std::size_t mask = state_count - 1;
  while (mask != 0) {
    const Vertex vertex = first[mask];
    result.ordering.push_back(vertex);
    mask ^= static_cast<std::size_t>(std::uint64_t{1} << vertex);
  }

  std::vector<std::size_t> position(n, 0);
  for (std::size_t index = 0; index < n; ++index) {
    position[result.ordering[index]] = index;
  }
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : tournament.neighbors(from)) {
      if (position[from] > position[edge.to]) {
        result.feedback_arcs.emplace_back(from, edge.to);
      }
    }
  }
  if (result.feedback_arcs.size() != result.feedback_arc_count) {
    throw std::logic_error(
        "tournament feedback-arc reconstruction invariant failed");
  }
  return result;
}

}  // namespace algorithms::graphs
