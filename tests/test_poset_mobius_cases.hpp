#pragma once

#include "algorithms/combinatorial/poset_mobius.hpp"
#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::combinatorial::FinitePosetIndex;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

std::vector<std::vector<unsigned char>> naive_poset_closure(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<unsigned char>> closure(
      n, std::vector<unsigned char>(n, 0));
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    closure[vertex][vertex] = 1;
    for (const auto& edge : graph.neighbors(vertex)) {
      closure[vertex][edge.to] = 1;
    }
  }
  for (Vertex middle = 0; middle < n; ++middle) {
    for (Vertex from = 0; from < n; ++from) {
      if (closure[from][middle] == 0) {
        continue;
      }
      for (Vertex to = 0; to < n; ++to) {
        if (closure[middle][to] != 0) {
          closure[from][to] = 1;
        }
      }
    }
  }
  return closure;
}

std::vector<std::int64_t> naive_zeta(
    const std::vector<std::vector<unsigned char>>& closure,
    const std::vector<std::int64_t>& values) {
  std::vector<std::int64_t> result(values.size(), 0);
  for (Vertex upper = 0; upper < values.size(); ++upper) {
    for (Vertex lower = 0; lower < values.size(); ++lower) {
      if (closure[lower][upper] != 0) {
        result[upper] += values[lower];
      }
    }
  }
  return result;
}

void require_linear_extension(
    const std::vector<std::vector<unsigned char>>& closure,
    const std::vector<Vertex>& order) {
  std::vector<std::size_t> position(order.size(), 0);
  for (std::size_t index = 0; index < order.size(); ++index) {
    position[order[index]] = index;
  }
  for (Vertex lower = 0; lower < order.size(); ++lower) {
    for (Vertex upper = 0; upper < order.size(); ++upper) {
      if (lower != upper && closure[lower][upper] != 0) {
        REQUIRE(position[lower] < position[upper]);
      }
    }
  }
}

TEST_CASE(poset_mobius_validation_and_basic_semantics) {
  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(FinitePosetIndex(undirected), std::invalid_argument);

  Graph self_loop(1, true);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(FinitePosetIndex(self_loop), std::invalid_argument);

  Graph cycle(3, true);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(FinitePosetIndex(cycle), std::invalid_argument);

  Graph empty(0, true);
  const FinitePosetIndex empty_index(empty);
  REQUIRE(empty_index.linear_extension().empty());
  REQUIRE(empty_index.zeta_transform({}).empty());
  REQUIRE(empty_index.mobius_invert({}).empty());
  REQUIRE_THROWS_AS(empty_index.less_equal(0, 0), std::out_of_range);

  Graph diamond(4, true);
  diamond.add_edge(0, 2, 9);
  diamond.add_edge(0, 1, -7);
  diamond.add_edge(0, 1, 42);
  diamond.add_edge(1, 3, 5);
  diamond.add_edge(2, 3, -11);
  const FinitePosetIndex index(diamond);
  REQUIRE_EQ(index.linear_extension(), (std::vector<Vertex>{0, 1, 2, 3}));
  REQUIRE(index.less_equal(0, 3));
  REQUIRE(!index.less_equal(1, 2));
  REQUIRE_THROWS_AS(index.less_equal(4, 0), std::out_of_range);

  const std::vector<std::int64_t> values{1, 2, 3, 4};
  const auto transformed = index.zeta_transform(values);
  REQUIRE_EQ(transformed, (std::vector<std::int64_t>{1, 3, 4, 10}));
  REQUIRE_EQ(index.mobius_invert(transformed), values);
  REQUIRE_THROWS_AS(index.zeta_transform(std::vector<std::int64_t>{1, 2}),
                    std::invalid_argument);
}

