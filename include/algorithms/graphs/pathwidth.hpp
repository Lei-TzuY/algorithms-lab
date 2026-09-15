#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

struct ExactPathwidthResult {
  std::size_t pathwidth{};
  std::vector<Vertex> ordering;
  std::vector<std::size_t> prefix_boundary_sizes;
  std::vector<std::vector<Vertex>> bags;
};

namespace detail {

inline std::size_t pathwidth_boundary_size(
    std::uint64_t prefix, const std::vector<std::uint64_t>& adjacency,
    std::uint64_t full_mask) {
  const std::uint64_t suffix = full_mask ^ prefix;
  std::size_t count = 0;
  std::uint64_t remaining = prefix;
  while (remaining != 0) {
    const std::size_t bit = static_cast<std::size_t>(std::countr_zero(remaining));
    remaining &= remaining - 1;
    if ((adjacency[bit] & suffix) != 0) {
      ++count;
    }
  }
  return count;
}

}  // namespace detail

// Exact bounded pathwidth via the vertex-separation characterization.
//
// Semantics:
// - input must be undirected;
// - self-loops and weights are ignored;
// - parallel copies collapse to one structural adjacency relation;
// - at most 20 vertices are accepted by this exact-exponential baseline;
// - the empty graph has pathwidth 0 by explicit API convention.
//
// For a prefix S of an ordering, boundary(S) is the number of vertices in S
// with a neighbor outside S. Kinnersley's characterization states that the
// minimum possible maximum boundary size is graph pathwidth. The suffix DP
// below stores the optimum completion value for every prefix set, permitting
// lexicographically smallest optimum reconstruction by scanning next vertices
// in ascending order.
[[nodiscard]] inline ExactPathwidthResult exact_pathwidth(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("pathwidth requires an undirected graph");
  }

  const std::size_t n = graph.vertex_count();
  constexpr std::size_t kMaxExactVertices = 20;
  if (n > kMaxExactVertices) {
    throw std::length_error("exact pathwidth supports at most 20 vertices");
  }

  if (n == 0) {
    return ExactPathwidthResult{0, {}, {0}, {}};
  }

  std::vector<std::uint64_t> adjacency(n, 0);
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to != from) {
        adjacency[from] |= std::uint64_t{1} << edge.to;
      }
    }
  }

  const std::uint64_t state_count = std::uint64_t{1} << n;
  const std::uint64_t full_mask = state_count - 1;
  std::vector<std::uint8_t> boundary(state_count, 0);
  for (std::uint64_t mask = 0; mask < state_count; ++mask) {
    boundary[mask] = static_cast<std::uint8_t>(
        detail::pathwidth_boundary_size(mask, adjacency, full_mask));
  }

  // completion[mask] is the minimum possible maximum boundary among the
  // current prefix `mask` and every later prefix in a completion to V.
  std::vector<std::uint8_t> completion(state_count, 0);
  completion[full_mask] = boundary[full_mask];
  for (std::uint64_t mask = full_mask; mask-- > 0;) {
    std::uint8_t best_future = std::numeric_limits<std::uint8_t>::max();
    const std::uint64_t remaining = full_mask ^ mask;
    std::uint64_t choices = remaining;
    while (choices != 0) {
      const std::size_t bit = static_cast<std::size_t>(std::countr_zero(choices));
      choices &= choices - 1;
      const std::uint64_t next = mask | (std::uint64_t{1} << bit);
      best_future = std::min(best_future, completion[next]);
    }
    completion[mask] = std::max(boundary[mask], best_future);
  }

  ExactPathwidthResult result;
  result.pathwidth = completion[0];
  result.ordering.reserve(n);
  result.prefix_boundary_sizes.reserve(n + 1);
  result.bags.reserve(n);

  std::uint64_t prefix = 0;
  result.prefix_boundary_sizes.push_back(boundary[prefix]);
  while (prefix != full_mask) {
    bool found = false;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      const std::uint64_t bit = std::uint64_t{1} << vertex;
      if ((prefix & bit) != 0) {
        continue;
      }
      const std::uint64_t next = prefix | bit;
      if (completion[next] > result.pathwidth) {
        continue;
      }

      std::vector<Vertex> bag;
      for (Vertex prior = 0; prior < n; ++prior) {
        const std::uint64_t prior_bit = std::uint64_t{1} << prior;
        if ((prefix & prior_bit) != 0 &&
            (adjacency[prior] & (full_mask ^ prefix)) != 0) {
          bag.push_back(prior);
        }
      }
      bag.push_back(vertex);
      std::sort(bag.begin(), bag.end());
      result.bags.push_back(std::move(bag));
      result.ordering.push_back(vertex);
      prefix = next;
      result.prefix_boundary_sizes.push_back(boundary[prefix]);
      found = true;
      break;
    }
    if (!found) {
      throw std::logic_error("pathwidth reconstruction invariant failed");
    }
  }

  return result;
}

}  // namespace algorithms::graphs
