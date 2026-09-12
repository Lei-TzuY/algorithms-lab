#pragma once

#include "algorithms/graphs/spanning_tree_count.hpp"
#include "test_framework.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace spanning_tree_count_tests {

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct LogicalEdge {
  Vertex first;
  Vertex second;
  std::int64_t weight;
};

[[nodiscard]] inline Graph make_graph(
    std::size_t vertex_count, const std::vector<LogicalEdge>& edges) {
  Graph graph(vertex_count, false);
  for (const LogicalEdge& edge : edges) {
    graph.add_edge(edge.first, edge.second, edge.weight);
  }
  return graph;
}

[[nodiscard]] inline std::uint64_t exhaustive_tree_count_mod(
    std::size_t vertex_count, const std::vector<LogicalEdge>& edges,
    std::uint64_t modulus) {
  if (vertex_count == 0U) {
    return 0U;
  }
  if (vertex_count == 1U) {
    return 1U % modulus;
  }

  std::vector<std::pair<Vertex, Vertex>> usable;
  for (const LogicalEdge& edge : edges) {
    if (edge.first != edge.second) {
      usable.emplace_back(edge.first, edge.second);
    }
  }
  if (usable.size() >= 63U) {
    throw std::length_error("exhaustive spanning-tree oracle edge cap exceeded");
  }

  std::uint64_t count = 0U;
  const std::uint64_t subset_count = std::uint64_t{1} << usable.size();
  for (std::uint64_t mask = 0U; mask < subset_count; ++mask) {
    if (std::popcount(mask) != static_cast<int>(vertex_count - 1U)) {
      continue;
    }

    std::vector<std::vector<Vertex>> adjacency(vertex_count);
    for (std::size_t index = 0; index < usable.size(); ++index) {
      if ((mask & (std::uint64_t{1} << index)) == 0U) {
        continue;
      }
      const auto [first, second] = usable[index];
      adjacency[first].push_back(second);
      adjacency[second].push_back(first);
    }

    std::vector<bool> seen(vertex_count, false);
    std::vector<Vertex> stack{0U};
    seen[0] = true;
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      for (const Vertex next : adjacency[vertex]) {
        if (!seen[next]) {
          seen[next] = true;
          stack.push_back(next);
        }
      }
    }

    bool connected = true;
    for (const bool visited : seen) {
      if (!visited) {
        connected = false;
        break;
      }
    }
    if (connected) {
      ++count;
      if (count == modulus) {
        count = 0U;
      }
    }
  }
  return count;
}

}  // namespace spanning_tree_count_tests

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

TEST_CASE(spanning_tree_count_validates_contract_and_trivial_graphs) {
  using algorithms::graphs::spanning_tree_count_mod_prime;

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(spanning_tree_count_mod_prime(directed, 17U),
                    std::invalid_argument);

  Graph empty(0, false);
  REQUIRE_EQ(spanning_tree_count_mod_prime(empty, 17U), std::uint64_t{0});
  REQUIRE_THROWS_AS(spanning_tree_count_mod_prime(empty, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(spanning_tree_count_mod_prime(empty, 21U),
                    std::invalid_argument);

  Graph singleton(1, false);
  singleton.add_edge(0, 0, -99);
  REQUIRE_EQ(spanning_tree_count_mod_prime(singleton, 2U), std::uint64_t{1});
  REQUIRE_EQ(spanning_tree_count_mod_prime(singleton, 97U), std::uint64_t{1});
}

TEST_CASE(spanning_tree_count_handles_parallel_edges_loops_and_ignored_weights) {
  using algorithms::graphs::spanning_tree_count_mod_prime;

  Graph graph(2, false);
  graph.add_edge(0, 1, -1000);
  graph.add_edge(0, 1, 0);
  graph.add_edge(0, 1, 7);
  graph.add_edge(0, 1, 99);
  graph.add_edge(0, 1, 123456);
  graph.add_edge(0, 0, 5);
  graph.add_edge(1, 1, -8);

  REQUIRE_EQ(spanning_tree_count_mod_prime(graph, 97U), std::uint64_t{5});
  REQUIRE_EQ(spanning_tree_count_mod_prime(graph, 3U), std::uint64_t{2});
}

TEST_CASE(spanning_tree_count_matches_classic_known_counts) {
  using algorithms::graphs::spanning_tree_count_mod_prime;

  Graph triangle(3, false);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  REQUIRE_EQ(spanning_tree_count_mod_prime(triangle, 101U),
             std::uint64_t{3});

  Graph cycle4(4, false);
  cycle4.add_edge(0, 1);
  cycle4.add_edge(1, 2);
  cycle4.add_edge(2, 3);
  cycle4.add_edge(3, 0);
  REQUIRE_EQ(spanning_tree_count_mod_prime(cycle4, 101U),
             std::uint64_t{4});

  Graph complete4(4, false);
  for (Vertex first = 0; first < 4U; ++first) {
    for (Vertex second = first + 1U; second < 4U; ++second) {
      complete4.add_edge(first, second,
                         static_cast<std::int64_t>(first + 10U * second));
    }
  }
  REQUIRE_EQ(spanning_tree_count_mod_prime(complete4, 101U),
             std::uint64_t{16});

  Graph disconnected(4, false);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(2, 3);
  REQUIRE_EQ(spanning_tree_count_mod_prime(disconnected, 101U),
             std::uint64_t{0});
}

TEST_CASE(spanning_tree_count_supports_full_width_prime_modulus) {
  using algorithms::graphs::spanning_tree_count_mod_prime;
  constexpr std::uint64_t prime = 18446744073709551557ULL;

  Graph complete5(5, false);
  for (Vertex first = 0; first < 5U; ++first) {
    for (Vertex second = first + 1U; second < 5U; ++second) {
      complete5.add_edge(first, second);
    }
  }
  // Cayley's formula: K_5 has 5^(5-2) = 125 spanning trees.
  REQUIRE_EQ(spanning_tree_count_mod_prime(complete5, prime),
             std::uint64_t{125});
}

TEST_CASE(spanning_tree_count_randomized_matches_exhaustive_edge_subset_oracle) {
  using algorithms::graphs::spanning_tree_count_mod_prime;
  using spanning_tree_count_tests::LogicalEdge;
  using spanning_tree_count_tests::exhaustive_tree_count_mod;
  using spanning_tree_count_tests::make_graph;

  std::mt19937_64 random(0x4B49524348484F46ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 7);
  std::uniform_int_distribution<int> edge_count_distribution(0, 12);
  std::uniform_int_distribution<int> weight_distribution(-100, 100);
  constexpr std::uint64_t primes[]{2U, 3U, 5U, 17U, 97U, 1000000007U};

  for (int trial = 0; trial < 500; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(
        vertex_count_distribution(random));
    std::vector<LogicalEdge> edges;
    if (vertex_count != 0U) {
      const int edge_count = edge_count_distribution(random);
      std::uniform_int_distribution<std::size_t> vertex_distribution(
          0U, vertex_count - 1U);
      edges.reserve(static_cast<std::size_t>(edge_count));
      for (int edge = 0; edge < edge_count; ++edge) {
        edges.push_back(LogicalEdge{
            vertex_distribution(random), vertex_distribution(random),
            static_cast<std::int64_t>(weight_distribution(random))});
      }
    }

    const std::uint64_t prime =
        primes[static_cast<std::size_t>(random() % std::size(primes))];
    const Graph graph = make_graph(vertex_count, edges);
    REQUIRE_EQ(spanning_tree_count_mod_prime(graph, prime),
               exhaustive_tree_count_mod(vertex_count, edges, prime));
  }
}
