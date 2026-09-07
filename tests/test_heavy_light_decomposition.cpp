#include "algorithms/graphs/heavy_light_decomposition.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::graphs::Graph;
using algorithms::graphs::HeavyLightDecomposition;
using algorithms::graphs::Vertex;

namespace {

std::vector<Vertex> rooted_parent(const Graph& tree, Vertex root) {
  const std::size_t vertex_count = tree.vertex_count();
  std::vector<Vertex> parent(vertex_count, vertex_count);
  std::queue<Vertex> pending;
  parent[root] = root;
  pending.push(root);
  while (!pending.empty()) {
    const Vertex vertex = pending.front();
    pending.pop();
    for (const auto& edge : tree.neighbors(vertex)) {
      if (parent[edge.to] != vertex_count) {
        continue;
      }
      parent[edge.to] = vertex;
      pending.push(edge.to);
    }
  }
  return parent;
}

std::int64_t naive_path_sum(const Graph& tree,
                            const std::vector<std::int64_t>& values,
                            Vertex source, Vertex target) {
  const std::size_t vertex_count = tree.vertex_count();
  std::vector<Vertex> parent(vertex_count, vertex_count);
  std::queue<Vertex> pending;
  parent[source] = source;
  pending.push(source);
  while (!pending.empty() && parent[target] == vertex_count) {
    const Vertex vertex = pending.front();
    pending.pop();
    for (const auto& edge : tree.neighbors(vertex)) {
      if (parent[edge.to] != vertex_count) {
        continue;
      }
      parent[edge.to] = vertex;
      pending.push(edge.to);
    }
  }

  std::int64_t sum = 0;
  Vertex vertex = target;
  while (vertex != source) {
    sum += values[vertex];
    vertex = parent[vertex];
  }
  return sum + values[source];
}

bool is_descendant(Vertex candidate, Vertex ancestor,
                   const std::vector<Vertex>& parent) {
  Vertex current = candidate;
  while (true) {
    if (current == ancestor) {
      return true;
    }
    if (parent[current] == current) {
      return false;
    }
    current = parent[current];
  }
}

std::int64_t naive_subtree_sum(Vertex root,
                               const std::vector<std::int64_t>& values,
                               const std::vector<Vertex>& parent) {
  std::int64_t sum = 0;
  for (Vertex vertex = 0; vertex < values.size(); ++vertex) {
    if (is_descendant(vertex, root, parent)) {
      sum += values[vertex];
    }
  }
  return sum;
}

}  // namespace

TEST_CASE(heavy_light_path_subtree_updates_and_rerooting) {
  Graph tree(6U, false);
  tree.add_edge(0U, 1U);
  tree.add_edge(0U, 2U);
  tree.add_edge(1U, 3U);
  tree.add_edge(1U, 4U);
  tree.add_edge(2U, 5U);
  std::vector<std::int64_t> values{5, -2, 3, 7, 1, 4};

  HeavyLightDecomposition hld(tree, values, 0U);
  REQUIRE_EQ(hld.vertex_count(), 6U);
  REQUIRE_EQ(hld.root(), 0U);
  REQUIRE_EQ(hld.path_sum(3U, 5U), 17);
  REQUIRE_EQ(hld.path_sum(4U, 4U), 1);
  REQUIRE_EQ(hld.subtree_sum(1U), 6);
  REQUIRE_EQ(hld.subtree_sum(0U), 18);

  hld.assign(1U, 10);
  values[1U] = 10;
  REQUIRE_EQ(hld.path_sum(3U, 5U), 29);
  REQUIRE_EQ(hld.subtree_sum(1U), 18);

  HeavyLightDecomposition rerooted(tree, values, 5U);
  const auto parent = rooted_parent(tree, 5U);
  for (Vertex vertex = 0; vertex < tree.vertex_count(); ++vertex) {
    REQUIRE_EQ(rerooted.subtree_sum(vertex),
               naive_subtree_sum(vertex, values, parent));
  }
}

