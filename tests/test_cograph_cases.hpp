#pragma once

#include "algorithms/graphs/cograph.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::CographCotree;
using algorithms::graphs::CotreeNodeKind;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::cograph_cotree;
using algorithms::graphs::valid_cograph_cotree;

std::vector<std::vector<std::uint8_t>> structural_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<std::uint8_t>> adjacency(
      n, std::vector<std::uint8_t>(n, 0U));
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to != from) {
        adjacency[from][edge.to] = 1U;
        adjacency[edge.to][from] = 1U;
      }
    }
  }
  return adjacency;
}

bool independent_contains_induced_p4(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  const auto adjacency = structural_adjacency(graph);
  for (Vertex a = 0U; a < n; ++a) {
    for (Vertex b = a + 1U; b < n; ++b) {
      for (Vertex c = b + 1U; c < n; ++c) {
        for (Vertex d = c + 1U; d < n; ++d) {
          const std::array<Vertex, 4U> vertices{a, b, c, d};
          std::array<std::size_t, 4U> degrees{0U, 0U, 0U, 0U};
          std::size_t edges = 0U;
          for (std::size_t left = 0U; left < vertices.size(); ++left) {
            for (std::size_t right = left + 1U; right < vertices.size();
                 ++right) {
              if (adjacency[vertices[left]][vertices[right]] != 0U) {
                ++degrees[left];
                ++degrees[right];
                ++edges;
              }
            }
          }
          std::sort(degrees.begin(), degrees.end());
          if (edges == 3U &&
              degrees == std::array<std::size_t, 4U>{1U, 1U, 2U, 2U}) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

Graph simple_graph_from_mask(const std::size_t n, std::uint64_t mask) {
  Graph graph(n, false);
  std::size_t bit = 0U;
  for (Vertex left = 0U; left < n; ++left) {
    for (Vertex right = left + 1U; right < n; ++right) {
      if (((mask >> bit) & 1ULL) != 0ULL) {
        graph.add_edge(left, right, 1);
      }
      ++bit;
    }
  }
  return graph;
}

std::uint64_t simple_edge_count(const std::size_t n) {
  return static_cast<std::uint64_t>(n * (n - 1U) / 2U);
}

void require_cotree_reconstructs(const Graph& graph,
                                 const CographCotree& cotree) {
  REQUIRE(valid_cograph_cotree(graph, cotree));
  const auto adjacency = structural_adjacency(graph);
  const std::size_t n = graph.vertex_count();

  std::vector<Vertex> leaf_vertex(cotree.nodes.size(), n);
  for (std::size_t id = 0U; id < cotree.nodes.size(); ++id) {
    if (cotree.nodes[id].kind == CotreeNodeKind::leaf) {
      REQUIRE(cotree.nodes[id].vertex.has_value());
      leaf_vertex[id] = *cotree.nodes[id].vertex;
    }
  }

  const auto leaves_under = [&](auto&& self, const std::size_t id)
      -> std::vector<Vertex> {
    const auto& node = cotree.nodes[id];
    if (node.kind == CotreeNodeKind::leaf) {
      return {leaf_vertex[id]};
    }
    std::vector<Vertex> result;
    for (const std::size_t child : node.children) {
      auto child_leaves = self(self, child);
      result.insert(result.end(), child_leaves.begin(), child_leaves.end());
    }
    std::sort(result.begin(), result.end());
    return result;
  };

  if (n == 0U) {
    REQUIRE(!cotree.root.has_value());
    return;
  }
  REQUIRE(cotree.root.has_value());
  const auto all = leaves_under(leaves_under, *cotree.root);
  REQUIRE_EQ(all.size(), n);
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    REQUIRE_EQ(all[vertex], vertex);
  }

  // Independent pair replay: the first internal node containing both leaves
  // determines whether their cross relation is union (no edge) or join (edge).
  const auto pair_relation = [&](auto&& self, const std::size_t id,
                                 const Vertex first,
                                 const Vertex second) -> int {
    const auto& node = cotree.nodes[id];
    if (node.kind == CotreeNodeKind::leaf) {
      return -1;
    }
    for (const std::size_t child : node.children) {
      const auto child_leaves = leaves_under(leaves_under, child);
      const bool has_first =
          std::binary_search(child_leaves.begin(), child_leaves.end(), first);
      const bool has_second =
          std::binary_search(child_leaves.begin(), child_leaves.end(), second);
      if (has_first && has_second) {
        return self(self, child, first, second);
      }
      if (has_first != has_second) {
        continue;
      }
    }
    return node.kind == CotreeNodeKind::complete_join ? 1 : 0;
  };

  for (Vertex first = 0U; first < n; ++first) {
    for (Vertex second = first + 1U; second < n; ++second) {
      REQUIRE_EQ(pair_relation(pair_relation, *cotree.root, first, second),
                 adjacency[first][second] != 0U ? 1 : 0);
    }
  }
}

TEST_CASE(cograph_validates_input_and_basic_cotrees) {
  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(cograph_cotree(directed), std::invalid_argument);

  Graph empty(0U, false);
  const auto empty_result = cograph_cotree(empty);
  REQUIRE(empty_result.has_value());
  REQUIRE(empty_result->nodes.empty());
  REQUIRE(!empty_result->root.has_value());
  require_cotree_reconstructs(empty, *empty_result);

  Graph singleton(1U, false);
  singleton.add_edge(0U, 0U, -17);
  const auto single_result = cograph_cotree(singleton);
  REQUIRE(single_result.has_value());
  REQUIRE_EQ(single_result->nodes.size(), 1U);
  REQUIRE_EQ(single_result->nodes[0].kind, CotreeNodeKind::leaf);
  REQUIRE_EQ(single_result->nodes[0].vertex, std::optional<Vertex>{0U});
  require_cotree_reconstructs(singleton, *single_result);

  Graph independent(4U, false);
  const auto independent_result = cograph_cotree(independent);
  REQUIRE(independent_result.has_value());
  REQUIRE(independent_result->root.has_value());
  REQUIRE_EQ(independent_result->nodes[*independent_result->root].kind,
             CotreeNodeKind::disjoint_union);
  require_cotree_reconstructs(independent, *independent_result);

  Graph clique(4U, false);
  for (Vertex first = 0U; first < 4U; ++first) {
    for (Vertex second = first + 1U; second < 4U; ++second) {
      clique.add_edge(first, second, static_cast<std::int64_t>(first + second));
    }
  }
  const auto clique_result = cograph_cotree(clique);
  REQUIRE(clique_result.has_value());
  REQUIRE_EQ(clique_result->nodes[*clique_result->root].kind,
             CotreeNodeKind::complete_join);
  require_cotree_reconstructs(clique, *clique_result);
}

TEST_CASE(cograph_rejects_induced_p4_and_accepts_c4) {
  Graph path(4U, false);
  path.add_edge(0U, 1U);
  path.add_edge(1U, 2U);
  path.add_edge(2U, 3U);
  REQUIRE(independent_contains_induced_p4(path));
  REQUIRE(!cograph_cotree(path).has_value());

  Graph cycle(4U, false);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 3U);
  cycle.add_edge(3U, 0U);
  REQUIRE(!independent_contains_induced_p4(cycle));
  const auto cycle_result = cograph_cotree(cycle);
  REQUIRE(cycle_result.has_value());
  require_cotree_reconstructs(cycle, *cycle_result);
}

TEST_CASE(cograph_ignores_loops_parallel_copies_weights_and_insertion_order) {
  Graph first(5U, false);
  first.add_edge(0U, 2U, 7);
  first.add_edge(0U, 3U, -9);
  first.add_edge(1U, 2U, 12);
  first.add_edge(1U, 3U, 1);
  first.add_edge(4U, 4U, 99);
  first.add_edge(0U, 2U, -500);

  Graph second(5U, false);
  second.add_edge(1U, 3U, 400);
  second.add_edge(1U, 2U, -4);
  second.add_edge(0U, 3U, 8);
  second.add_edge(0U, 2U, 0);
  second.add_edge(2U, 2U, -1);

  const auto first_result = cograph_cotree(first);
  const auto second_result = cograph_cotree(second);
  REQUIRE(first_result.has_value());
  REQUIRE(second_result.has_value());
  REQUIRE_EQ(*first_result, *second_result);
  require_cotree_reconstructs(first, *first_result);
  require_cotree_reconstructs(second, *second_result);
}

TEST_CASE(cograph_validator_rejects_tampered_witnesses) {
  Graph clique(3U, false);
  clique.add_edge(0U, 1U);
  clique.add_edge(0U, 2U);
  clique.add_edge(1U, 2U);
  const auto production = cograph_cotree(clique);
  REQUIRE(production.has_value());
  REQUIRE(valid_cograph_cotree(clique, *production));

  auto wrong_kind = *production;
  REQUIRE(wrong_kind.root.has_value());
  wrong_kind.nodes[*wrong_kind.root].kind = CotreeNodeKind::disjoint_union;
  REQUIRE(!valid_cograph_cotree(clique, wrong_kind));

  auto shared_child = *production;
  REQUIRE(shared_child.root.has_value());
  auto& root = shared_child.nodes[*shared_child.root];
  REQUIRE(root.children.size() >= 2U);
  root.children[1U] = root.children[0U];
  REQUIRE(!valid_cograph_cotree(clique, shared_child));

  auto unreachable = *production;
  unreachable.nodes.push_back(
      algorithms::graphs::CotreeNode{CotreeNodeKind::leaf, 0U, {}});
  REQUIRE(!valid_cograph_cotree(clique, unreachable));
}

TEST_CASE(cograph_exhaustive_all_simple_graphs_through_six_vertices) {
  for (std::size_t n = 0U; n <= 6U; ++n) {
    const std::uint64_t edges = simple_edge_count(n);
    const std::uint64_t graph_count = 1ULL << edges;
    for (std::uint64_t mask = 0ULL; mask < graph_count; ++mask) {
      const Graph graph = simple_graph_from_mask(n, mask);
      const bool expected = !independent_contains_induced_p4(graph);
      const auto production = cograph_cotree(graph);
      REQUIRE_EQ(production.has_value(), expected);
      if (production.has_value()) {
        require_cotree_reconstructs(graph, *production);
        REQUIRE_EQ(cograph_cotree(graph), production);
      }
    }
  }
}

TEST_CASE(cograph_random_multigraph_structural_differential) {
  std::mt19937_64 rng(0x434F47524150485FULL);
  std::uniform_int_distribution<std::size_t> n_dist(0U, 12U);
  std::uniform_int_distribution<int> copies_dist(0, 3);
  std::uniform_int_distribution<std::int64_t> weight_dist(-1000, 1000);

  for (std::size_t trial = 0U; trial < 800U; ++trial) {
    const std::size_t n = n_dist(rng);
    Graph graph(n, false);
    for (Vertex first = 0U; first < n; ++first) {
      for (Vertex second = first; second < n; ++second) {
        const int copies = copies_dist(rng);
        for (int copy = 0; copy < copies; ++copy) {
          graph.add_edge(first, second, weight_dist(rng));
        }
      }
    }
    const bool expected = !independent_contains_induced_p4(graph);
    const auto production = cograph_cotree(graph);
    REQUIRE_EQ(production.has_value(), expected);
    if (production.has_value()) {
      require_cotree_reconstructs(graph, *production);
    }
  }
}

}  // namespace
