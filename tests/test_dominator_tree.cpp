#include "algorithms/graphs/dominator_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::DominatorTree;
using algorithms::graphs::Edge;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct DominatorOracle {
  std::vector<unsigned char> reachable;
  std::vector<std::vector<unsigned char>> dominators;
  std::vector<std::optional<Vertex>> immediate_dominator;
};

DominatorOracle fixed_point_oracle(const Graph& graph, Vertex start) {
  graph.validate_vertex(start);
  const std::size_t n = graph.vertex_count();

  std::vector<unsigned char> reachable(n, 0U);
  std::vector<Vertex> stack{start};
  reachable[start] = 1U;
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (reachable[edge.to] == 0U) {
        reachable[edge.to] = 1U;
        stack.push_back(edge.to);
      }
    }
  }

  std::vector<std::vector<Vertex>> predecessors(n);
  for (Vertex from = 0U; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      predecessors[edge.to].push_back(from);
    }
  }

  std::vector<std::vector<unsigned char>> dominators(
      n, std::vector<unsigned char>(n, 0U));
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if (reachable[vertex] == 0U) {
      continue;
    }
    if (vertex == start) {
      dominators[vertex][start] = 1U;
    } else {
      for (Vertex candidate = 0U; candidate < n; ++candidate) {
        dominators[vertex][candidate] = reachable[candidate];
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      if (reachable[vertex] == 0U || vertex == start) {
        continue;
      }

      std::vector<unsigned char> next(n, 0U);
      bool initialized = false;
      for (const Vertex predecessor : predecessors[vertex]) {
        if (reachable[predecessor] == 0U) {
          continue;
        }
        if (!initialized) {
          next = dominators[predecessor];
          initialized = true;
        } else {
          for (Vertex candidate = 0U; candidate < n; ++candidate) {
            next[candidate] = static_cast<unsigned char>(
                next[candidate] != 0U &&
                dominators[predecessor][candidate] != 0U);
          }
        }
      }
      REQUIRE(initialized);
      next[vertex] = 1U;
      if (next != dominators[vertex]) {
        dominators[vertex] = std::move(next);
        changed = true;
      }
    }
  }

  std::vector<std::optional<Vertex>> immediate(n);
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if (reachable[vertex] == 0U || vertex == start) {
      continue;
    }

    std::optional<Vertex> chosen;
    for (Vertex candidate = 0U; candidate < n; ++candidate) {
      if (candidate == vertex || dominators[vertex][candidate] == 0U) {
        continue;
      }

      bool dominated_by_all_strict_dominators = true;
      for (Vertex other = 0U; other < n; ++other) {
        if (other == vertex || other == candidate ||
            dominators[vertex][other] == 0U) {
          continue;
        }
        if (dominators[candidate][other] == 0U) {
          dominated_by_all_strict_dominators = false;
          break;
        }
      }
      if (dominated_by_all_strict_dominators) {
        REQUIRE(!chosen.has_value());
        chosen = candidate;
      }
    }
    REQUIRE(chosen.has_value());
    immediate[vertex] = chosen;
  }

  return DominatorOracle{std::move(reachable), std::move(dominators),
                         std::move(immediate)};
}

void require_matches_oracle(const Graph& graph, Vertex start) {
  const DominatorOracle oracle = fixed_point_oracle(graph, start);
  const DominatorTree tree(graph, start);
  const DominatorTree repeated(graph, start);
  const std::size_t n = graph.vertex_count();

  REQUIRE_EQ(tree.start(), start);
  REQUIRE_EQ(tree.vertex_count(), n);
  REQUIRE_EQ(tree.dfs_preorder(), repeated.dfs_preorder());
  REQUIRE_EQ(tree.immediate_dominators(), repeated.immediate_dominators());
  REQUIRE_EQ(tree.tree_children(), repeated.tree_children());

  std::vector<unsigned char> seen_dfs(n, 0U);
  for (std::size_t number = 0U; number < tree.dfs_preorder().size(); ++number) {
    const Vertex vertex = tree.dfs_preorder()[number];
    REQUIRE(vertex < n);
    REQUIRE(seen_dfs[vertex] == 0U);
    seen_dfs[vertex] = 1U;
    REQUIRE_EQ(tree.dfs_number(vertex), std::optional<std::size_t>(number));
  }

  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    REQUIRE_EQ(tree.reachable(vertex), oracle.reachable[vertex] != 0U);
    REQUIRE_EQ(tree.immediate_dominator(vertex), oracle.immediate_dominator[vertex]);
    if (oracle.reachable[vertex] == 0U) {
      REQUIRE(!tree.dfs_number(vertex).has_value());
    }

    if (vertex == start || oracle.reachable[vertex] == 0U) {
      REQUIRE(!tree.immediate_dominator(vertex).has_value());
    } else {
      const Vertex parent = *tree.immediate_dominator(vertex);
      const auto& siblings = tree.tree_children()[parent];
      REQUIRE(std::find(siblings.begin(), siblings.end(), vertex) != siblings.end());
    }

    for (Vertex candidate = 0U; candidate < n; ++candidate) {
      const bool expected = oracle.reachable[vertex] != 0U &&
                            oracle.reachable[candidate] != 0U &&
                            oracle.dominators[vertex][candidate] != 0U;
      REQUIRE_EQ(tree.dominates(candidate, vertex), expected);
    }
  }
}

