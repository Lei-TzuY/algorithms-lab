#pragma once

#include "algorithms/graphs/directed_arborescence_count.hpp"
#include "algorithms/graphs/spanning_tree_count.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

namespace directed_arborescence_count_tests {

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct DirectedArc {
  Vertex from;
  Vertex to;
  std::int64_t weight;
};

[[nodiscard]] inline Graph make_directed_graph(
    std::size_t vertex_count, const std::vector<DirectedArc>& arcs) {
  Graph graph(vertex_count, true);
  for (const DirectedArc& arc : arcs) {
    graph.add_edge(arc.from, arc.to, arc.weight);
  }
  return graph;
}

[[nodiscard]] inline std::uint64_t exhaustive_out_arborescence_count_mod(
    std::size_t vertex_count, const std::vector<DirectedArc>& arcs,
    Vertex root, std::uint64_t modulus) {
  if (vertex_count == 1U) {
    return 1U % modulus;
  }

  std::vector<std::vector<Vertex>> incoming(vertex_count);
  for (const DirectedArc& arc : arcs) {
    if (arc.from != arc.to && arc.to != root) {
      incoming[arc.to].push_back(arc.from);
    }
  }
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != root && incoming[vertex].empty()) {
      return 0U;
    }
  }

  std::vector<Vertex> parent(vertex_count, root);
  parent[root] = root;
  std::uint64_t count = 0U;

  std::function<void(Vertex)> enumerate = [&](Vertex vertex) {
    if (vertex == vertex_count) {
      for (Vertex start = 0; start < vertex_count; ++start) {
        if (start == root) {
          continue;
        }
        Vertex current = start;
        for (std::size_t step = 0; step < vertex_count && current != root;
             ++step) {
          current = parent[current];
        }
        if (current != root) {
          return;
        }
      }
      ++count;
      if (count == modulus) {
        count = 0U;
      }
      return;
    }

    if (vertex == root) {
      enumerate(vertex + 1U);
      return;
    }
    for (const Vertex source : incoming[vertex]) {
      parent[vertex] = source;
      enumerate(vertex + 1U);
    }
  };
  enumerate(0U);
  return count;
}

}  // namespace directed_arborescence_count_tests

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

TEST_CASE(directed_arborescence_count_validates_contract_and_trivial_graphs) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;

  Graph singleton(1, true);
  singleton.add_edge(0, 0, -99);
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(singleton, 0, 2U),
             std::uint64_t{1});

  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      rooted_out_arborescence_count_mod_prime(undirected, 0, 17U),
      std::invalid_argument);

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      rooted_out_arborescence_count_mod_prime(directed, 2, 17U),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      rooted_out_arborescence_count_mod_prime(directed, 0, 1U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      rooted_out_arborescence_count_mod_prime(directed, 0, 21U),
      std::invalid_argument);
}

TEST_CASE(directed_arborescence_count_has_explicit_out_from_root_semantics) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;
  constexpr std::uint64_t prime = 1000000007U;

  Graph cycle(3, true);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(cycle, 0, prime),
             std::uint64_t{1});

  Graph graph(3, true);
  graph.add_edge(0, 1);
  graph.add_edge(0, 2);
  graph.add_edge(1, 2);
  graph.add_edge(2, 1);
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(graph, 0, prime),
             std::uint64_t{3});

  Graph reversed(3, true);
  reversed.add_edge(1, 0);
  reversed.add_edge(2, 1);
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(reversed, 0, prime),
             std::uint64_t{0});
}

TEST_CASE(directed_arborescence_count_preserves_parallel_multiplicity_and_ignores_loops_weights) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;
  constexpr std::uint64_t prime = 1000000007U;

  Graph graph(3, true);
  graph.add_edge(0, 1, -1000);
  graph.add_edge(0, 1, 77);
  graph.add_edge(0, 2, 1);
  graph.add_edge(0, 2, 2);
  graph.add_edge(0, 2, 3);
  graph.add_edge(1, 2, -9);
  graph.add_edge(0, 0, 55);
  graph.add_edge(1, 1, -3);
  graph.add_edge(2, 2, 8);
  graph.add_edge(2, 0, 123);  // incoming to the root is never selected

  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(graph, 0, prime),
             std::uint64_t{8});
}

