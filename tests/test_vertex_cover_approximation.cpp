#include "algorithms/approximation/vertex_cover.hpp"
#include "algorithms/graphs/graph.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::approximation::VertexCoverApproximationResult;
using algorithms::approximation::approximate_minimum_vertex_cover;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct UndirectedEdge {
  Vertex u;
  Vertex v;
};

std::vector<UndirectedEdge> canonical_edges(const Graph& graph) {
  std::vector<UndirectedEdge> edges;
  for (Vertex u = 0; u < graph.vertex_count(); ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to < u) {
        continue;
      }
      edges.push_back({u, edge.to});
    }
  }
  return edges;
}

bool is_vertex_cover(const std::vector<bool>& selected,
                     const std::vector<UndirectedEdge>& edges) {
  for (const auto& edge : edges) {
    if (!selected[edge.u] && !selected[edge.v]) {
      return false;
    }
  }
  return true;
}

std::size_t exhaustive_optimum(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  const auto edges = canonical_edges(graph);
  if (n >= 63) {
    throw std::invalid_argument("exhaustive test oracle supports fewer than 63 vertices");
  }
  const std::uint64_t limit = std::uint64_t{1} << n;
  std::size_t best = n + 1;
  std::vector<bool> selected(n, false);
  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    const std::size_t count = static_cast<std::size_t>(std::popcount(mask));
    if (count >= best) {
      continue;
    }
    for (std::size_t v = 0; v < n; ++v) {
      selected[v] = ((mask >> v) & std::uint64_t{1}) != 0;
    }
    if (is_vertex_cover(selected, edges)) {
      best = count;
    }
  }
  return best == n + 1 ? 0 : best;
}

void verify_result(const Graph& graph, const VertexCoverApproximationResult& result) {
  const std::size_t n = graph.vertex_count();
  std::vector<bool> selected(n, false);
  for (const Vertex v : result.vertices) {
    REQUIRE(v < n);
    REQUIRE(!selected[v]);
    selected[v] = true;
  }
  REQUIRE(is_vertex_cover(selected, canonical_edges(graph)));

  std::vector<bool> forced(n, false);
  for (const Vertex v : result.forced_self_loop_vertices) {
    REQUIRE(v < n);
    REQUIRE(!forced[v]);
    forced[v] = true;
    REQUIRE(selected[v]);
    bool has_loop = false;
    for (const auto& edge : graph.neighbors(v)) {
      has_loop = has_loop || edge.to == v;
    }
    REQUIRE(has_loop);
  }

  std::vector<bool> matched(n, false);
  for (const auto& [u, v] : result.maximal_matching_edges) {
    REQUIRE(u < v);
    REQUIRE(v < n);
    REQUIRE(!forced[u]);
    REQUIRE(!forced[v]);
    REQUIRE(!matched[u]);
    REQUIRE(!matched[v]);
    REQUIRE(selected[u]);
    REQUIRE(selected[v]);
    matched[u] = true;
    matched[v] = true;

    bool found = false;
    for (const auto& edge : graph.neighbors(u)) {
      found = found || edge.to == v;
    }
    REQUIRE(found);
  }

  // Maximality on the residual graph: every non-forced original edge has at
  // least one endpoint already matched.
  for (const auto& edge : canonical_edges(graph)) {
    if (edge.u == edge.v || forced[edge.u] || forced[edge.v]) {
      continue;
    }
    REQUIRE(matched[edge.u] || matched[edge.v]);
  }
}

TEST_CASE(vertex_cover_approximation_empty_and_directed_validation) {
  Graph empty(0, false);
  const auto result = approximate_minimum_vertex_cover(empty);
  REQUIRE(result.vertices.empty());
  REQUIRE(result.forced_self_loop_vertices.empty());
  REQUIRE(result.maximal_matching_edges.empty());

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(approximate_minimum_vertex_cover(directed), std::invalid_argument);
}

TEST_CASE(vertex_cover_approximation_self_loops_parallel_and_deterministic_matching) {
  Graph graph(6, false);
  graph.add_edge(0, 0, -99);
  graph.add_edge(0, 1, 1000);
  graph.add_edge(1, 2, -7);
  graph.add_edge(1, 2, 42);  // parallel copy with a different ignored weight
  graph.add_edge(2, 3, 8);
  graph.add_edge(3, 4, -1234);
  graph.add_edge(4, 5, 5);

  const auto first = approximate_minimum_vertex_cover(graph);
  const auto second = approximate_minimum_vertex_cover(graph);
  REQUIRE_EQ(first.vertices, second.vertices);
  REQUIRE_EQ(first.forced_self_loop_vertices, second.forced_self_loop_vertices);
  REQUIRE_EQ(first.maximal_matching_edges, second.maximal_matching_edges);
  REQUIRE_EQ(first.forced_self_loop_vertices, std::vector<Vertex>({0}));
  verify_result(graph, first);
}

TEST_CASE(vertex_cover_approximation_matches_two_approx_bound_on_adversarial_graphs) {
  for (std::size_t n = 1; n <= 9; ++n) {
    Graph clique(n, false);
    for (Vertex u = 0; u < n; ++u) {
      for (Vertex v = u + 1; v < n; ++v) {
        clique.add_edge(u, v);
      }
    }
    const auto result = approximate_minimum_vertex_cover(clique);
    verify_result(clique, result);
    const std::size_t optimum = exhaustive_optimum(clique);
    REQUIRE(result.vertices.size() <= 2 * optimum);
  }

  Graph star(9, false);
  for (Vertex v = 1; v < 9; ++v) {
    star.add_edge(0, v);
  }
  const auto star_result = approximate_minimum_vertex_cover(star);
  verify_result(star, star_result);
  REQUIRE(star_result.vertices.size() <= 2 * exhaustive_optimum(star));
}

TEST_CASE(vertex_cover_approximation_randomized_exhaustive_quality_verification) {
  std::mt19937_64 rng(0xA9910C0EULL);
  std::uniform_int_distribution<int> vertex_count_dist(0, 10);
  std::uniform_int_distribution<int> multiplicity_dist(0, 2);
  std::uniform_int_distribution<int> loop_dist(0, 11);

  for (int trial = 0; trial < 600; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_dist(rng));
    Graph graph(n, false);
    for (Vertex u = 0; u < n; ++u) {
      if (loop_dist(rng) == 0) {
        graph.add_edge(u, u);
      }
      for (Vertex v = u + 1; v < n; ++v) {
        const int multiplicity = multiplicity_dist(rng);
        for (int copy = 0; copy < multiplicity; ++copy) {
          graph.add_edge(u, v);
        }
      }
    }

    const auto result = approximate_minimum_vertex_cover(graph);
    verify_result(graph, result);
    const std::size_t optimum = exhaustive_optimum(graph);
    REQUIRE(result.vertices.size() <= 2 * optimum);

    // The proof witness gives a lower bound independent of exhaustive search.
    const std::size_t lower_bound = result.forced_self_loop_vertices.size() +
                                    result.maximal_matching_edges.size();
    REQUIRE(lower_bound <= optimum);
    REQUIRE(result.vertices.size() == result.forced_self_loop_vertices.size() +
                                         2 * result.maximal_matching_edges.size());
  }
}

}  // namespace
