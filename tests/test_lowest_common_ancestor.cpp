#include "test_framework.hpp"

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/lowest_common_ancestor.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::LowestCommonAncestor;
using algorithms::graphs::Vertex;

struct NaiveRootedTree {
  std::vector<Vertex> parent;
  std::vector<std::size_t> depth;
};

NaiveRootedTree root_naively(const Graph& tree, Vertex root) {
  const std::size_t n = tree.vertex_count();
  NaiveRootedTree result{std::vector<Vertex>(n, n),
                         std::vector<std::size_t>(n, 0)};
  std::queue<Vertex> queue;
  result.parent[root] = root;
  queue.push(root);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const auto& edge : tree.neighbors(vertex)) {
      if (result.parent[edge.to] != n) {
        continue;
      }
      result.parent[edge.to] = vertex;
      result.depth[edge.to] = result.depth[vertex] + 1;
      queue.push(edge.to);
    }
  }
  return result;
}

Vertex naive_lca(Vertex first, Vertex second, const NaiveRootedTree& tree) {
  while (tree.depth[first] > tree.depth[second]) {
    first = tree.parent[first];
  }
  while (tree.depth[second] > tree.depth[first]) {
    second = tree.parent[second];
  }
  while (first != second) {
    first = tree.parent[first];
    second = tree.parent[second];
  }
  return first;
}

std::optional<Vertex> naive_kth(Vertex vertex, std::size_t steps,
                                const NaiveRootedTree& tree) {
  if (steps > tree.depth[vertex]) {
    return std::nullopt;
  }
  for (std::size_t step = 0; step < steps; ++step) {
    vertex = tree.parent[vertex];
  }
  return vertex;
}

TEST_CASE(lca_deterministic_queries_and_root) {
  Graph tree(8, false);
  tree.add_edge(0, 1);
  tree.add_edge(0, 2);
  tree.add_edge(1, 3);
  tree.add_edge(1, 4);
  tree.add_edge(2, 5);
  tree.add_edge(5, 6);
  tree.add_edge(5, 7);
  const LowestCommonAncestor index(tree, 0);
  REQUIRE_EQ(index.root(), Vertex{0});
  REQUIRE_EQ(index.vertex_count(), std::size_t{8});
  REQUIRE_EQ(index.lca(3, 4), Vertex{1});
  REQUIRE_EQ(index.lca(3, 7), Vertex{0});
  REQUIRE_EQ(index.lca(5, 7), Vertex{5});
  REQUIRE_EQ(index.distance_edges(3, 7), std::size_t{5});
  REQUIRE_EQ(index.depth(6), std::size_t{3});
  REQUIRE_EQ(index.kth_ancestor(6, 0), std::optional<Vertex>{6});
  REQUIRE_EQ(index.kth_ancestor(6, 2), std::optional<Vertex>{2});
  REQUIRE_EQ(index.kth_ancestor(6, 3), std::optional<Vertex>{0});
  REQUIRE_EQ(index.kth_ancestor(6, 4), std::optional<Vertex>{});
  REQUIRE_THROWS_AS(index.lca(0, 8), std::out_of_range);
}

TEST_CASE(lca_rejects_non_tree_inputs) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(LowestCommonAncestor(directed, 0), std::invalid_argument);

  Graph self_loop(1, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(LowestCommonAncestor(self_loop, 0), std::invalid_argument);

  Graph parallel(2, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(LowestCommonAncestor(parallel, 0), std::invalid_argument);

  Graph cycle(3, false);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(LowestCommonAncestor(cycle, 0), std::invalid_argument);

  Graph disconnected(3, false);
  disconnected.add_edge(0, 1);
  REQUIRE_THROWS_AS(LowestCommonAncestor(disconnected, 0), std::invalid_argument);

  Graph singleton(1, false);
  REQUIRE_THROWS_AS(LowestCommonAncestor(singleton, 1), std::out_of_range);
}

TEST_CASE(lca_randomized_naive_differential) {
  std::mt19937_64 rng(0x1CA1CAULL);
  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t n = 1 + static_cast<std::size_t>(rng() % 80U);
    Graph tree(n, false);
    for (Vertex vertex = 1; vertex < n; ++vertex) {
      const Vertex parent = static_cast<Vertex>(rng() % vertex);
      tree.add_edge(parent, vertex, static_cast<std::int64_t>(rng()));
    }
    const Vertex root = static_cast<Vertex>(rng() % n);
    const LowestCommonAncestor index(tree, root);
    const NaiveRootedTree naive = root_naively(tree, root);
    for (std::size_t query = 0; query < 100; ++query) {
      const Vertex first = static_cast<Vertex>(rng() % n);
      const Vertex second = static_cast<Vertex>(rng() % n);
      const Vertex expected = naive_lca(first, second, naive);
      REQUIRE_EQ(index.lca(first, second), expected);
      REQUIRE_EQ(index.depth(first), naive.depth[first]);
      REQUIRE_EQ(index.distance_edges(first, second),
                 (naive.depth[first] - naive.depth[expected]) +
                     (naive.depth[second] - naive.depth[expected]));
      const std::size_t steps = static_cast<std::size_t>(rng() % (n + 5U));
      REQUIRE_EQ(index.kth_ancestor(first, steps),
                 naive_kth(first, steps, naive));
    }
  }
}

}  // namespace
