#include "algorithms/graphs/general_matching.hpp"

#include "algorithms/graphs/bipartite_matching.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::graphs::BipartiteEdge;
using algorithms::graphs::GeneralMatchingResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::edmonds_blossom_maximum_matching;
using algorithms::graphs::hopcroft_karp;

std::vector<std::vector<bool>> edge_matrix(const Graph& graph) {
  const std::size_t vertex_count = graph.vertex_count();
  std::vector<std::vector<bool>> matrix(
      vertex_count, std::vector<bool>(vertex_count, false));
  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        matrix[from][edge.to] = true;
      }
    }
  }
  return matrix;
}

std::size_t exhaustive_from(const std::vector<std::vector<bool>>& edges,
                            std::vector<bool>& used) {
  const std::size_t vertex_count = used.size();
  Vertex first = vertex_count;
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (!used[vertex]) {
      first = vertex;
      break;
    }
  }
  if (first == vertex_count) {
    return 0;
  }

  used[first] = true;
  std::size_t best = exhaustive_from(edges, used);
  for (Vertex second = first + 1; second < vertex_count; ++second) {
    if (!used[second] && edges[first][second]) {
      used[second] = true;
      best = std::max(best,
                      std::size_t{1} + exhaustive_from(edges, used));
      used[second] = false;
    }
  }
  used[first] = false;
  return best;
}

std::size_t exhaustive_matching(const Graph& graph) {
  const auto edges = edge_matrix(graph);
  std::vector<bool> used(graph.vertex_count(), false);
  return exhaustive_from(edges, used);
}

void verify_witness(const Graph& graph, const GeneralMatchingResult& result) {
  const auto edges = edge_matrix(graph);
  REQUIRE_EQ(result.mate.size(), graph.vertex_count());

  std::size_t counted = 0;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (!result.mate[vertex].has_value()) {
      continue;
    }
    const Vertex mate = *result.mate[vertex];
    REQUIRE(mate < graph.vertex_count());
    REQUIRE(mate != vertex);
    REQUIRE(result.mate[mate].has_value());
    REQUIRE_EQ(*result.mate[mate], vertex);
    REQUIRE(edges[vertex][mate]);
    if (vertex < mate) {
      ++counted;
    }
  }
  REQUIRE_EQ(counted, result.cardinality);
}

void add_petersen_graph(Graph& graph) {
  for (Vertex vertex = 0; vertex < 5; ++vertex) {
    graph.add_edge(vertex, (vertex + 1) % 5,
                   static_cast<std::int64_t>(vertex + 7));
    graph.add_edge(vertex, vertex + 5,
                   -static_cast<std::int64_t>(vertex + 1));
  }
  graph.add_edge(5, 7);
  graph.add_edge(7, 9);
  graph.add_edge(9, 6);
  graph.add_edge(6, 8);
  graph.add_edge(8, 5);
}

}  // namespace

TEST_CASE(general_matching_odd_cycles_and_multigraph_semantics) {
  Graph triangle(3, false);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  const auto triangle_result = edmonds_blossom_maximum_matching(triangle);
  verify_witness(triangle, triangle_result);
  REQUIRE_EQ(triangle_result.cardinality, std::size_t{1});

  Graph five_cycle(5, false);
  for (Vertex vertex = 0; vertex < 5; ++vertex) {
    five_cycle.add_edge(vertex, (vertex + 1) % 5);
  }
  const auto cycle_result = edmonds_blossom_maximum_matching(five_cycle);
  verify_witness(five_cycle, cycle_result);
  REQUIRE_EQ(cycle_result.cardinality, std::size_t{2});

  Graph tail(6, false);
  tail.add_edge(0, 1);
  tail.add_edge(1, 2);
  tail.add_edge(2, 0);
  tail.add_edge(2, 3);
  tail.add_edge(3, 4);
  tail.add_edge(4, 5);
  const auto tail_result = edmonds_blossom_maximum_matching(tail);
  verify_witness(tail, tail_result);
  REQUIRE_EQ(tail_result.cardinality, std::size_t{3});

  Graph multigraph(4, false);
  multigraph.add_edge(0, 0);
  multigraph.add_edge(0, 1);
  multigraph.add_edge(0, 1);
  multigraph.add_edge(2, 3);
  const auto multigraph_result =
      edmonds_blossom_maximum_matching(multigraph);
  verify_witness(multigraph, multigraph_result);
  REQUIRE_EQ(multigraph_result.cardinality, std::size_t{2});
}

TEST_CASE(general_matching_petersen_and_deterministic_witness) {
  Graph petersen(10, false);
  add_petersen_graph(petersen);

  const auto first = edmonds_blossom_maximum_matching(petersen);
  const auto second = edmonds_blossom_maximum_matching(petersen);
  verify_witness(petersen, first);
  verify_witness(petersen, second);
  REQUIRE_EQ(first.cardinality, std::size_t{5});
  REQUIRE_EQ(first.mate, second.mate);
}

TEST_CASE(general_matching_rejects_directed_input) {
  Graph graph(2, true);
  graph.add_edge(0, 1);
  REQUIRE_THROWS_AS(edmonds_blossom_maximum_matching(graph),
                    std::invalid_argument);
}

TEST_CASE(general_matching_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xB10550ULL);
  for (std::size_t trial = 0; trial < 1000; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(rng() % 10U);
    Graph graph(vertex_count, false);
    for (Vertex first = 0; first < vertex_count; ++first) {
      if ((rng() % 11U) == 0U) {
        graph.add_edge(first, first);
      }
      for (Vertex second = first + 1; second < vertex_count; ++second) {
        if ((rng() % 100U) < 38U) {
          graph.add_edge(first, second);
          if ((rng() % 9U) == 0U) {
            graph.add_edge(first, second);
          }
        }
      }
    }

    const auto result = edmonds_blossom_maximum_matching(graph);
    verify_witness(graph, result);
    REQUIRE_EQ(result.cardinality, exhaustive_matching(graph));
  }
}

TEST_CASE(general_matching_bipartite_crosscheck_hopcroft_karp) {
  std::mt19937_64 rng(0xB1A47EULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t left_count = static_cast<std::size_t>(rng() % 6U);
    const std::size_t right_count = static_cast<std::size_t>(rng() % 6U);
    Graph graph(left_count + right_count, false);
    std::vector<BipartiteEdge> edges;

    for (std::size_t left = 0; left < left_count; ++left) {
      for (std::size_t right = 0; right < right_count; ++right) {
        if ((rng() % 100U) < 42U) {
          edges.push_back(BipartiteEdge{left, right});
          graph.add_edge(left, left_count + right);
          if ((rng() % 13U) == 0U) {
            edges.push_back(BipartiteEdge{left, right});
            graph.add_edge(left, left_count + right);
          }
        }
      }
    }

    const auto general = edmonds_blossom_maximum_matching(graph);
    const auto bipartite = hopcroft_karp(left_count, right_count, edges);
    verify_witness(graph, general);
    REQUIRE_EQ(general.cardinality, bipartite.cardinality);
  }
}
