#pragma once

#include "algorithms/graphs/linear_extension_count.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace linear_extension_count_tests {

using algorithms::graphs::Graph;
using algorithms::graphs::LinearExtensionCountResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::count_dag_linear_extensions;

struct OracleResult {
  std::uint64_t count{};
  std::vector<Vertex> lexicographically_smallest_order;
};

[[nodiscard]] inline OracleResult permutation_oracle(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<unsigned char> relation(n * n, 0);
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      relation[from * n + edge.to] = 1;
    }
  }

  std::vector<Vertex> permutation(n);
  std::iota(permutation.begin(), permutation.end(), Vertex{0});
  std::uint64_t count = 0;
  std::vector<Vertex> first;
  do {
    std::vector<std::size_t> position(n, 0);
    for (std::size_t index = 0; index < n; ++index) {
      position[permutation[index]] = index;
    }
    bool valid = true;
    for (Vertex from = 0; from < n && valid; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if (relation[from * n + to] != 0 && position[from] >= position[to]) {
          valid = false;
          break;
        }
      }
    }
    if (valid) {
      if (count == 0) {
        first = permutation;
      }
      ++count;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return OracleResult{count, std::move(first)};
}

inline void require_matches_oracle(const Graph& graph) {
  const OracleResult oracle = permutation_oracle(graph);
  if (oracle.count == 0) {
    REQUIRE_THROWS_AS(count_dag_linear_extensions(graph), std::invalid_argument);
    return;
  }
  const LinearExtensionCountResult result = count_dag_linear_extensions(graph);
  REQUIRE_EQ(result.count, oracle.count);
  REQUIRE_EQ(result.vertex_count, graph.vertex_count());
  REQUIRE(result.ideals_evaluated >= 1);
  REQUIRE_EQ(result.lexicographically_smallest_order,
             oracle.lexicographically_smallest_order);
  REQUIRE(result == count_dag_linear_extensions(graph));
}

inline void shuffle_vertices(std::vector<Vertex>& values, std::mt19937_64& rng) {
  for (std::size_t i = values.size(); i > 1; --i) {
    const std::size_t j = static_cast<std::size_t>(rng() % i);
    std::swap(values[i - 1], values[j]);
  }
}

}  // namespace linear_extension_count_tests

TEST_CASE(linear_extension_count_deterministic_shapes) {
  using namespace linear_extension_count_tests;

  Graph empty(0, true);
  const auto empty_result = count_dag_linear_extensions(empty);
  REQUIRE_EQ(empty_result.count, 1ULL);
  REQUIRE_EQ(empty_result.ideals_evaluated, 1ULL);
  REQUIRE(empty_result.lexicographically_smallest_order.empty());

  Graph edgeless(4, true);
  const auto edgeless_result = count_dag_linear_extensions(edgeless);
  REQUIRE_EQ(edgeless_result.count, 24ULL);
  REQUIRE_EQ(edgeless_result.ideals_evaluated, 16ULL);
  REQUIRE_EQ(edgeless_result.lexicographically_smallest_order,
             (std::vector<Vertex>{0, 1, 2, 3}));

  Graph chain(4, true);
  chain.add_edge(0, 1);
  chain.add_edge(1, 2);
  chain.add_edge(2, 3);
  const auto chain_result = count_dag_linear_extensions(chain);
  REQUIRE_EQ(chain_result.count, 1ULL);
  REQUIRE_EQ(chain_result.ideals_evaluated, 5ULL);

  Graph diamond(4, true);
  diamond.add_edge(0, 1);
  diamond.add_edge(0, 2);
  diamond.add_edge(1, 3);
  diamond.add_edge(2, 3);
  REQUIRE_EQ(count_dag_linear_extensions(diamond).count, 2ULL);

  Graph two_chains(5, true);
  two_chains.add_edge(0, 1);
  two_chains.add_edge(1, 2);
  two_chains.add_edge(3, 4);
  REQUIRE_EQ(count_dag_linear_extensions(two_chains).count, 10ULL);
}

TEST_CASE(linear_extension_count_multigraph_and_rejections) {
  using namespace linear_extension_count_tests;

  Graph parallel(3, true);
  parallel.add_edge(0, 1, -9);
  parallel.add_edge(0, 1, 41);
  parallel.add_edge(0, 1, 0);
  REQUIRE_EQ(count_dag_linear_extensions(parallel).count, 3ULL);

  Graph self_loop(2, true);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(count_dag_linear_extensions(self_loop), std::invalid_argument);

  Graph cycle(3, true);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(count_dag_linear_extensions(cycle), std::invalid_argument);

  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(count_dag_linear_extensions(undirected), std::invalid_argument);

  Graph too_large(21, true);
  REQUIRE_THROWS_AS(count_dag_linear_extensions(too_large), std::length_error);
}

TEST_CASE(linear_extension_count_exact_uint64_boundary) {
  using namespace linear_extension_count_tests;

  Graph edgeless(20, true);
  const auto result = count_dag_linear_extensions(edgeless);
  REQUIRE_EQ(result.count, 2432902008176640000ULL);
  REQUIRE_EQ(result.ideals_evaluated, 1048576ULL);
  REQUIRE_EQ(result.lexicographically_smallest_order.front(), Vertex{0});
  REQUIRE_EQ(result.lexicographically_smallest_order.back(), Vertex{19});
}

TEST_CASE(linear_extension_count_randomized_permutation_differential) {
  using namespace linear_extension_count_tests;

  std::mt19937_64 rng(0x11EAE57E7C0A17ULL);

  for (std::size_t trial = 0; trial < 350; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 9ULL);
    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if ((rng() % 7ULL) == 0ULL) {
          graph.add_edge(from, to, static_cast<std::int64_t>(rng() % 101ULL) - 50);
          if ((rng() % 5ULL) == 0ULL) {
            graph.add_edge(from, to, static_cast<std::int64_t>(rng() % 101ULL) - 50);
          }
        }
      }
    }
    require_matches_oracle(graph);
  }

  for (std::size_t trial = 0; trial < 350; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 9ULL);
    std::vector<Vertex> hidden_order(n);
    std::iota(hidden_order.begin(), hidden_order.end(), Vertex{0});
    shuffle_vertices(hidden_order, rng);

    Graph graph(n, true);
    for (std::size_t left = 0; left < n; ++left) {
      for (std::size_t right = left + 1; right < n; ++right) {
        if ((rng() % 4ULL) == 0ULL) {
          const Vertex from = hidden_order[left];
          const Vertex to = hidden_order[right];
          graph.add_edge(from, to, static_cast<std::int64_t>(rng() % 101ULL) - 50);
          if ((rng() % 6ULL) == 0ULL) {
            graph.add_edge(from, to, static_cast<std::int64_t>(rng() % 101ULL) - 50);
          }
        }
      }
    }
    require_matches_oracle(graph);
  }
}
