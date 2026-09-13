#pragma once

#include "algorithms/graphs/gallai_edmonds.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

namespace gallai_edmonds_test_detail {

using algorithms::graphs::GallaiEdmondsResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

using Matching = std::vector<std::pair<Vertex, Vertex>>;

struct OracleResult {
  std::size_t cardinality{};
  std::vector<Vertex> d_vertices;
  std::vector<Vertex> a_vertices;
  std::vector<Vertex> c_vertices;
  std::vector<Matching> maximum_matchings;
};

[[nodiscard]] inline std::vector<std::vector<bool>> simple_adjacency(
    const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to != u) {
        adjacent[u][edge.to] = true;
        adjacent[edge.to][u] = true;
      }
    }
  }
  return adjacent;
}

inline void enumerate_matchings(
    const std::vector<std::vector<bool>>& adjacent, std::vector<bool>& used,
    Matching& current, std::vector<Matching>& all) {
  Vertex first = used.size();
  for (Vertex vertex = 0; vertex < used.size(); ++vertex) {
    if (!used[vertex]) {
      first = vertex;
      break;
    }
  }
  if (first == used.size()) {
    all.push_back(current);
    return;
  }

  used[first] = true;
  enumerate_matchings(adjacent, used, current, all);
  used[first] = false;

  for (Vertex mate = first + 1U; mate < used.size(); ++mate) {
    if (used[mate] || !adjacent[first][mate]) {
      continue;
    }
    used[first] = true;
    used[mate] = true;
    current.emplace_back(first, mate);
    enumerate_matchings(adjacent, used, current, all);
    current.pop_back();
    used[first] = false;
    used[mate] = false;
  }
}

[[nodiscard]] inline std::vector<Matching> all_matchings(
    const std::vector<std::vector<bool>>& adjacent) {
  std::vector<bool> used(adjacent.size(), false);
  Matching current;
  std::vector<Matching> result;
  enumerate_matchings(adjacent, used, current, result);
  return result;
}

[[nodiscard]] inline OracleResult oracle(const Graph& graph) {
  const auto adjacent = simple_adjacency(graph);
  auto matchings = all_matchings(adjacent);
  std::size_t optimum = 0;
  for (const auto& matching : matchings) {
    optimum = std::max(optimum, matching.size());
  }

  std::vector<Matching> maxima;
  std::vector<bool> exposable(adjacent.size(), false);
  for (const auto& matching : matchings) {
    if (matching.size() != optimum) {
      continue;
    }
    maxima.push_back(matching);
    std::vector<bool> matched(adjacent.size(), false);
    for (const auto& [u, v] : matching) {
      matched[u] = true;
      matched[v] = true;
    }
    for (Vertex vertex = 0; vertex < adjacent.size(); ++vertex) {
      if (!matched[vertex]) {
        exposable[vertex] = true;
      }
    }
  }

  std::vector<bool> attachment(adjacent.size(), false);
  for (Vertex d = 0; d < adjacent.size(); ++d) {
    if (!exposable[d]) {
      continue;
    }
    for (Vertex vertex = 0; vertex < adjacent.size(); ++vertex) {
      if (!exposable[vertex] && adjacent[d][vertex]) {
        attachment[vertex] = true;
      }
    }
  }

  OracleResult result;
  result.cardinality = optimum;
  result.maximum_matchings = std::move(maxima);
  for (Vertex vertex = 0; vertex < adjacent.size(); ++vertex) {
    if (exposable[vertex]) {
      result.d_vertices.push_back(vertex);
    } else if (attachment[vertex]) {
      result.a_vertices.push_back(vertex);
    } else {
      result.c_vertices.push_back(vertex);
    }
  }
  return result;
}

[[nodiscard]] inline std::size_t maximum_matching_size_on_vertices(
    const std::vector<std::vector<bool>>& adjacent,
    const std::vector<Vertex>& vertices,
    std::optional<Vertex> removed = std::nullopt) {
  std::vector<Vertex> kept;
  for (const Vertex vertex : vertices) {
    if (!removed.has_value() || vertex != *removed) {
      kept.push_back(vertex);
    }
  }
  std::vector<std::vector<bool>> induced(
      kept.size(), std::vector<bool>(kept.size(), false));
  for (std::size_t i = 0; i < kept.size(); ++i) {
    for (std::size_t j = i + 1U; j < kept.size(); ++j) {
      induced[i][j] = adjacent[kept[i]][kept[j]];
      induced[j][i] = induced[i][j];
    }
  }
  std::size_t optimum = 0;
  for (const auto& matching : all_matchings(induced)) {
    optimum = std::max(optimum, matching.size());
  }
  return optimum;
}