Graph make_graph(std::size_t vertex_count,
                 const std::vector<std::pair<Vertex, Vertex>>& edges) {
  Graph graph(vertex_count, true);
  for (const auto& [from, to] : edges) {
    graph.add_edge(from, to);
  }
  return graph;
}

}  // namespace

TEST_CASE(dominator_tree_validates_domain_and_singleton) {
  Graph empty(0U, true);
  REQUIRE_THROWS_AS(DominatorTree(empty, 0U), std::out_of_range);

  Graph undirected(2U, false);
  undirected.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(DominatorTree(undirected, 0U), std::invalid_argument);

  Graph singleton(1U, true);
  singleton.add_edge(0U, 0U);
  const DominatorTree tree(singleton, 0U);
  REQUIRE(tree.reachable(0U));
  REQUIRE_EQ(tree.dfs_preorder(), std::vector<Vertex>({0U}));
  REQUIRE_EQ(tree.dfs_number(0U), std::optional<std::size_t>(0U));
  REQUIRE(!tree.immediate_dominator(0U).has_value());
  REQUIRE(tree.dominates(0U, 0U));
  REQUIRE_THROWS_AS(tree.reachable(1U), std::out_of_range);
  REQUIRE_THROWS_AS(tree.dfs_number(1U), std::out_of_range);
  REQUIRE_THROWS_AS(tree.immediate_dominator(1U), std::out_of_range);
  REQUIRE_THROWS_AS(tree.dominates(0U, 1U), std::out_of_range);
}

TEST_CASE(dominator_tree_chain_diamond_and_unreachable) {
  Graph graph = make_graph(
      7U, {{0U, 1U}, {1U, 2U}, {2U, 3U}, {0U, 4U}, {4U, 3U},
           {3U, 5U}, {5U, 3U}});
  graph.add_edge(1U, 2U);
  graph.add_edge(3U, 3U);

  const DominatorTree tree(graph, 0U);
  REQUIRE_EQ(tree.dfs_preorder(), std::vector<Vertex>({0U, 1U, 2U, 3U, 5U, 4U}));
  REQUIRE_EQ(tree.immediate_dominator(1U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(2U), std::optional<Vertex>(1U));
  REQUIRE_EQ(tree.immediate_dominator(3U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(4U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(5U), std::optional<Vertex>(3U));
  REQUIRE(!tree.reachable(6U));
  REQUIRE(!tree.immediate_dominator(6U).has_value());
  REQUIRE(!tree.dominates(6U, 6U));
  REQUIRE(!tree.dominates(0U, 6U));
  REQUIRE(tree.dominates(0U, 5U));
  REQUIRE(tree.dominates(3U, 5U));
  REQUIRE(!tree.dominates(1U, 3U));

  require_matches_oracle(graph, 0U);
}

TEST_CASE(dominator_tree_irreducible_merge_shape) {
  Graph graph = make_graph(
      6U, {{0U, 1U}, {0U, 2U}, {1U, 3U}, {2U, 3U}, {3U, 1U},
           {3U, 2U}, {1U, 4U}, {2U, 4U}, {4U, 5U}});
  const DominatorTree tree(graph, 0U);
  REQUIRE_EQ(tree.immediate_dominator(1U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(2U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(3U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(4U), std::optional<Vertex>(0U));
  REQUIRE_EQ(tree.immediate_dominator(5U), std::optional<Vertex>(4U));
  require_matches_oracle(graph, 0U);
}

TEST_CASE(dominator_tree_start_can_be_inside_larger_graph) {
  Graph graph = make_graph(
      7U, {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 2U},
           {3U, 5U}, {5U, 6U}, {6U, 5U}});
  require_matches_oracle(graph, 2U);
  const DominatorTree tree(graph, 2U);
  REQUIRE(!tree.reachable(0U));
  REQUIRE(!tree.reachable(1U));
  REQUIRE_EQ(tree.immediate_dominator(3U), std::optional<Vertex>(2U));
  REQUIRE_EQ(tree.immediate_dominator(4U), std::optional<Vertex>(3U));
  REQUIRE_EQ(tree.immediate_dominator(5U), std::optional<Vertex>(3U));
  REQUIRE_EQ(tree.immediate_dominator(6U), std::optional<Vertex>(5U));
}

TEST_CASE(dominator_tree_randomized_fixed_point_differential) {
  std::mt19937_64 rng(0xD0A11A70ULL);
  for (std::size_t trial = 0U; trial < 600U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 18U);
    Graph graph(n, true);

    const std::size_t edge_attempts = static_cast<std::size_t>(rng() % (n * n + 1U));
    for (std::size_t edge_index = 0U; edge_index < edge_attempts; ++edge_index) {
      const Vertex from = static_cast<Vertex>(rng() % n);
      const Vertex to = static_cast<Vertex>(rng() % n);
      const std::int64_t ignored_weight = static_cast<std::int64_t>(rng() % 101U) - 50;
      graph.add_edge(from, to, ignored_weight);
      if ((rng() & 7U) == 0U) {
        graph.add_edge(from, to, ignored_weight + 1);
      }
    }

    const Vertex start = static_cast<Vertex>(rng() % n);
    require_matches_oracle(graph, start);
  }
}
