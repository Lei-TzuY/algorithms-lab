#pragma once

#include "algorithms/graphs/dsu_on_tree_frequency.hpp"
#include "algorithms/graphs/graph.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::DsuOnTreeFrequencyIndex;
using algorithms::graphs::Graph;
using algorithms::graphs::SubtreeFrequencySummary;
using algorithms::graphs::Vertex;

struct NaiveRooting {
  std::vector<std::optional<Vertex>> parent;
  std::vector<std::vector<Vertex>> children;
  std::vector<std::size_t> subtree_size;
};

NaiveRooting naive_root_tree(const Graph& tree, Vertex root) {
  const std::size_t n = tree.vertex_count();
  NaiveRooting result{std::vector<std::optional<Vertex>>(n),
                      std::vector<std::vector<Vertex>>(n),
                      std::vector<std::size_t>(n, 1)};
  std::vector<bool> seen(n, false);
  std::vector<Vertex> order;
  order.reserve(n);
  std::vector<Vertex> stack{root};
  seen[root] = true;
  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    order.push_back(vertex);
    for (const auto& edge : tree.neighbors(vertex)) {
      if (!seen[edge.to]) {
        seen[edge.to] = true;
        result.parent[edge.to] = vertex;
        result.children[vertex].push_back(edge.to);
        stack.push_back(edge.to);
      }
    }
  }
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (result.parent[*it].has_value()) {
      result.subtree_size[*result.parent[*it]] += result.subtree_size[*it];
    }
  }
  return result;
}

SubtreeFrequencySummary naive_summary(const NaiveRooting& rooting,
                                      const std::vector<std::int64_t>& labels,
                                      Vertex vertex) {
  std::map<std::int64_t, std::size_t> frequencies;
  std::vector<Vertex> stack{vertex};
  std::size_t count = 0;
  while (!stack.empty()) {
    const Vertex current = stack.back();
    stack.pop_back();
    ++frequencies[labels[current]];
    ++count;
    for (const Vertex child : rooting.children[current]) {
      stack.push_back(child);
    }
  }

  std::size_t max_frequency = 0;
  std::int64_t mode = 0;
  for (const auto& [label, frequency] : frequencies) {
    if (frequency > max_frequency) {
      max_frequency = frequency;
      mode = label;
    }
  }
  return SubtreeFrequencySummary{count, frequencies.size(), max_frequency, mode};
}

std::optional<Vertex> expected_heavy_child(const NaiveRooting& rooting,
                                           Vertex vertex) {
  std::optional<Vertex> best;
  for (const Vertex child : rooting.children[vertex]) {
    if (!best.has_value() ||
        rooting.subtree_size[child] > rooting.subtree_size[*best] ||
        (rooting.subtree_size[child] == rooting.subtree_size[*best] &&
         child < *best)) {
      best = child;
    }
  }
  return best;
}

void verify_against_naive(const Graph& tree,
                          const std::vector<std::int64_t>& labels,
                          Vertex root) {
  const DsuOnTreeFrequencyIndex index(tree, labels, root);
  const NaiveRooting naive = naive_root_tree(tree, root);
  REQUIRE_EQ(index.size(), tree.vertex_count());
  REQUIRE_EQ(index.root(), root);
  for (Vertex vertex = 0; vertex < tree.vertex_count(); ++vertex) {
    REQUIRE_EQ(index.parent(vertex), naive.parent[vertex]);
    REQUIRE_EQ(index.subtree_size(vertex), naive.subtree_size[vertex]);
    REQUIRE_EQ(index.heavy_child(vertex), expected_heavy_child(naive, vertex));
    REQUIRE_EQ(index.summary(vertex), naive_summary(naive, labels, vertex));
  }
  REQUIRE(index.add_operations() <= index.amortized_add_bound());
}

