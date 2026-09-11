#include "algorithms/graphs/exact_coloring.hpp"
#include "algorithms/graphs/maximum_clique.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
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

namespace {
using algorithms::graphs::ExactColoringResult;

bool is_valid_coloring(const Graph& graph, const ExactColoringResult& result) {
  if (result.colors.size() != graph.vertex_count()) return false;
  if (graph.vertex_count() == 0) return result.chromatic_number == 0;
  std::size_t maximum_color = 0;
  for (const std::size_t color : result.colors) {
    if (color >= result.chromatic_number) return false;
    maximum_color = std::max(maximum_color, color);
  }
  if (maximum_color + 1U != result.chromatic_number) return false;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from == edge.to || result.colors[from] == result.colors[edge.to]) return false;
    }
  }
  return true;
}

bool exhaustive_k_colorable(const std::vector<std::vector<bool>>& adjacent,
                            std::size_t color_count,
                            std::vector<std::size_t>& colors, Vertex vertex) {
  if (vertex == colors.size()) return true;
  for (std::size_t color = 0; color < color_count; ++color) {
    bool allowed = true;
    for (Vertex earlier = 0; earlier < vertex; ++earlier) {
      if (adjacent[vertex][earlier] && colors[earlier] == color) {
        allowed = false;
        break;
      }
    }
    if (!allowed) continue;
    colors[vertex] = color;
    if (exhaustive_k_colorable(adjacent, color_count, colors, vertex + 1U)) return true;
  }
  return false;
}

std::optional<std::size_t> exhaustive_chromatic_number(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    for (const auto& edge : graph.neighbors(vertex)) {
      if (vertex == edge.to) return std::nullopt;
    }
  }
  if (n == 0) return 0U;
  const auto adjacent = simple_adjacency(graph);
  for (std::size_t color_count = 1; color_count <= n; ++color_count) {
    std::vector<std::size_t> colors(n, 0U);
    if (exhaustive_k_colorable(adjacent, color_count, colors, 0U)) return color_count;
  }
  return n;
}

Graph odd_cycle_graph(std::size_t n) {
  Graph graph(n, false);
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    graph.add_edge(vertex, (vertex + 1U) % n);
  }
  return graph;
}
}  // namespace

TEST_CASE(exact_coloring_empty_directed_and_self_loop_boundaries) {
  const Graph empty(0, false);
  const auto empty_result = algorithms::graphs::exact_graph_coloring_dsatur(empty);
  REQUIRE(empty_result.has_value());
  REQUIRE_EQ(empty_result->chromatic_number, 0U);
  REQUIRE(empty_result->colors.empty());

  const Graph directed(2, true);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_graph_coloring_dsatur(directed),
                    std::invalid_argument);

  Graph loop(1, false);
  loop.add_edge(0, 0, -123);
  REQUIRE(!algorithms::graphs::exact_graph_coloring_dsatur(loop).has_value());
}

TEST_CASE(exact_coloring_clique_lower_bound_is_not_the_answer) {
  Graph cycle = odd_cycle_graph(5);
  const auto result = algorithms::graphs::exact_graph_coloring_dsatur(cycle);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->clique_lower_bound_witness, std::vector<Vertex>({0, 1}));
  REQUIRE_EQ(result->chromatic_number, 3U);
  REQUIRE(result->greedy_upper_bound >= result->chromatic_number);
  REQUIRE(is_valid_coloring(cycle, *result));

  const auto repeated = algorithms::graphs::exact_graph_coloring_dsatur(cycle);
  REQUIRE(repeated.has_value());
  REQUIRE_EQ(repeated->colors, result->colors);
  REQUIRE_EQ(repeated->search_nodes, result->search_nodes);
}

TEST_CASE(exact_coloring_complete_bipartite_and_multigraph_invariance) {
  Graph complete(5, false);
  for (Vertex first = 0; first < 5; ++first) {
    for (Vertex second = first + 1U; second < 5; ++second) complete.add_edge(first, second);
  }
  const auto complete_result = algorithms::graphs::exact_graph_coloring_dsatur(complete);
  REQUIRE(complete_result.has_value());
  REQUIRE_EQ(complete_result->chromatic_number, 5U);
  REQUIRE_EQ(complete_result->clique_lower_bound_witness.size(), 5U);

  Graph bipartite(6, false);
  for (Vertex left = 0; left < 3; ++left) {
    for (Vertex right = 3; right < 6; ++right) bipartite.add_edge(left, right);
  }
  const auto bipartite_result = algorithms::graphs::exact_graph_coloring_dsatur(bipartite);
  REQUIRE(bipartite_result.has_value());
  REQUIRE_EQ(bipartite_result->chromatic_number, 2U);
  REQUIRE(is_valid_coloring(bipartite, *bipartite_result));

  Graph first(5, false);
  first.add_edge(0, 1, 1);
  first.add_edge(1, 2, 2);
  first.add_edge(2, 3, 3);
  first.add_edge(3, 4, 4);
  first.add_edge(4, 0, 5);
  first.add_edge(0, 2, 6);

  Graph reordered(5, false);
  reordered.add_edge(2, 0, -100);
  reordered.add_edge(0, 4, 99);
  reordered.add_edge(4, 3, -7);
  reordered.add_edge(3, 2, 42);
  reordered.add_edge(2, 1, 11);
  reordered.add_edge(1, 0, 12);
  reordered.add_edge(0, 2, 777);  // parallel copy, ignored for coloring

  const auto first_result = algorithms::graphs::exact_graph_coloring_dsatur(first);
  const auto reordered_result = algorithms::graphs::exact_graph_coloring_dsatur(reordered);
  REQUIRE(first_result.has_value());
  REQUIRE(reordered_result.has_value());
  REQUIRE_EQ(reordered_result->chromatic_number, first_result->chromatic_number);
  REQUIRE_EQ(reordered_result->colors, first_result->colors);
  REQUIRE_EQ(reordered_result->clique_lower_bound_witness,
             first_result->clique_lower_bound_witness);
}

TEST_CASE(exact_coloring_randomized_differential_and_clique_crosscheck) {
  std::mt19937_64 rng(0xC010A11ULL);
  std::uniform_int_distribution<int> size_distribution(0, 9);
  std::bernoulli_distribution edge_present(0.38);
  std::bernoulli_distribution parallel_copy(0.14);
  std::uniform_int_distribution<int> weight_distribution(-100, 100);

  for (std::size_t trial = 0; trial < 600; ++trial) {
    const auto n = static_cast<std::size_t>(size_distribution(rng));
    Graph graph(n, false);
    for (Vertex first = 0; first < n; ++first) {
      for (Vertex second = first + 1U; second < n; ++second) {
        if (!edge_present(rng)) continue;
        graph.add_edge(first, second, weight_distribution(rng));
        if (parallel_copy(rng)) graph.add_edge(first, second, weight_distribution(rng));
      }
    }

    const auto expected = exhaustive_chromatic_number(graph);
    const auto actual = algorithms::graphs::exact_graph_coloring_dsatur(graph);
    REQUIRE(expected.has_value());
    REQUIRE(actual.has_value());
    REQUIRE_EQ(actual->chromatic_number, *expected);
    REQUIRE(is_valid_coloring(graph, *actual));

    const auto clique = algorithms::graphs::maximum_clique_bron_kerbosch(graph);
    REQUIRE_EQ(actual->clique_lower_bound_witness, clique.vertices);
    REQUIRE(actual->clique_lower_bound_witness.size() <= actual->chromatic_number);
    REQUIRE(actual->chromatic_number <= actual->greedy_upper_bound);
  }
}
