#include "algorithms/graphs/biconnected_components.hpp"
#include "algorithms/graphs/traversal.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::BlockCutIncidence;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::VertexBiconnectedDecomposition;
using algorithms::graphs::vertex_biconnected_decomposition;

std::vector<std::vector<unsigned char>> structural_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<unsigned char>> adjacency(
      n, std::vector<unsigned char>(n, 0));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        adjacency[from][edge.to] = 1;
        adjacency[edge.to][from] = 1;
      }
    }
  }
  return adjacency;
}

bool subset_connected(const std::vector<std::vector<unsigned char>>& adjacency,
                      std::uint64_t mask, std::size_t removed) {
  const std::size_t n = adjacency.size();
  std::size_t start = n;
  std::size_t count = 0;
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (((mask >> vertex) & 1ULL) != 0ULL && vertex != removed) {
      if (start == n) start = vertex;
      ++count;
    }
  }
  if (count <= 1) return true;
  std::vector<unsigned char> seen(n, 0);
  std::vector<std::size_t> stack{start};
  seen[start] = 1;
  std::size_t reached = 0;
  while (!stack.empty()) {
    const std::size_t from = stack.back();
    stack.pop_back();
    ++reached;
    for (std::size_t to = 0; to < n; ++to) {
      if (to != removed && ((mask >> to) & 1ULL) != 0ULL &&
          adjacency[from][to] != 0U && seen[to] == 0U) {
        seen[to] = 1;
        stack.push_back(to);
      }
    }
  }
  return reached == count;
}

bool subset_has_edge(const std::vector<std::vector<unsigned char>>& adjacency,
                     std::uint64_t mask) {
  for (std::size_t first = 0; first < adjacency.size(); ++first) {
    if (((mask >> first) & 1ULL) == 0ULL) continue;
    for (std::size_t second = first + 1; second < adjacency.size(); ++second) {
      if (((mask >> second) & 1ULL) != 0ULL &&
          adjacency[first][second] != 0U) {
        return true;
      }
    }
  }
  return false;
}

bool is_block_candidate(const std::vector<std::vector<unsigned char>>& adjacency,
                        std::uint64_t mask) {
  const int cardinality = std::popcount(mask);
  if (cardinality == 0) return false;
  if (cardinality == 1) {
    const std::size_t only = static_cast<std::size_t>(std::countr_zero(mask));
    for (std::size_t other = 0; other < adjacency.size(); ++other) {
      if (adjacency[only][other] != 0U) return false;
    }
    return true;
  }
  const std::size_t none = adjacency.size();
  if (!subset_connected(adjacency, mask, none)) return false;
  if (cardinality == 2) return subset_has_edge(adjacency, mask);
  for (std::size_t removed = 0; removed < adjacency.size(); ++removed) {
    if (((mask >> removed) & 1ULL) != 0ULL &&
        !subset_connected(adjacency, mask, removed)) {
      return false;
    }
  }
  return true;
}

std::vector<std::vector<Vertex>> exhaustive_blocks(const Graph& graph) {
  const auto adjacency = structural_adjacency(graph);
  const std::size_t n = adjacency.size();
  REQUIRE(n < 63);
  const std::uint64_t limit = 1ULL << n;
  std::vector<std::uint64_t> candidates;
  for (std::uint64_t mask = 1; mask < limit; ++mask) {
    if (is_block_candidate(adjacency, mask)) candidates.push_back(mask);
  }
  std::vector<std::vector<Vertex>> blocks;
  for (std::uint64_t candidate : candidates) {
    bool maximal = true;
    for (std::uint64_t other : candidates) {
      if (other != candidate && (candidate & other) == candidate) {
        maximal = false;
        break;
      }
    }
    if (!maximal) continue;
    std::vector<Vertex> block;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (((candidate >> vertex) & 1ULL) != 0ULL) block.push_back(vertex);
    }
    blocks.push_back(std::move(block));
  }
  std::sort(blocks.begin(), blocks.end());
  return blocks;
}