TEST_CASE(dsu_on_tree_frequency_singleton_and_ties) {
  Graph singleton(1, false);
  const DsuOnTreeFrequencyIndex one(singleton, {-7}, 0);
  REQUIRE_EQ(one.summary(0), (SubtreeFrequencySummary{1, 1, 1, -7}));
  REQUIRE(!one.parent(0).has_value());
  REQUIRE(!one.heavy_child(0).has_value());
  REQUIRE_EQ(one.add_operations(), std::size_t{1});
  REQUIRE_EQ(one.remove_operations(), std::size_t{0});

  Graph tree(7, false);
  tree.add_edge(0, 1, 99);
  tree.add_edge(0, 2, -99);
  tree.add_edge(1, 3, 5);
  tree.add_edge(1, 4, 6);
  tree.add_edge(2, 5, 7);
  tree.add_edge(2, 6, 8);
  const std::vector<std::int64_t> labels{
      5, -4, 5, std::numeric_limits<std::int64_t>::min(), -4,
      std::numeric_limits<std::int64_t>::max(), 5};
  verify_against_naive(tree, labels, 0);
  const DsuOnTreeFrequencyIndex index(tree, labels, 0);
  REQUIRE_EQ(index.heavy_child(0), std::optional<Vertex>{1});
  REQUIRE_EQ(index.summary(0).max_frequency, std::size_t{3});
  REQUIRE_EQ(index.summary(0).mode_label, std::int64_t{5});
  REQUIRE_EQ(index.summary(1).max_frequency, std::size_t{2});
  REQUIRE_EQ(index.summary(1).mode_label, std::int64_t{-4});
}

TEST_CASE(dsu_on_tree_frequency_rejects_non_trees_and_bad_arguments) {
  Graph empty(0, false);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(empty, {}, 0), std::invalid_argument);

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(directed, {1, 2}, 0),
                    std::invalid_argument);

  Graph self_loop(1, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(self_loop, {1}, 0),
                    std::invalid_argument);

  Graph parallel(2, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(parallel, {1, 2}, 0),
                    std::invalid_argument);

  Graph cycle(3, false);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(cycle, {1, 2, 3}, 0),
                    std::invalid_argument);

  Graph disconnected(3, false);
  disconnected.add_edge(0, 1);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(disconnected, {1, 2, 3}, 0),
                    std::invalid_argument);

  Graph valid(2, false);
  valid.add_edge(0, 1);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(valid, {1}, 0),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(DsuOnTreeFrequencyIndex(valid, {1, 2}, 2),
                    std::out_of_range);

  DsuOnTreeFrequencyIndex index(valid, {1, 2}, 0);
  REQUIRE_THROWS_AS(index.summary(2), std::out_of_range);
  REQUIRE_THROWS_AS(index.parent(2), std::out_of_range);
  REQUIRE_THROWS_AS(index.heavy_child(2), std::out_of_range);
  REQUIRE_THROWS_AS(index.subtree_size(2), std::out_of_range);
}

TEST_CASE(dsu_on_tree_frequency_deep_chain_is_iterative) {
  constexpr std::size_t kSize = 4096;
  Graph chain(kSize, false);
  std::vector<std::int64_t> labels(kSize);
  for (std::size_t vertex = 0; vertex < kSize; ++vertex) {
    labels[vertex] = static_cast<std::int64_t>(vertex % 11U) - 5;
    if (vertex != 0) {
      chain.add_edge(vertex - 1, vertex, static_cast<std::int64_t>(vertex));
    }
  }
  const DsuOnTreeFrequencyIndex index(chain, labels, 0);
  REQUIRE_EQ(index.subtree_size(0), kSize);
  REQUIRE_EQ(index.summary(kSize - 1).subtree_size, std::size_t{1});
  REQUIRE_EQ(index.add_operations(), kSize);
  REQUIRE_EQ(index.remove_operations(), std::size_t{0});
  REQUIRE(index.add_operations() <= index.amortized_add_bound());
}

TEST_CASE(dsu_on_tree_frequency_randomized_differential) {
  std::mt19937_64 rng(0xD5A0F123ULL);
  std::uniform_int_distribution<std::size_t> size_distribution(1, 128);
  std::uniform_int_distribution<int> label_distribution(-8, 8);
  std::uniform_int_distribution<int> weight_distribution(-100, 100);

  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t n = size_distribution(rng);
    Graph tree(n, false);
    for (Vertex vertex = 1; vertex < n; ++vertex) {
      std::uniform_int_distribution<Vertex> parent_distribution(0, vertex - 1);
      tree.add_edge(parent_distribution(rng), vertex,
                    static_cast<std::int64_t>(weight_distribution(rng)));
    }
    std::vector<std::int64_t> labels(n);
    for (auto& label : labels) {
      label = static_cast<std::int64_t>(label_distribution(rng));
    }
    std::uniform_int_distribution<Vertex> root_distribution(0, n - 1);
    verify_against_naive(tree, labels, root_distribution(rng));
  }
}

}  // namespace