TEST_CASE(directed_arborescence_count_matches_complete_digraph_and_full_width_prime) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;
  constexpr std::uint64_t prime = 18446744073709551557ULL;

  Graph complete5(5, true);
  for (Vertex from = 0; from < 5U; ++from) {
    for (Vertex to = 0; to < 5U; ++to) {
      if (from != to) {
        complete5.add_edge(from, to,
                           static_cast<std::int64_t>(from + 10U * to));
      }
    }
  }

  // Cayley's formula with every undirected tree oriented away from the root.
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(complete5, 0, prime),
             std::uint64_t{125});
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(complete5, 3, prime),
             std::uint64_t{125});
  REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(complete5, 0, 5U),
             std::uint64_t{0});
}

TEST_CASE(directed_arborescence_count_randomized_matches_independent_incoming_edge_oracle) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;
  using directed_arborescence_count_tests::DirectedArc;
  using directed_arborescence_count_tests::exhaustive_out_arborescence_count_mod;
  using directed_arborescence_count_tests::make_directed_graph;

  std::mt19937_64 random(0x4449524543544D54ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(1, 6);
  std::uniform_int_distribution<int> edge_count_distribution(0, 12);
  std::uniform_int_distribution<int> weight_distribution(-100, 100);
  constexpr std::array<std::uint64_t, 6> primes{
      2U, 3U, 5U, 17U, 97U, 1000000007U};

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(
        vertex_count_distribution(random));
    std::uniform_int_distribution<std::size_t> vertex_distribution(
        0U, vertex_count - 1U);
    const int edge_count = edge_count_distribution(random);
    std::vector<DirectedArc> arcs;
    arcs.reserve(static_cast<std::size_t>(edge_count));
    for (int edge = 0; edge < edge_count; ++edge) {
      arcs.push_back(DirectedArc{
          vertex_distribution(random), vertex_distribution(random),
          static_cast<std::int64_t>(weight_distribution(random))});
    }

    const Vertex root = vertex_distribution(random);
    const std::uint64_t prime =
        primes[static_cast<std::size_t>(random() % primes.size())];
    const Graph graph = make_directed_graph(vertex_count, arcs);
    REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(graph, root, prime),
               exhaustive_out_arborescence_count_mod(vertex_count, arcs, root,
                                                      prime));
  }
}

TEST_CASE(directed_arborescence_count_agrees_with_kirchhoff_on_bidirected_multigraphs) {
  using algorithms::graphs::rooted_out_arborescence_count_mod_prime;
  using algorithms::graphs::spanning_tree_count_mod_prime;
  constexpr std::uint64_t prime = 1000000007U;

  std::mt19937_64 random(0x4249444952454354ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(1, 7);
  std::uniform_int_distribution<int> edge_count_distribution(0, 12);

  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(
        vertex_count_distribution(random));
    std::uniform_int_distribution<std::size_t> vertex_distribution(
        0U, vertex_count - 1U);
    Graph undirected(vertex_count, false);
    Graph directed(vertex_count, true);
    const int edge_count = edge_count_distribution(random);
    for (int edge = 0; edge < edge_count; ++edge) {
      const Vertex first = vertex_distribution(random);
      const Vertex second = vertex_distribution(random);
      const std::int64_t weight =
          static_cast<std::int64_t>(random() % 201U) - 100;
      undirected.add_edge(first, second, weight);
      directed.add_edge(first, second, weight);
      if (first != second) {
        directed.add_edge(second, first, weight);
      }
    }

    const Vertex root = vertex_distribution(random);
    REQUIRE_EQ(rooted_out_arborescence_count_mod_prime(directed, root, prime),
               spanning_tree_count_mod_prime(undirected, prime));
  }
}