TEST_CASE(heavy_light_validation_and_checked_sum_boundaries) {
  Graph singleton(1U, false);
  HeavyLightDecomposition one(singleton, std::vector<std::int64_t>{9}, 0U);
  REQUIRE_EQ(one.path_sum(0U, 0U), 9);
  REQUIRE_THROWS_AS(one.path_sum(0U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(one.assign(1U, 0), std::out_of_range);

  Graph empty(0U, false);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(empty, std::vector<std::int64_t>{}, 0U),
      std::invalid_argument);

  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(directed, std::vector<std::int64_t>{1, 2}, 0U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(singleton, std::vector<std::int64_t>{}, 0U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(singleton, std::vector<std::int64_t>{1}, 1U),
      std::out_of_range);

  Graph self_loop(1U, false);
  self_loop.add_edge(0U, 0U);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(self_loop, std::vector<std::int64_t>{1}, 0U),
      std::invalid_argument);

  Graph parallel(2U, false);
  parallel.add_edge(0U, 1U);
  parallel.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(parallel, std::vector<std::int64_t>{1, 2}, 0U),
      std::invalid_argument);

  Graph cycle(3U, false);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 0U);
  REQUIRE_THROWS_AS(
      HeavyLightDecomposition(cycle, std::vector<std::int64_t>{1, 2, 3}, 0U),
      std::invalid_argument);

  Graph disconnected(3U, false);
  disconnected.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(HeavyLightDecomposition(
                        disconnected, std::vector<std::int64_t>{1, 2, 3}, 0U),
                    std::invalid_argument);

  Graph two(2U, false);
  two.add_edge(0U, 1U);
  const auto maximum = std::numeric_limits<std::int64_t>::max();
  REQUIRE_THROWS_AS(HeavyLightDecomposition(
                        two, std::vector<std::int64_t>{maximum, 1}, 0U),
                    std::overflow_error);

  HeavyLightDecomposition transactional(
      two, std::vector<std::int64_t>{maximum, 0}, 0U);
  REQUIRE_THROWS_AS(transactional.assign(1U, 1), std::overflow_error);
  REQUIRE_EQ(transactional.path_sum(0U, 1U), maximum);

  Graph star(4U, false);
  star.add_edge(0U, 1U);
  star.add_edge(0U, 2U);
  star.add_edge(0U, 3U);
  const auto minimum = std::numeric_limits<std::int64_t>::min();
  HeavyLightDecomposition path_overflow(
      star, std::vector<std::int64_t>{0, maximum, 1, minimum}, 0U);
  REQUIRE_THROWS_AS(path_overflow.path_sum(1U, 2U), std::overflow_error);
}

TEST_CASE(heavy_light_randomized_against_naive_tree_oracle) {
  std::mt19937_64 rng(0x71DCAFEULL);
  std::uniform_int_distribution<std::size_t> size_dist(1U, 45U);
  std::uniform_int_distribution<std::int64_t> value_dist(-50, 50);
  std::uniform_int_distribution<int> operation_dist(0, 2);

  for (std::size_t trial = 0; trial < 300U; ++trial) {
    const std::size_t vertex_count = size_dist(rng);
    Graph tree(vertex_count, false);
    for (Vertex vertex = 1U; vertex < vertex_count; ++vertex) {
      std::uniform_int_distribution<std::size_t> parent_dist(0U, vertex - 1U);
      tree.add_edge(vertex, parent_dist(rng));
    }

    std::uniform_int_distribution<std::size_t> vertex_dist(
        0U, vertex_count - 1U);
    const Vertex root = vertex_dist(rng);
    std::vector<std::int64_t> values(vertex_count, 0);
    for (auto& value : values) {
      value = value_dist(rng);
    }

    HeavyLightDecomposition hld(tree, values, root);
    const auto parent = rooted_parent(tree, root);

    for (std::size_t round = 0; round < 120U; ++round) {
      const int operation = operation_dist(rng);
      if (operation == 0) {
        const Vertex vertex = vertex_dist(rng);
        const std::int64_t value = value_dist(rng);
        hld.assign(vertex, value);
        values[vertex] = value;
      } else if (operation == 1) {
        const Vertex first = vertex_dist(rng);
        const Vertex second = vertex_dist(rng);
        REQUIRE_EQ(hld.path_sum(first, second),
                   naive_path_sum(tree, values, first, second));
      } else {
        const Vertex vertex = vertex_dist(rng);
        REQUIRE_EQ(hld.subtree_sum(vertex),
                   naive_subtree_sum(vertex, values, parent));
      }
    }
  }
}
