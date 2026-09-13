#pragma once

#include "algorithms/graphs/densest_subgraph.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace densest_subgraph_test_detail {
using algorithms::graphs::DensestSubgraphResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct LogicalEdge { Vertex first; Vertex second; };
struct Ratio { std::size_t edges{}; std::size_t vertices{1U}; };

inline std::vector<LogicalEdge> logical_edges(const Graph& graph) {
  std::vector<LogicalEdge> edges;
  for (Vertex first = 0; first < graph.vertex_count(); ++first) {
    for (const auto& edge : graph.neighbors(first)) {
      if (first < edge.to) edges.push_back(LogicalEdge{first, edge.to});
    }
  }
  return edges;
}

inline std::size_t internal_edges(const std::vector<LogicalEdge>& edges,
                                  const std::vector<bool>& selected) {
  std::size_t count = 0;
  for (const auto& edge : edges) {
    if (selected[edge.first] && selected[edge.second]) ++count;
  }
  return count;
}

inline bool ratio_less(const Ratio& left, const Ratio& right) {
  return left.edges * right.vertices < right.edges * left.vertices;
}

inline Ratio exhaustive_optimum(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n == 0) return Ratio{};
  const auto edges = logical_edges(graph);
  Ratio best{};
  const std::uint64_t limit = std::uint64_t{1} << n;
  for (std::uint64_t mask = 1; mask < limit; ++mask) {
    std::vector<bool> selected(n, false);
    std::size_t vertices = 0;
    for (std::size_t index = 0; index < n; ++index) {
      if (((mask >> index) & 1U) != 0U) {
        selected[index] = true;
        ++vertices;
      }
    }
    const Ratio candidate{internal_edges(edges, selected), vertices};
    if (ratio_less(best, candidate)) best = candidate;
  }
  return best;
}

inline void verify_exact(const Graph& graph, const DensestSubgraphResult& result) {
  if (graph.vertex_count() == 0) {
    REQUIRE(result.vertices.empty());
    REQUIRE_EQ(result.internal_edge_count, std::size_t{0});
    return;
  }
  REQUIRE(!result.vertices.empty());
  REQUIRE(std::is_sorted(result.vertices.begin(), result.vertices.end()));
  std::vector<bool> selected(graph.vertex_count(), false);
  for (const Vertex vertex : result.vertices) {
    REQUIRE(vertex < graph.vertex_count());
    REQUIRE(!selected[vertex]);
    selected[vertex] = true;
  }
  const auto edges = logical_edges(graph);
  const std::size_t replayed = internal_edges(edges, selected);
  REQUIRE_EQ(result.internal_edge_count, replayed);
  const Ratio actual{replayed, result.vertices.size()};
  const Ratio expected = exhaustive_optimum(graph);
  REQUIRE(!ratio_less(actual, expected));
  REQUIRE(!ratio_less(expected, actual));
}
}  // namespace densest_subgraph_test_detail

TEST_CASE(densest_subgraph_handles_empty_edgeless_and_dense_components) {
  using namespace densest_subgraph_test_detail;
  Graph empty(0, false);
  verify_exact(empty, algorithms::graphs::densest_subgraph(empty));

  Graph edgeless(5, false);
  const auto singleton = algorithms::graphs::densest_subgraph(edgeless);
  REQUIRE_EQ(singleton.vertices, std::vector<Vertex>{0});
  REQUIRE_EQ(singleton.internal_edge_count, std::size_t{0});

  Graph graph(6, false);
  for (Vertex first = 0; first < 4; ++first) {
    for (Vertex second = first + 1; second < 4; ++second) graph.add_edge(first, second);
  }
  graph.add_edge(4, 5);
  const auto result = algorithms::graphs::densest_subgraph(graph);
  verify_exact(graph, result);
  REQUIRE_EQ(result.vertices, (std::vector<Vertex>{0, 1, 2, 3}));
}

TEST_CASE(densest_subgraph_counts_parallel_edges_and_ignores_weights) {
  using namespace densest_subgraph_test_detail;
  Graph graph(3, false);
  graph.add_edge(0, 1, 99);
  graph.add_edge(0, 1, -7);
  graph.add_edge(1, 2, 123);
  const auto first = algorithms::graphs::densest_subgraph(graph);
  const auto second = algorithms::graphs::densest_subgraph(graph);
  verify_exact(graph, first);
  REQUIRE_EQ(first, second);
}

TEST_CASE(densest_subgraph_rejects_directed_graphs_and_self_loops) {
  using densest_subgraph_test_detail::Graph;
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::densest_subgraph(directed), std::invalid_argument);

  Graph looped(2, false);
  looped.add_edge(0, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::densest_subgraph(looped), std::invalid_argument);
}

TEST_CASE(densest_subgraph_randomized_differential_against_subset_oracle) {
  using namespace densest_subgraph_test_detail;
  std::mt19937_64 random(0xD3A53E57ULL);
  for (std::size_t trial = 0; trial < 400; ++trial) {
    const std::size_t n = static_cast<std::size_t>(random() % 9U);
    Graph graph(n, false);
    for (Vertex first = 0; first < n; ++first) {
      for (Vertex second = first + 1; second < n; ++second) {
        const unsigned copies = static_cast<unsigned>(random() % 3U);
        for (unsigned copy = 0; copy < copies; ++copy) {
          const auto weight = static_cast<std::int64_t>(random() % 201U) - 100;
          graph.add_edge(first, second, weight);
        }
      }
    }
    verify_exact(graph, algorithms::graphs::densest_subgraph(graph));
  }
}
