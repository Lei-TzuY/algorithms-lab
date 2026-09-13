#include "test_framework.hpp"

#include "algorithms/graphs/bipartite_matching.hpp"
#include "algorithms/graphs/max_flow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::BipartiteEdge;
using algorithms::graphs::BipartiteMatchingResult;
using algorithms::graphs::BipartiteEdgeColoringResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::minimum_bipartite_edge_coloring;
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

struct EdgeColorOracleEdge {
  Vertex first;
  Vertex second;
  std::int64_t weight;
};

[[nodiscard]] std::vector<EdgeColorOracleEdge> edge_coloring_logical_edges(
    const Graph& graph) {
  std::vector<EdgeColorOracleEdge> edges;
  for (Vertex first = 0; first < graph.vertex_count(); ++first) {
    for (const auto& edge : graph.neighbors(first)) {
      if (first < edge.to) {
        edges.push_back(EdgeColorOracleEdge{first, edge.to, edge.weight});
      }
    }
  }
  return edges;
}

[[nodiscard]] std::size_t edge_coloring_maximum_degree(const Graph& graph) {
  std::vector<std::size_t> degree(graph.vertex_count(), 0U);
  std::size_t maximum = 0;
  for (const auto& edge : edge_coloring_logical_edges(graph)) {
    maximum = std::max(maximum, ++degree[edge.first]);
    maximum = std::max(maximum, ++degree[edge.second]);
  }
  return maximum;
}

[[nodiscard]] bool edge_coloring_can_use(
    std::span<const EdgeColorOracleEdge> edges, std::size_t vertex_count,
    std::size_t color_count) {
  if (edges.empty()) {
    return true;
  }
  if (color_count == 0U) {
    return false;
  }
  std::vector<std::vector<bool>> used(
      vertex_count, std::vector<bool>(color_count, false));
  auto search = [&](auto&& self, std::size_t index) -> bool {
    if (index == edges.size()) {
      return true;
    }
    const auto& edge = edges[index];
    for (std::size_t color = 0; color < color_count; ++color) {
      if (used[edge.first][color] || used[edge.second][color]) {
        continue;
      }
      used[edge.first][color] = true;
      used[edge.second][color] = true;
      if (self(self, index + 1U)) {
        return true;
      }
      used[edge.first][color] = false;
      used[edge.second][color] = false;
    }
    return false;
  };
  return search(search, 0U);
}

void verify_edge_coloring(const Graph& graph,
                          const BipartiteEdgeColoringResult& result) {
  const auto oracle_edges = edge_coloring_logical_edges(graph);
  REQUIRE_EQ(result.left_partition.size(), graph.vertex_count());
  REQUIRE_EQ(result.edges.size(), oracle_edges.size());
  REQUIRE_EQ(result.color_count, edge_coloring_maximum_degree(graph));

  std::vector<std::set<std::size_t>> incident_colors(graph.vertex_count());
  for (std::size_t edge_id = 0; edge_id < result.edges.size(); ++edge_id) {
    const auto& actual = result.edges[edge_id];
    const auto& expected = oracle_edges[edge_id];
    REQUIRE_EQ(actual.edge_id, edge_id);
    REQUIRE_EQ(actual.first, expected.first);
    REQUIRE_EQ(actual.second, expected.second);
    REQUIRE_EQ(actual.weight, expected.weight);
    REQUIRE(actual.color < result.color_count);
    REQUIRE(incident_colors[actual.first].insert(actual.color).second);
    REQUIRE(incident_colors[actual.second].insert(actual.color).second);
    REQUIRE(result.left_partition[actual.first] !=
            result.left_partition[actual.second]);
  }
}

TEST_CASE(bipartite_edge_coloring_empty_and_parallel_edges) {
  Graph empty(5, false);
  const auto empty_result = minimum_bipartite_edge_coloring(empty);
  REQUIRE_EQ(empty_result.color_count, std::size_t{0});
  REQUIRE(empty_result.edges.empty());

  Graph graph(2, false);
  graph.add_edge(0, 1, -9);
  graph.add_edge(0, 1, 0);
  graph.add_edge(0, 1, 42);
  const auto result = minimum_bipartite_edge_coloring(graph);
  REQUIRE_EQ(result.color_count, std::size_t{3});
  REQUIRE_EQ(result.edges.size(), std::size_t{3});
  REQUIRE_EQ(result.edges[0].weight, std::int64_t{-9});
  REQUIRE_EQ(result.edges[1].weight, std::int64_t{0});
  REQUIRE_EQ(result.edges[2].weight, std::int64_t{42});
  verify_edge_coloring(graph, result);
}

TEST_CASE(bipartite_edge_coloring_complete_disconnected_and_deterministic) {
  Graph graph(9, false);
  for (Vertex left = 0; left < 3; ++left) {
    for (Vertex right = 3; right < 6; ++right) {
      graph.add_edge(left, right);
    }
  }
  graph.add_edge(6, 7, 3);
  graph.add_edge(6, 8, -4);
  const auto first = minimum_bipartite_edge_coloring(graph);
  const auto second = minimum_bipartite_edge_coloring(graph);
  REQUIRE(first == second);
  REQUIRE_EQ(first.color_count, std::size_t{3});
  verify_edge_coloring(graph, first);
}

TEST_CASE(bipartite_edge_coloring_rejects_non_bipartite_domains) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(minimum_bipartite_edge_coloring(directed),
                    std::invalid_argument);

  Graph self_loop(1, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(minimum_bipartite_edge_coloring(self_loop),
                    std::invalid_argument);

  Graph triangle(3, false);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  REQUIRE_THROWS_AS(minimum_bipartite_edge_coloring(triangle),
                    std::invalid_argument);
}

TEST_CASE(bipartite_edge_coloring_randomized_exhaustive_optimality) {
  std::mt19937_64 rng(0xEC010A5ULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t left_count = static_cast<std::size_t>(rng() % 4U);
    const std::size_t right_count = static_cast<std::size_t>(rng() % 4U);
    Graph graph(left_count + right_count, false);

    std::size_t edge_budget = 8U;
    for (std::size_t left = 0; left < left_count && edge_budget != 0U;
         ++left) {
      for (std::size_t right = 0;
           right < right_count && edge_budget != 0U; ++right) {
        const std::size_t copies =
            std::min<std::size_t>(static_cast<std::size_t>(rng() % 3U),
                                  edge_budget);
        for (std::size_t copy = 0; copy < copies; ++copy) {
          const auto weight = static_cast<std::int64_t>(rng() % 201U) - 100;
          graph.add_edge(left, left_count + right, weight);
        }
        edge_budget -= copies;
      }
    }

    const auto result = minimum_bipartite_edge_coloring(graph);
    verify_edge_coloring(graph, result);
    REQUIRE(result == minimum_bipartite_edge_coloring(graph));

    const auto edges = edge_coloring_logical_edges(graph);
    const std::size_t delta = edge_coloring_maximum_degree(graph);
    if (delta != 0U) {
      REQUIRE(!edge_coloring_can_use(edges, graph.vertex_count(), delta - 1U));
      REQUIRE(edge_coloring_can_use(edges, graph.vertex_count(), delta));
    }
  }
}

}  // namespace
