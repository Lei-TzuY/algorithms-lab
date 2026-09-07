#include "test_framework.hpp"

#include "algorithms/graphs/bipartite_matching.hpp"
#include "algorithms/graphs/max_flow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::BipartiteEdge;
using algorithms::graphs::BipartiteMatchingResult;
using algorithms::graphs::CapacityEdge;
using algorithms::graphs::dinic_max_flow;
using algorithms::graphs::hopcroft_karp;

bool contains_edge(const std::vector<BipartiteEdge>& edges, std::size_t left,
                   std::size_t right) {
  return std::any_of(edges.begin(), edges.end(), [&](const auto& edge) {
    return edge.left == left && edge.right == right;
  });
}

std::size_t exhaustive_matching_rec(std::size_t left,
                                    std::size_t left_count,
                                    std::size_t right_count,
                                    const std::vector<BipartiteEdge>& edges,
                                    std::vector<bool>& used_right) {
  if (left == left_count) {
    return 0;
  }
  std::size_t best = exhaustive_matching_rec(left + 1, left_count, right_count,
                                             edges, used_right);
  for (const auto& edge : edges) {
    if (edge.left != left || edge.right >= right_count ||
        used_right[edge.right]) {
      continue;
    }
    used_right[edge.right] = true;
    best = std::max(best, std::size_t{1} + exhaustive_matching_rec(
                                             left + 1, left_count, right_count,
                                             edges, used_right));
    used_right[edge.right] = false;
  }
  return best;
}

std::size_t exhaustive_matching(std::size_t left_count,
                                std::size_t right_count,
                                const std::vector<BipartiteEdge>& edges) {
  std::vector<bool> used_right(right_count, false);
  return exhaustive_matching_rec(0, left_count, right_count, edges, used_right);
}

std::size_t matching_via_flow(std::size_t left_count, std::size_t right_count,
                              const std::vector<BipartiteEdge>& edges) {
  const std::size_t source = 0;
  const std::size_t left_begin = 1;
  const std::size_t right_begin = left_begin + left_count;
  const std::size_t sink = right_begin + right_count;
  std::vector<CapacityEdge> network;
  network.reserve(left_count + edges.size() + right_count);
  for (std::size_t left = 0; left < left_count; ++left) {
    network.push_back(CapacityEdge{source, left_begin + left, 1});
  }
  for (const auto& edge : edges) {
    network.push_back(
        CapacityEdge{left_begin + edge.left, right_begin + edge.right, 1});
  }
  for (std::size_t right = 0; right < right_count; ++right) {
    network.push_back(CapacityEdge{right_begin + right, sink, 1});
  }
  const auto flow = dinic_max_flow(sink + 1, network, source, sink);
  REQUIRE(flow.value >= 0);
  return static_cast<std::size_t>(flow.value);
}

void verify_result(std::size_t left_count, std::size_t right_count,
                   const std::vector<BipartiteEdge>& edges,
                   const BipartiteMatchingResult& result) {
  REQUIRE_EQ(result.left_match.size(), left_count);
  REQUIRE_EQ(result.right_match.size(), right_count);
  REQUIRE_EQ(result.left_in_min_vertex_cover.size(), left_count);
  REQUIRE_EQ(result.right_in_min_vertex_cover.size(), right_count);

  std::size_t counted = 0;
  for (std::size_t left = 0; left < left_count; ++left) {
    if (!result.left_match[left].has_value()) {
      continue;
    }
    ++counted;
    const std::size_t right = *result.left_match[left];
    REQUIRE(right < right_count);
    REQUIRE(result.right_match[right].has_value());
    REQUIRE_EQ(*result.right_match[right], left);
    REQUIRE(contains_edge(edges, left, right));
  }
  for (std::size_t right = 0; right < right_count; ++right) {
    if (result.right_match[right].has_value()) {
      const std::size_t left = *result.right_match[right];
      REQUIRE(left < left_count);
      REQUIRE(result.left_match[left].has_value());
      REQUIRE_EQ(*result.left_match[left], right);
    }
  }
  REQUIRE_EQ(counted, result.cardinality);

  std::size_t cover_size = 0;
  for (const bool included : result.left_in_min_vertex_cover) {
    if (included) {
      ++cover_size;
    }
  }
  for (const bool included : result.right_in_min_vertex_cover) {
    if (included) {
      ++cover_size;
    }
  }
  REQUIRE_EQ(cover_size, result.cardinality);
  for (const auto& edge : edges) {
    REQUIRE(result.left_in_min_vertex_cover[edge.left] ||
            result.right_in_min_vertex_cover[edge.right]);
  }
}

TEST_CASE(bipartite_matching_empty_and_reassignment) {
  const std::vector<BipartiteEdge> empty;
  const auto no_vertices = hopcroft_karp(0, 0, empty);
  REQUIRE_EQ(no_vertices.cardinality, std::size_t{0});

  const std::vector<BipartiteEdge> edges{{0, 0}, {0, 1}, {1, 0}};
  const auto result = hopcroft_karp(2, 2, edges);
  REQUIRE_EQ(result.cardinality, std::size_t{2});
  verify_result(2, 2, edges, result);
}

TEST_CASE(bipartite_matching_parallel_partial_and_invalid) {
  const std::vector<BipartiteEdge> edges{{0, 0}, {0, 0}, {1, 0},
                                         {1, 1}, {3, 2}};
  const auto result = hopcroft_karp(4, 3, edges);
  REQUIRE_EQ(result.cardinality, std::size_t{3});
  verify_result(4, 3, edges, result);

  const std::vector<BipartiteEdge> bad_left{{2, 0}};
  REQUIRE_THROWS_AS(hopcroft_karp(2, 1, bad_left), std::out_of_range);
  const std::vector<BipartiteEdge> bad_right{{0, 2}};
  REQUIRE_THROWS_AS(hopcroft_karp(1, 2, bad_right), std::out_of_range);
}

TEST_CASE(bipartite_matching_randomized_exhaustive_and_flow_integration) {
  std::mt19937_64 rng(0xB1A47EULL);
  for (std::size_t trial = 0; trial < 400; ++trial) {
    const std::size_t left_count = static_cast<std::size_t>(rng() % 7U);
    const std::size_t right_count = static_cast<std::size_t>(rng() % 7U);
    std::vector<BipartiteEdge> edges;
    if (left_count != 0 && right_count != 0) {
      const std::size_t edge_count = static_cast<std::size_t>(rng() % 25U);
      edges.reserve(edge_count);
      for (std::size_t edge = 0; edge < edge_count; ++edge) {
        edges.push_back(BipartiteEdge{
            static_cast<std::size_t>(rng() % left_count),
            static_cast<std::size_t>(rng() % right_count)});
      }
    }

    const auto result = hopcroft_karp(left_count, right_count, edges);
    REQUIRE_EQ(result.cardinality,
               exhaustive_matching(left_count, right_count, edges));
    REQUIRE_EQ(result.cardinality,
               matching_via_flow(left_count, right_count, edges));
    verify_result(left_count, right_count, edges, result);
  }
}

}  // namespace
