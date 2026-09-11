#include "algorithms/graphs/maximum_clique.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

std::vector<std::vector<bool>> simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (u != edge.to) adjacent[u][edge.to] = true;
    }
  }
  return adjacent;
}

bool is_clique(const std::vector<Vertex>& vertices,
               const std::vector<std::vector<bool>>& adjacent) {
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    for (std::size_t j = i + 1; j < vertices.size(); ++j) {
      if (!adjacent[vertices[i]][vertices[j]]) return false;
    }
  }
  return true;
}

std::vector<Vertex> exhaustive_maximum_clique(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n > 20U) throw std::invalid_argument("oracle graph too large");
  const auto adjacent = simple_adjacency(graph);
  std::vector<Vertex> best;
  const std::uint64_t subset_count = std::uint64_t{1} << n;
  for (std::uint64_t mask = 0; mask < subset_count; ++mask) {
    std::vector<Vertex> subset;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if ((mask & (std::uint64_t{1} << vertex)) != 0U) subset.push_back(vertex);
    }
    if (subset.size() < best.size() || !is_clique(subset, adjacent)) continue;
    if (subset.size() > best.size() || subset < best) best = std::move(subset);
  }
  return best;
}

}  // namespace

TEST_CASE(maximum_clique_empty_singleton_and_directed_rejection) {
  const Graph empty(0, false);
  REQUIRE(algorithms::graphs::maximum_clique_bron_kerbosch(empty).vertices.empty());

  Graph singleton(1, false);
  singleton.add_edge(0, 0, 99);
  REQUIRE_EQ(algorithms::graphs::maximum_clique_bron_kerbosch(singleton).vertices,
             std::vector<Vertex>({0}));

  const Graph directed(2, true);
  REQUIRE_THROWS_AS(algorithms::graphs::maximum_clique_bron_kerbosch(directed),
                    std::invalid_argument);
}

TEST_CASE(maximum_clique_canonical_witness_and_multigraph_semantics) {
  Graph graph(6, false);
  graph.add_edge(0, 1, 9);
  graph.add_edge(0, 2, -7);
  graph.add_edge(1, 2, 4);
  graph.add_edge(0, 1, 111);  // parallel copy
  graph.add_edge(0, 0, -100); // ignored self-loop
  graph.add_edge(3, 4);
  graph.add_edge(3, 5);
  graph.add_edge(4, 5);

  const auto first = algorithms::graphs::maximum_clique_bron_kerbosch(graph);
  const auto second = algorithms::graphs::maximum_clique_bron_kerbosch(graph);
  REQUIRE_EQ(first.vertices, std::vector<Vertex>({0, 1, 2}));
  REQUIRE_EQ(second.vertices, first.vertices);
  REQUIRE(first.recursive_calls > 0U);
  REQUIRE(first.maximal_cliques_examined >= 2U);
}

TEST_CASE(maximum_clique_cycle_and_complete_graph) {
  Graph cycle(5, false);
  for (Vertex v = 0; v < 5; ++v) cycle.add_edge(v, (v + 1U) % 5U);
  REQUIRE_EQ(algorithms::graphs::maximum_clique_bron_kerbosch(cycle).vertices,
             std::vector<Vertex>({0, 1}));

  Graph complete(7, false);
  for (Vertex u = 0; u < 7; ++u) {
    for (Vertex v = u + 1U; v < 7; ++v) complete.add_edge(u, v);
  }
  REQUIRE_EQ(algorithms::graphs::maximum_clique_bron_kerbosch(complete).vertices,
             std::vector<Vertex>({0, 1, 2, 3, 4, 5, 6}));
}

TEST_CASE(maximum_clique_randomized_differential_exhaustive) {
  std::mt19937_64 rng(0xC11C0EULL);
  std::uniform_int_distribution<int> size_distribution(0, 11);
  std::bernoulli_distribution edge_present(0.34);
  std::bernoulli_distribution parallel_copy(0.12);
  std::bernoulli_distribution self_loop(0.10);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);

  for (std::size_t trial = 0; trial < 1000; ++trial) {
    const auto n = static_cast<std::size_t>(size_distribution(rng));
    Graph graph(n, false);
    for (Vertex u = 0; u < n; ++u) {
      if (self_loop(rng)) graph.add_edge(u, u, weight_distribution(rng));
      for (Vertex v = u + 1U; v < n; ++v) {
        if (!edge_present(rng)) continue;
        graph.add_edge(u, v, weight_distribution(rng));
        if (parallel_copy(rng)) graph.add_edge(u, v, weight_distribution(rng));
      }
    }

    const auto expected = exhaustive_maximum_clique(graph);
    const auto actual = algorithms::graphs::maximum_clique_bron_kerbosch(graph);
    REQUIRE_EQ(actual.vertices, expected);
    REQUIRE(is_clique(actual.vertices, simple_adjacency(graph)));
  }
}