void verify_against_oracle(const Graph& graph) {
  const VertexBiconnectedDecomposition actual =
      vertex_biconnected_decomposition(graph);
  const auto expected_blocks = exhaustive_blocks(graph);
  REQUIRE_EQ(actual.blocks, expected_blocks);

  std::vector<std::size_t> membership(graph.vertex_count(), 0);
  for (const auto& block : expected_blocks) {
    for (Vertex vertex : block) ++membership[vertex];
  }
  std::vector<Vertex> expected_articulations;
  std::vector<BlockCutIncidence> expected_incidence;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (membership[vertex] <= 1) continue;
    expected_articulations.push_back(vertex);
    for (std::size_t block = 0; block < expected_blocks.size(); ++block) {
      if (std::binary_search(expected_blocks[block].begin(),
                             expected_blocks[block].end(), vertex)) {
        expected_incidence.push_back({vertex, block});
      }
    }
  }
  REQUIRE_EQ(actual.articulation_vertices, expected_articulations);
  REQUIRE_EQ(actual.articulation_vertices,
             algorithms::graphs::analyze_undirected_low_link(graph)
                 .articulation_vertices);
  REQUIRE_EQ(actual.block_cut_incidence, expected_incidence);
}
}  // namespace

TEST_CASE(vertex_biconnected_decomposition_deterministic_structures) {
  Graph empty(0, false);
  verify_against_oracle(empty);

  Graph singleton(1, false);
  singleton.add_edge(0, 0, -9);
  verify_against_oracle(singleton);

  Graph path(4, false);
  path.add_edge(0, 1);
  path.add_edge(1, 2);
  path.add_edge(2, 3);
  const auto path_result = vertex_biconnected_decomposition(path);
  REQUIRE_EQ(path_result.blocks,
             (std::vector<std::vector<Vertex>>{{0, 1}, {1, 2}, {2, 3}}));
  REQUIRE_EQ(path_result.articulation_vertices, (std::vector<Vertex>{1, 2}));
  verify_against_oracle(path);

  Graph bow_tie(5, false);
  bow_tie.add_edge(0, 1);
  bow_tie.add_edge(1, 2);
  bow_tie.add_edge(2, 0);
  bow_tie.add_edge(2, 3);
  bow_tie.add_edge(3, 4);
  bow_tie.add_edge(4, 2);
  verify_against_oracle(bow_tie);
}

TEST_CASE(vertex_biconnected_decomposition_multigraph_semantics) {
  Graph graph(5, false);
  graph.add_edge(0, 1, 4);
  graph.add_edge(0, 1, -7);
  graph.add_edge(1, 2, 9);
  graph.add_edge(2, 0, 3);
  graph.add_edge(2, 3, 1);
  graph.add_edge(3, 3, -100);
  graph.add_edge(4, 4, 12);
  verify_against_oracle(graph);
  const auto first = vertex_biconnected_decomposition(graph);
  const auto second = vertex_biconnected_decomposition(graph);
  REQUIRE_EQ(first.blocks, second.blocks);
  REQUIRE_EQ(first.block_cut_incidence, second.block_cut_incidence);
}

TEST_CASE(vertex_biconnected_decomposition_rejects_directed_input) {
  Graph directed(3, true);
  directed.add_edge(0, 1);
  directed.add_edge(1, 2);
  REQUIRE_THROWS_AS(vertex_biconnected_decomposition(directed),
                    std::invalid_argument);
}

TEST_CASE(vertex_biconnected_decomposition_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xB10CC07ULL);
  for (int trial = 0; trial < 500; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    Graph graph(n, false);
    const int edge_copies = static_cast<int>(rng() % 19U);
    for (int edge = 0; edge < edge_copies && n != 0; ++edge) {
      const Vertex first = static_cast<Vertex>(rng() % n);
      const Vertex second = static_cast<Vertex>(rng() % n);
      const auto weight =
          static_cast<algorithms::graphs::Weight>(rng() % 21U) - 10;
      graph.add_edge(first, second, weight);
      if ((rng() % 5U) == 0U) graph.add_edge(first, second, -weight);
    }
    verify_against_oracle(graph);
  }
}
