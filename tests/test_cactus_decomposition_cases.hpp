#pragma once

#include "algorithms/graphs/cactus_decomposition.hpp"
#include "algorithms/graphs/graph.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::CactusAnalysis;
using algorithms::graphs::CactusBlock;
using algorithms::graphs::CactusBlockKind;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::analyze_cactus_forest;
using UndirectedEdge = std::pair<Vertex, Vertex>;

UndirectedEdge cactus_normalize(Vertex first, Vertex second) {
  return first < second ? UndirectedEdge{first, second}
                        : UndirectedEdge{second, first};
}

std::vector<UndirectedEdge> cactus_simple_edges(const Graph& graph) {
  std::vector<UndirectedEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.emplace_back(from, edge.to);
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  return edges;
}

std::size_t cactus_alternate_path_count_capped_two(
    std::size_t vertex_count, const std::vector<UndirectedEdge>& edges,
    std::size_t excluded_edge, Vertex source, Vertex target) {
  std::vector<std::vector<Vertex>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    if (edge_id == excluded_edge) {
      continue;
    }
    const auto [first, second] = edges[edge_id];
    adjacency[first].push_back(second);
    adjacency[second].push_back(first);
  }

  std::vector<bool> visited(vertex_count, false);
  std::size_t paths = 0;
  std::function<void(Vertex)> search = [&](Vertex vertex) {
    if (paths >= 2) {
      return;
    }
    if (vertex == target) {
      ++paths;
      return;
    }
    visited[vertex] = true;
    for (const Vertex next : adjacency[vertex]) {
      if (!visited[next]) {
        search(next);
      }
      if (paths >= 2) {
        break;
      }
    }
    visited[vertex] = false;
  };
  search(source);
  return paths;
}

bool cactus_oracle_is_cactus_forest(const Graph& graph) {
  const auto edges = cactus_simple_edges(graph);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const auto [first, second] = edges[edge_id];
    if (cactus_alternate_path_count_capped_two(graph.vertex_count(), edges, edge_id,
                                        first, second) >= 2) {
      return false;
    }
  }
  return true;
}

std::size_t cactus_oracle_component_count(const Graph& graph) {
  std::vector<bool> seen(graph.vertex_count(), false);
  std::size_t components = 0;
  for (Vertex start = 0; start < graph.vertex_count(); ++start) {
    if (seen[start]) {
      continue;
    }
    ++components;
    std::queue<Vertex> queue;
    queue.push(start);
    seen[start] = true;
    while (!queue.empty()) {
      const Vertex vertex = queue.front();
      queue.pop();
      for (const auto& edge : graph.neighbors(vertex)) {
        if (!seen[edge.to]) {
          seen[edge.to] = true;
          queue.push(edge.to);
        }
      }
    }
  }
  return components;
}

void cactus_validate_decomposition(const Graph& graph, const CactusAnalysis& analysis) {
  const auto original_edges = cactus_simple_edges(graph);
  std::vector<UndirectedEdge> decomposed_edges;
  bool saw_complex = false;

  for (const CactusBlock& block : analysis.blocks) {
    decomposed_edges.insert(decomposed_edges.end(), block.edges.begin(),
                            block.edges.end());
    REQUIRE(std::is_sorted(block.edges.begin(), block.edges.end()));
    if (block.kind == CactusBlockKind::kBridge) {
      REQUIRE_EQ(block.edges.size(), std::size_t{1});
      REQUIRE(block.cycle_vertices.empty());
      continue;
    }
    if (block.kind == CactusBlockKind::kComplex) {
      saw_complex = true;
      REQUIRE(block.edges.size() >= 2);
      REQUIRE(block.cycle_vertices.empty());
      continue;
    }

    REQUIRE(block.kind == CactusBlockKind::kCycle);
    REQUIRE(block.cycle_vertices.size() >= 3);
    REQUIRE_EQ(block.edges.size(), block.cycle_vertices.size());
    REQUIRE_EQ(block.cycle_vertices.front(),
               *std::min_element(block.cycle_vertices.begin(),
                                 block.cycle_vertices.end()));
    REQUIRE(block.cycle_vertices[1] < block.cycle_vertices.back());

    std::vector<UndirectedEdge> witness_edges;
    for (std::size_t index = 0; index < block.cycle_vertices.size(); ++index) {
      const Vertex first = block.cycle_vertices[index];
      const Vertex second =
          block.cycle_vertices[(index + 1) % block.cycle_vertices.size()];
      witness_edges.push_back(cactus_normalize(first, second));
    }
    std::sort(witness_edges.begin(), witness_edges.end());
    REQUIRE_EQ(witness_edges, block.edges);
  }

  std::sort(decomposed_edges.begin(), decomposed_edges.end());
  REQUIRE_EQ(decomposed_edges, original_edges);
  REQUIRE_EQ(analysis.connected_components, cactus_oracle_component_count(graph));
  REQUIRE_EQ(analysis.is_cactus, !saw_complex);
}

Graph cactus_make_figure_eight() {
  Graph graph(6, false);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(2, 0);
  graph.add_edge(0, 3);
  graph.add_edge(3, 4);
  graph.add_edge(4, 0);
  graph.add_edge(4, 5);
  return graph;
}