TEST_CASE(poset_mobius_exact_cancellation_and_overflow) {
  Graph fork(3, true);
  fork.add_edge(0, 2);
  fork.add_edge(1, 2);
  const FinitePosetIndex index(fork);

  const std::vector<std::int64_t> values{
      std::numeric_limits<std::int64_t>::max(),
      std::numeric_limits<std::int64_t>::min(), 5};
  const auto transformed = index.zeta_transform(values);
  REQUIRE_EQ(transformed[0], std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(transformed[1], std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(transformed[2], 4);
  REQUIRE_EQ(index.mobius_invert(transformed), values);

  Graph chain(2, true);
  chain.add_edge(0, 1);
  const FinitePosetIndex chain_index(chain);
  REQUIRE_THROWS_AS(
      chain_index.zeta_transform(std::vector<std::int64_t>{
          std::numeric_limits<std::int64_t>::max(), 1}),
      std::overflow_error);
  REQUIRE_THROWS_AS(
      chain_index.mobius_invert(std::vector<std::int64_t>{
          std::numeric_limits<std::int64_t>::min(),
          std::numeric_limits<std::int64_t>::max()}),
      std::overflow_error);
}

TEST_CASE(poset_mobius_chain_antichain_and_determinism) {
  Graph chain(5, true);
  chain.add_edge(0, 1);
  chain.add_edge(1, 2);
  chain.add_edge(2, 3);
  chain.add_edge(3, 4);
  const FinitePosetIndex chain_index(chain);
  REQUIRE_EQ(chain_index.zeta_transform(std::vector<std::int64_t>{1, 2, 3, 4, 5}),
             (std::vector<std::int64_t>{1, 3, 6, 10, 15}));

  Graph antichain(5, true);
  const FinitePosetIndex first(antichain);
  const FinitePosetIndex second(antichain);
  REQUIRE_EQ(first.linear_extension(), (std::vector<Vertex>{0, 1, 2, 3, 4}));
  REQUIRE_EQ(first.linear_extension(), second.linear_extension());
  const std::vector<std::int64_t> values{-5, 0, 7, 9, -2};
  REQUIRE_EQ(first.zeta_transform(values), values);
  REQUIRE_EQ(first.mobius_invert(values), values);
}

TEST_CASE(poset_mobius_randomized_floyd_warshall_differential) {
  std::mt19937_64 random(0x50E7B1A5ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 12);
  std::uniform_int_distribution<int> value_distribution(-20, 20);
  std::uniform_int_distribution<int> weight_distribution(-1000, 1000);
  std::bernoulli_distribution edge_distribution(0.24);
  std::bernoulli_distribution duplicate_distribution(0.15);

  for (int trial = 0; trial < 900; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_distribution(random));
    std::vector<Vertex> permutation(n);
    std::iota(permutation.begin(), permutation.end(), Vertex{0});
    std::shuffle(permutation.begin(), permutation.end(), random);

    Graph graph(n, true);
    for (std::size_t left = 0; left < n; ++left) {
      for (std::size_t right = left + 1; right < n; ++right) {
        if (!edge_distribution(random)) {
          continue;
        }
        const Vertex from = permutation[left];
        const Vertex to = permutation[right];
        graph.add_edge(from, to, static_cast<std::int64_t>(weight_distribution(random)));
        if (duplicate_distribution(random)) {
          graph.add_edge(from, to,
                         static_cast<std::int64_t>(weight_distribution(random)));
        }
      }
    }

    const auto closure = naive_poset_closure(graph);
    const FinitePosetIndex index(graph);
    require_linear_extension(closure, index.linear_extension());
    for (Vertex lower = 0; lower < n; ++lower) {
      for (Vertex upper = 0; upper < n; ++upper) {
        REQUIRE_EQ(index.less_equal(lower, upper), closure[lower][upper] != 0);
      }
    }

    std::vector<std::int64_t> values(n, 0);
    for (auto& value : values) {
      value = static_cast<std::int64_t>(value_distribution(random));
    }
    const auto expected = naive_zeta(closure, values);
    const auto actual = index.zeta_transform(values);
    REQUIRE_EQ(actual, expected);
    REQUIRE_EQ(index.mobius_invert(actual), values);

    const FinitePosetIndex repeated(graph);
    REQUIRE_EQ(repeated.linear_extension(), index.linear_extension());
    REQUIRE_EQ(repeated.zeta_transform(values), actual);
  }
}

}  // namespace