inline void check_theorem_properties(const Graph& graph,
                                     const GallaiEdmondsResult& result) {
  const auto adjacent = simple_adjacency(graph);

  for (const auto& component : result.d_components) {
    REQUIRE(component.size() % 2U == 1U);
    for (const Vertex removed : component) {
      REQUIRE_EQ(maximum_matching_size_on_vertices(adjacent, component, removed),
                 (component.size() - 1U) / 2U);
    }
  }

  for (const auto& component : result.c_components) {
    REQUIRE(component.size() % 2U == 0U);
    REQUIRE_EQ(maximum_matching_size_on_vertices(adjacent, component),
               component.size() / 2U);
  }

  std::vector<std::size_t> d_component_id(graph.vertex_count(),
                                           result.d_components.size());
  for (std::size_t component = 0; component < result.d_components.size();
       ++component) {
    for (const Vertex vertex : result.d_components[component]) {
      d_component_id[vertex] = component;
    }
  }

  std::set<std::size_t> d_components_matched_from_a;
  for (const Vertex vertex : result.a_vertices) {
    REQUIRE(result.maximum_matching.mate[vertex].has_value());
    const Vertex mate = *result.maximum_matching.mate[vertex];
    REQUIRE(std::binary_search(result.d_vertices.begin(), result.d_vertices.end(),
                               mate));
    REQUIRE(d_components_matched_from_a.insert(d_component_id[mate]).second);
  }
  for (const Vertex vertex : result.c_vertices) {
    REQUIRE(result.maximum_matching.mate[vertex].has_value());
    const Vertex mate = *result.maximum_matching.mate[vertex];
    REQUIRE(std::binary_search(result.c_vertices.begin(), result.c_vertices.end(),
                               mate));
  }
}

inline void check_against_oracle(const Graph& graph) {
  const auto got = algorithms::graphs::gallai_edmonds_decomposition(graph);
  const auto expected = oracle(graph);
  REQUIRE_EQ(got.maximum_matching.cardinality, expected.cardinality);
  REQUIRE_EQ(got.d_vertices, expected.d_vertices);
  REQUIRE_EQ(got.a_vertices, expected.a_vertices);
  REQUIRE_EQ(got.c_vertices, expected.c_vertices);
  REQUIRE_EQ(got.blossom_calls, graph.vertex_count() + 1U);
  check_theorem_properties(graph, got);
}

}  // namespace gallai_edmonds_test_detail

TEST_CASE(gallai_edmonds_deterministic_structures) {
  using gallai_edmonds_test_detail::check_against_oracle;
  using algorithms::graphs::Graph;

  Graph empty(0, false);
  check_against_oracle(empty);

  Graph singleton(1, false);
  check_against_oracle(singleton);

  Graph perfect(4, false);
  perfect.add_edge(0, 1);
  perfect.add_edge(2, 3);
  check_against_oracle(perfect);

  Graph odd_cycle(5, false);
  for (std::size_t v = 0; v < 5; ++v) {
    odd_cycle.add_edge(v, (v + 1U) % 5U);
  }
  check_against_oracle(odd_cycle);

  Graph star(4, false);
  star.add_edge(0, 1);
  star.add_edge(0, 2);
  star.add_edge(0, 3);
  check_against_oracle(star);

  Graph disconnected(7, false);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(1, 2);
  disconnected.add_edge(2, 0);
  disconnected.add_edge(3, 4);
  disconnected.add_edge(5, 6);
  check_against_oracle(disconnected);
}

TEST_CASE(gallai_edmonds_multigraph_semantics_and_validation) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::gallai_edmonds_decomposition;

  Graph graph(5, false);
  graph.add_edge(0, 0, 777);
  graph.add_edge(0, 1, -9);
  graph.add_edge(0, 1, 13);
  graph.add_edge(1, 2, 5);
  graph.add_edge(2, 3, -11);
  graph.add_edge(3, 4, 17);
  gallai_edmonds_test_detail::check_against_oracle(graph);

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(gallai_edmonds_decomposition(directed),
                    std::invalid_argument);
}

TEST_CASE(gallai_edmonds_randomized_exhaustive_differential) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Vertex;

  std::mt19937_64 rng(0x47414c4c41494544ULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 9U);
    Graph graph(n, false);
    const std::size_t edge_copies = static_cast<std::size_t>(rng() % 20U);
    for (std::size_t edge = 0; edge < edge_copies && n != 0; ++edge) {
      const Vertex u = static_cast<Vertex>(rng() % n);
      const Vertex v = static_cast<Vertex>(rng() % n);
      graph.add_edge(u, v, static_cast<std::int64_t>(rng()));
    }
    gallai_edmonds_test_detail::check_against_oracle(graph);
  }
}

TEST_CASE(gallai_edmonds_repeat_determinism) {
  using algorithms::graphs::Vertex;
  algorithms::graphs::Graph graph(8, false);
  const std::vector<std::pair<Vertex, Vertex>> edges{{0, 1}, {1, 2}, {2, 0},
                                                      {2, 3}, {3, 4}, {4, 5},
                                                      {5, 3}, {5, 6}, {6, 7}};
  for (const auto& [u, v] : edges) {
    graph.add_edge(u, v);
  }
  const auto first = algorithms::graphs::gallai_edmonds_decomposition(graph);
  const auto second = algorithms::graphs::gallai_edmonds_decomposition(graph);
  REQUIRE_EQ(first.maximum_matching.cardinality,
             second.maximum_matching.cardinality);
  REQUIRE_EQ(first.maximum_matching.mate, second.maximum_matching.mate);
  REQUIRE_EQ(first.d_vertices, second.d_vertices);
  REQUIRE_EQ(first.a_vertices, second.a_vertices);
  REQUIRE_EQ(first.c_vertices, second.c_vertices);
  REQUIRE_EQ(first.d_components, second.d_components);
  REQUIRE_EQ(first.c_components, second.c_components);
}