TEST_CASE(cactus_empty_tree_cycle_and_disconnected_contracts) {
  {
    Graph empty(0, false);
    const auto result = analyze_cactus_forest(empty);
    REQUIRE(result.is_cactus);
    REQUIRE_EQ(result.connected_components, std::size_t{0});
    REQUIRE(result.blocks.empty());
  }
  {
    Graph forest(5, false);
    forest.add_edge(0, 1, 17);
    forest.add_edge(1, 2, -9);
    forest.add_edge(3, 4, 77);
    const auto result = analyze_cactus_forest(forest);
    REQUIRE(result.is_cactus);
    REQUIRE_EQ(result.connected_components, std::size_t{2});
    REQUIRE_EQ(result.blocks.size(), std::size_t{3});
    cactus_validate_decomposition(forest, result);
  }
  {
    Graph cycle(4, false);
    cycle.add_edge(0, 1);
    cycle.add_edge(1, 2);
    cycle.add_edge(2, 3);
    cycle.add_edge(3, 0);
    const auto result = analyze_cactus_forest(cycle);
    REQUIRE(result.is_cactus);
    REQUIRE_EQ(result.blocks.size(), std::size_t{1});
    REQUIRE(result.blocks[0].kind == CactusBlockKind::kCycle);
    REQUIRE_EQ(result.blocks[0].cycle_vertices,
               (std::vector<Vertex>{0, 1, 2, 3}));
    cactus_validate_decomposition(cycle, result);
  }
}

TEST_CASE(cactus_figure_eight_is_decomposed_into_two_cycles_and_bridge) {
  Graph graph = cactus_make_figure_eight();
  const auto result = analyze_cactus_forest(graph);
  REQUIRE(result.is_cactus);
  REQUIRE_EQ(result.connected_components, std::size_t{1});
  REQUIRE_EQ(result.blocks.size(), std::size_t{3});
  REQUIRE(result.blocks[0].kind == CactusBlockKind::kCycle);
  REQUIRE_EQ(result.blocks[0].cycle_vertices,
             (std::vector<Vertex>{0, 1, 2}));
  REQUIRE(result.blocks[1].kind == CactusBlockKind::kCycle);
  REQUIRE_EQ(result.blocks[1].cycle_vertices,
             (std::vector<Vertex>{0, 3, 4}));
  REQUIRE(result.blocks[2].kind == CactusBlockKind::kBridge);
  REQUIRE_EQ(result.blocks[2].edges,
             (std::vector<UndirectedEdge>{{4, 5}}));
  cactus_validate_decomposition(graph, result);
}

TEST_CASE(cactus_complex_blocks_and_simple_graph_validation) {
  {
    Graph diamond(4, false);
    diamond.add_edge(0, 1);
    diamond.add_edge(1, 2);
    diamond.add_edge(2, 0);
    diamond.add_edge(1, 3);
    diamond.add_edge(3, 2);
    const auto result = analyze_cactus_forest(diamond);
    REQUIRE(!result.is_cactus);
    REQUIRE(std::any_of(result.blocks.begin(), result.blocks.end(),
                        [](const CactusBlock& block) {
                          return block.kind == CactusBlockKind::kComplex;
                        }));
    cactus_validate_decomposition(diamond, result);
  }
  {
    Graph directed(2, true);
    directed.add_edge(0, 1);
    REQUIRE_THROWS_AS(analyze_cactus_forest(directed), std::invalid_argument);
  }
  {
    Graph loop(1, false);
    loop.add_edge(0, 0);
    REQUIRE_THROWS_AS(analyze_cactus_forest(loop), std::invalid_argument);
  }
  {
    Graph parallel(2, false);
    parallel.add_edge(0, 1);
    parallel.add_edge(0, 1);
    REQUIRE_THROWS_AS(analyze_cactus_forest(parallel), std::invalid_argument);
  }
}

TEST_CASE(cactus_ignores_weights_is_iterative_and_matches_independent_oracle) {
  {
    Graph weighted = cactus_make_figure_eight();
    Graph reweighted(6, false);
    reweighted.add_edge(0, 1, -100);
    reweighted.add_edge(1, 2, 9);
    reweighted.add_edge(2, 0, 17);
    reweighted.add_edge(0, 3, 999);
    reweighted.add_edge(3, 4, -42);
    reweighted.add_edge(4, 0, 123);
    reweighted.add_edge(4, 5, -1);
    REQUIRE_EQ(analyze_cactus_forest(weighted),
               analyze_cactus_forest(reweighted));
  }
  {
    constexpr std::size_t kChainSize = 20000;
    Graph chain(kChainSize, false);
    for (Vertex vertex = 1; vertex < kChainSize; ++vertex) {
      chain.add_edge(vertex - 1, vertex);
    }
    const auto result = analyze_cactus_forest(chain);
    REQUIRE(result.is_cactus);
    REQUIRE_EQ(result.blocks.size(), kChainSize - 1);
  }

  std::mt19937_64 rng(0xCA67A5ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 9);
  std::bernoulli_distribution edge_distribution(0.28);
  std::uniform_int_distribution<int> weight_distribution(-1000, 1000);

  for (std::size_t trial = 0; trial < 600; ++trial) {
    const std::size_t n =
        static_cast<std::size_t>(vertex_count_distribution(rng));
    Graph graph(n, false);
    for (Vertex first = 0; first < n; ++first) {
      for (Vertex second = first + 1; second < n; ++second) {
        if (edge_distribution(rng)) {
          graph.add_edge(first, second,
                         static_cast<std::int64_t>(weight_distribution(rng)));
        }
      }
    }

    const auto result = analyze_cactus_forest(graph);
    REQUIRE_EQ(result.is_cactus, cactus_oracle_is_cactus_forest(graph));
    cactus_validate_decomposition(graph, result);
  }
}

}  // namespace
