#pragma once

#include "algorithms/graphs/general_graph_isomorphism.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::graphs::GeneralGraphIsomorphismResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

using GiMatrix = std::vector<std::vector<std::size_t>>;

GiMatrix gi_test_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  GiMatrix matrix(n, std::vector<std::size_t>(n, 0));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      ++matrix[u][edge.to];
    }
  }
  return matrix;
}

Graph gi_graph_from_matrix(const GiMatrix& matrix, std::int64_t weight_bias = 0) {
  const std::size_t n = matrix.size();
  Graph graph(n, false);
  std::int64_t serial = 0;
  for (Vertex u = 0; u < n; ++u) {
    for (Vertex v = u; v < n; ++v) {
      REQUIRE_EQ(matrix[u][v], matrix[v][u]);
      for (std::size_t copy = 0; copy < matrix[u][v]; ++copy) {
        graph.add_edge(u, v, weight_bias + serial);
        ++serial;
      }
    }
  }
  return graph;
}

bool gi_mapping_replays(const Graph& first, const Graph& second,
                        const GeneralGraphIsomorphismResult& result) {
  const std::size_t n = first.vertex_count();
  if (second.vertex_count() != n || result.first_to_second.size() != n ||
      result.second_to_first.size() != n) {
    return false;
  }
  std::vector<bool> seen(n, false);
  const GiMatrix a = gi_test_matrix(first);
  const GiMatrix b = gi_test_matrix(second);
  for (Vertex u = 0; u < n; ++u) {
    const Vertex mapped = result.first_to_second[u];
    if (mapped >= n || seen[mapped] || result.second_to_first[mapped] != u) {
      return false;
    }
    seen[mapped] = true;
  }
  for (Vertex u = 0; u < n; ++u) {
    for (Vertex v = 0; v < n; ++v) {
      if (a[u][v] != b[result.first_to_second[u]][result.first_to_second[v]]) {
        return false;
      }
    }
  }
  return true;
}

