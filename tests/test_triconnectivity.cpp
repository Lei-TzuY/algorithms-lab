#include "algorithms/graphs/triconnectivity.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::BiconnectedBlockKind;
using algorithms::graphs::BiconnectedBlockTriconnectivity;
using algorithms::graphs::Edge;
using algorithms::graphs::Graph;
using algorithms::graphs::SeparationPairWitness;
using algorithms::graphs::TriconnectivityAnalysis;
using algorithms::graphs::Vertex;
using algorithms::graphs::analyze_triconnectivity;

std::vector<std::vector<unsigned char>> structural_matrix(
    const Graph& graph, const std::vector<Vertex>& vertices) {
  const std::size_t n = graph.vertex_count();
  std::vector<unsigned char> in_block(n, 0);
  for (const Vertex vertex : vertices) {
    in_block[vertex] = 1;
  }
  std::vector<std::vector<unsigned char>> matrix(
      n, std::vector<unsigned char>(n, 0));
  for (const Vertex from : vertices) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to != from && edge.to < n && in_block[edge.to] != 0) {
        matrix[from][edge.to] = 1;
        matrix[edge.to][from] = 1;
      }
    }
  }
  return matrix;
}

std::vector<std::vector<Vertex>> oracle_components(
    const std::vector<Vertex>& vertices,
    const std::vector<std::vector<unsigned char>>& matrix, Vertex first,
    Vertex second) {
  std::vector<unsigned char> assigned(matrix.size(), 0);
  std::vector<std::vector<Vertex>> components;
  for (const Vertex start : vertices) {
    if (start == first || start == second || assigned[start] != 0) {
      continue;
    }
    std::vector<Vertex> component{start};
    assigned[start] = 1;
    for (std::size_t cursor = 0; cursor < component.size(); ++cursor) {
      const Vertex vertex = component[cursor];
      for (const Vertex candidate : vertices) {
        if (candidate == first || candidate == second ||
            assigned[candidate] != 0 || matrix[vertex][candidate] == 0) {
          continue;
        }
        assigned[candidate] = 1;
        component.push_back(candidate);
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  std::sort(components.begin(), components.end());
  return components;
}

std::vector<SeparationPairWitness> oracle_separation_pairs(
    const Graph& graph, const std::vector<Vertex>& vertices) {
  std::vector<SeparationPairWitness> result;
  if (vertices.size() < 4) {
    return result;
  }
  const auto matrix = structural_matrix(graph, vertices);
  for (std::size_t first = 0; first < vertices.size(); ++first) {
    for (std::size_t second = first + 1; second < vertices.size(); ++second) {
      auto components = oracle_components(vertices, matrix, vertices[first],
                                          vertices[second]);
      if (components.size() > 1) {
        result.push_back(
            {vertices[first], vertices[second], std::move(components)});
      }
    }
  }
  return result;
}

void add_undirected(Graph& graph, Vertex first, Vertex second,
                    std::int64_t weight = 1) {
  graph.add_edge(first, second, weight);
}

const BiconnectedBlockTriconnectivity& only_block(
    const TriconnectivityAnalysis& analysis) {
  REQUIRE_EQ(analysis.blocks.size(), std::size_t{1});
  return analysis.blocks.front();
}

TEST_CASE(triconnectivity_rejects_directed_input) {
  Graph graph(3, true);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  REQUIRE_THROWS_AS(analyze_triconnectivity(graph), std::invalid_argument);
}

TEST_CASE(triconnectivity_classifies_small_blocks_and_articulations) {
  Graph graph(10, false);
  for (Vertex first = 0; first < 4; ++first) {
    for (Vertex second = first + 1; second < 4; ++second) {
      add_undirected(graph, first, second);
    }
  }
  add_undirected(graph, 4, 5);
  add_undirected(graph, 5, 6);
  add_undirected(graph, 6, 4);
  add_undirected(graph, 7, 8);
  add_undirected(graph, 8, 9);

  const auto result = analyze_triconnectivity(graph);
  REQUIRE_EQ(result.articulation_vertices, std::vector<Vertex>({8}));
  REQUIRE_EQ(result.blocks.size(), std::size_t{4});
  REQUIRE_EQ(result.blocks[0].vertices,
             std::vector<Vertex>({0, 1, 2, 3}));
  REQUIRE(result.blocks[0].kind == BiconnectedBlockKind::triconnected);
  REQUIRE_EQ(result.blocks[1].vertices, std::vector<Vertex>({4, 5, 6}));
  REQUIRE(result.blocks[1].kind == BiconnectedBlockKind::triangle);
  REQUIRE_EQ(result.blocks[2].vertices, std::vector<Vertex>({7, 8}));
  REQUIRE(result.blocks[2].kind == BiconnectedBlockKind::bridge);
  REQUIRE_EQ(result.blocks[3].vertices, std::vector<Vertex>({8, 9}));
  REQUIRE(result.blocks[3].kind == BiconnectedBlockKind::bridge);
}

TEST_CASE(triconnectivity_finds_cycle_and_two_cliques_split_pairs) {
  Graph cycle(4, false);
  add_undirected(cycle, 0, 1);
  add_undirected(cycle, 1, 2);
  add_undirected(cycle, 2, 3);
  add_undirected(cycle, 3, 0);
  const auto cycle_block = only_block(analyze_triconnectivity(cycle));
  REQUIRE(cycle_block.kind ==
          BiconnectedBlockKind::split_pair_decomposable);
  REQUIRE_EQ(cycle_block.separation_pairs,
             std::vector<SeparationPairWitness>({
                 {0, 2, {{1}, {3}}}, {1, 3, {{0}, {2}}}}));

  Graph glued(6, false);
  for (const Vertex x : std::vector<Vertex>({2, 3})) {
    add_undirected(glued, 0, x);
    add_undirected(glued, 1, x);
  }
  add_undirected(glued, 0, 1);
  add_undirected(glued, 2, 3);
  for (const Vertex x : std::vector<Vertex>({4, 5})) {
    add_undirected(glued, 0, x);
    add_undirected(glued, 1, x);
  }
  add_undirected(glued, 4, 5);

  const auto glued_block = only_block(analyze_triconnectivity(glued));
  REQUIRE(glued_block.kind ==
          BiconnectedBlockKind::split_pair_decomposable);
  REQUIRE_EQ(glued_block.separation_pairs,
             std::vector<SeparationPairWitness>({
                 {0, 1, {{2, 3}, {4, 5}}}}));
}

TEST_CASE(triconnectivity_ignores_loop_parallel_and_weight_multiplicity) {
  Graph graph(4, false);
  for (Vertex first = 0; first < 4; ++first) {
    for (Vertex second = first + 1; second < 4; ++second) {
      add_undirected(graph, first, second,
                     static_cast<std::int64_t>(first + second + 1));
    }
  }
  add_undirected(graph, 0, 1, -100);
  add_undirected(graph, 0, 1, 999);
  graph.add_edge(0, 0, -7);
  graph.add_edge(3, 3, 22);

  const auto block = only_block(analyze_triconnectivity(graph));
  REQUIRE(block.kind == BiconnectedBlockKind::triconnected);
  REQUIRE(block.separation_pairs.empty());
}

TEST_CASE(triconnectivity_random_biconnected_graphs_match_independent_oracle) {
  std::mt19937_64 rng(0x53505152545249ULL);
  for (std::size_t trial = 0; trial < 1200; ++trial) {
    const std::size_t n = 3 + static_cast<std::size_t>(rng() % 6U);
    Graph graph(n, false);

    for (Vertex vertex = 0; vertex < n; ++vertex) {
      add_undirected(graph, vertex, (vertex + 1) % n,
                     static_cast<std::int64_t>(rng() % 21U) - 10);
    }
    for (Vertex first = 0; first < n; ++first) {
      for (Vertex second = first + 1; second < n; ++second) {
        const bool cycle_edge = second == first + 1 ||
                                (first == 0 && second + 1 == n);
        if (!cycle_edge && (rng() % 4U) == 0U) {
          add_undirected(graph, first, second,
                         static_cast<std::int64_t>(rng() % 51U) - 25);
          if ((rng() % 5U) == 0U) {
            add_undirected(graph, first, second,
                           static_cast<std::int64_t>(rng() % 51U) - 25);
          }
        }
      }
      if ((rng() % 7U) == 0U) {
        graph.add_edge(first, first,
                       static_cast<std::int64_t>(rng() % 31U) - 15);
      }
    }

    const auto result = analyze_triconnectivity(graph);
    REQUIRE(result.articulation_vertices.empty());
    const auto& block = only_block(result);
    std::vector<Vertex> vertices(n);
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      vertices[vertex] = vertex;
    }
    REQUIRE_EQ(block.vertices, vertices);
    const auto expected = oracle_separation_pairs(graph, vertices);
    REQUIRE_EQ(block.separation_pairs, expected);
    if (n == 3) {
      REQUIRE(block.kind == BiconnectedBlockKind::triangle);
    } else if (expected.empty()) {
      REQUIRE(block.kind == BiconnectedBlockKind::triconnected);
    } else {
      REQUIRE(block.kind ==
              BiconnectedBlockKind::split_pair_decomposable);
    }
  }
}

}  // namespace