std::optional<std::vector<Vertex>> gi_bruteforce_isomorphism(const Graph& first,
                                                              const Graph& second) {
  if (first.vertex_count() != second.vertex_count()) {
    return std::nullopt;
  }
  const std::size_t n = first.vertex_count();
  const GiMatrix a = gi_test_matrix(first);
  const GiMatrix b = gi_test_matrix(second);
  std::vector<Vertex> permutation(n);
  std::iota(permutation.begin(), permutation.end(), Vertex{0});
  do {
    bool matches = true;
    for (Vertex u = 0; u < n && matches; ++u) {
      for (Vertex v = 0; v < n; ++v) {
        if (a[u][v] != b[permutation[u]][permutation[v]]) {
          matches = false;
          break;
        }
      }
    }
    if (matches) {
      return permutation;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return std::nullopt;
}

GiMatrix gi_permute_matrix(const GiMatrix& matrix, const std::vector<Vertex>& image) {
  const std::size_t n = matrix.size();
  GiMatrix result(n, std::vector<std::size_t>(n, 0));
  for (Vertex u = 0; u < n; ++u) {
    for (Vertex v = 0; v < n; ++v) {
      result[image[u]][image[v]] = matrix[u][v];
    }
  }
  return result;
}

TEST_CASE(general_graph_isomorphism_validation_and_trivial_cases) {
  Graph directed(2, true);
  directed.add_edge(0, 1);
  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_general_graph_isomorphism(directed, directed),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_general_graph_isomorphism(undirected, directed),
                    std::invalid_argument);

  Graph empty_a(0, false);
  Graph empty_b(0, false);
  const auto empty = algorithms::graphs::exact_general_graph_isomorphism(empty_a, empty_b);
  REQUIRE(empty.has_value());
  REQUIRE(empty->first_to_second.empty());
  REQUIRE(empty->second_to_first.empty());

  Graph one_a(1, false);
  Graph one_b(1, false);
  one_a.add_edge(0, 0, -9);
  one_b.add_edge(0, 0, 777);
  const auto one = algorithms::graphs::exact_general_graph_isomorphism(one_a, one_b);
  REQUIRE(one.has_value());
  REQUIRE(gi_mapping_replays(one_a, one_b, *one));

  Graph different_size(2, false);
  REQUIRE(!algorithms::graphs::exact_general_graph_isomorphism(one_a, different_size).has_value());
}

TEST_CASE(general_graph_isomorphism_multigraph_weights_and_replay) {
  GiMatrix matrix(5, std::vector<std::size_t>(5, 0));
  matrix[0][0] = 2;
  matrix[1][1] = 1;
  auto connect = [&](Vertex u, Vertex v, std::size_t multiplicity) {
    matrix[u][v] = multiplicity;
    matrix[v][u] = multiplicity;
  };
  connect(0, 1, 2);
  connect(0, 2, 1);
  connect(1, 3, 3);
  connect(2, 3, 1);
  connect(3, 4, 2);

  Graph first = gi_graph_from_matrix(matrix, -100);
  const std::vector<Vertex> image{3, 1, 4, 0, 2};
  Graph second = gi_graph_from_matrix(gi_permute_matrix(matrix, image), 1000);
  const auto result = algorithms::graphs::exact_general_graph_isomorphism(first, second);
  REQUIRE(result.has_value());
  REQUIRE(gi_mapping_replays(first, second, *result));
  REQUIRE(result->search_nodes > 0);

  const auto again = algorithms::graphs::exact_general_graph_isomorphism(first, second);
  REQUIRE(again.has_value());
  REQUIRE_EQ(result->first_to_second, again->first_to_second);
  REQUIRE_EQ(result->search_nodes, again->search_nodes);
}

TEST_CASE(general_graph_isomorphism_same_degree_nonisomorphic_regression) {
  Graph cycle6(6, false);
  for (Vertex i = 0; i < 6; ++i) {
    cycle6.add_edge(i, (i + 1) % 6);
  }
  Graph two_triangles(6, false);
  two_triangles.add_edge(0, 1);
  two_triangles.add_edge(1, 2);
  two_triangles.add_edge(2, 0);
  two_triangles.add_edge(3, 4);
  two_triangles.add_edge(4, 5);
  two_triangles.add_edge(5, 3);
  REQUIRE(!algorithms::graphs::exact_general_graph_isomorphism(cycle6, two_triangles)
               .has_value());
}

TEST_CASE(general_graph_isomorphism_randomized_differential) {
  std::mt19937_64 rng(0x61A0'BEEF'1234ULL);
  for (std::size_t trial = 0; trial < 450; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    GiMatrix first_matrix(n, std::vector<std::size_t>(n, 0));
    for (Vertex u = 0; u < n; ++u) {
      first_matrix[u][u] = static_cast<std::size_t>(rng() % 2U);
      for (Vertex v = u + 1; v < n; ++v) {
        const std::size_t multiplicity = static_cast<std::size_t>(rng() % 3U);
        first_matrix[u][v] = multiplicity;
        first_matrix[v][u] = multiplicity;
      }
    }
    Graph first = gi_graph_from_matrix(first_matrix, -50);

    Graph second(0, false);
    if ((trial % 2U) == 0U) {
      std::vector<Vertex> image(n);
      std::iota(image.begin(), image.end(), Vertex{0});
      std::shuffle(image.begin(), image.end(), rng);
      second = gi_graph_from_matrix(gi_permute_matrix(first_matrix, image), 900);
    } else {
      GiMatrix second_matrix(n, std::vector<std::size_t>(n, 0));
      for (Vertex u = 0; u < n; ++u) {
        second_matrix[u][u] = static_cast<std::size_t>(rng() % 2U);
        for (Vertex v = u + 1; v < n; ++v) {
          const std::size_t multiplicity = static_cast<std::size_t>(rng() % 3U);
          second_matrix[u][v] = multiplicity;
          second_matrix[v][u] = multiplicity;
        }
      }
      second = gi_graph_from_matrix(second_matrix, 400);
    }

    const auto oracle = gi_bruteforce_isomorphism(first, second);
    const auto actual = algorithms::graphs::exact_general_graph_isomorphism(first, second);
    REQUIRE_EQ(actual.has_value(), oracle.has_value());
    if (actual.has_value()) {
      REQUIRE(gi_mapping_replays(first, second, *actual));
    }
  }
}

TEST_CASE(general_graph_isomorphism_high_symmetry_determinism) {
  Graph complete(8, false);
  for (Vertex u = 0; u < 8; ++u) {
    for (Vertex v = u + 1; v < 8; ++v) {
      complete.add_edge(u, v);
    }
  }
  const auto result = algorithms::graphs::exact_general_graph_isomorphism(complete, complete);
  REQUIRE(result.has_value());
  REQUIRE(gi_mapping_replays(complete, complete, *result));
  const auto repeat = algorithms::graphs::exact_general_graph_isomorphism(complete, complete);
  REQUIRE(repeat.has_value());
  REQUIRE_EQ(result->first_to_second, repeat->first_to_second);
  REQUIRE_EQ(result->search_nodes, repeat->search_nodes);
}

}  // namespace
