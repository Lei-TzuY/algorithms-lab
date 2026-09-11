#include "algorithms/graphs/tree_isomorphism.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <random>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::TreeIsomorphismResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::exact_tree_isomorphism;

std::vector<std::vector<unsigned char>> adjacency_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<unsigned char>> matrix(
      n, std::vector<unsigned char>(n, 0U));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        matrix[from][edge.to] = 1U;
      }
    }
  }
  return matrix;
}

bool mapping_is_isomorphism(const Graph& first, const Graph& second,
                            const TreeIsomorphismResult& result) {
  const std::size_t n = first.vertex_count();
  if (second.vertex_count() != n || result.first_to_second.size() != n ||
      result.second_to_first.size() != n) {
    return false;
  }
  std::vector<unsigned char> seen(n, 0U);
  for (Vertex first_vertex = 0; first_vertex < n; ++first_vertex) {
    const Vertex second_vertex = result.first_to_second[first_vertex];
    if (second_vertex >= n || seen[second_vertex] != 0U ||
        result.second_to_first[second_vertex] != first_vertex) {
      return false;
    }
    seen[second_vertex] = 1U;
  }
  const auto first_matrix = adjacency_matrix(first);
  const auto second_matrix = adjacency_matrix(second);
  for (Vertex left = 0; left < n; ++left) {
    for (Vertex right = 0; right < n; ++right) {
      if (first_matrix[left][right] !=
          second_matrix[result.first_to_second[left]]
                       [result.first_to_second[right]]) {
        return false;
      }
    }
  }
  return true;
}

bool brute_force_isomorphic(const Graph& first, const Graph& second) {
  const std::size_t n = first.vertex_count();
  if (second.vertex_count() != n) {
    return false;
  }
  const auto first_matrix = adjacency_matrix(first);
  const auto second_matrix = adjacency_matrix(second);
  std::vector<std::size_t> first_degree(n, 0U);
  std::vector<std::size_t> second_degree(n, 0U);
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    first_degree[vertex] = first.neighbors(vertex).size();
    second_degree[vertex] = second.neighbors(vertex).size();
  }

  std::vector<Vertex> permutation(n);
  std::iota(permutation.begin(), permutation.end(), 0U);
  do {
    bool compatible = true;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (first_degree[vertex] != second_degree[permutation[vertex]]) {
        compatible = false;
        break;
      }
    }
    if (!compatible) {
      continue;
    }
    for (Vertex left = 0; left < n && compatible; ++left) {
      for (Vertex right = left + 1U; right < n; ++right) {
        if (first_matrix[left][right] !=
            second_matrix[permutation[left]][permutation[right]]) {
          compatible = false;
          break;
        }
      }
    }
    if (compatible) {
      return true;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return false;
}

Graph make_tree(std::size_t n,
                const std::vector<std::pair<Vertex, Vertex>>& edges) {
  Graph graph(n, false);
  std::int64_t weight = -17;
  for (const auto& [first, second] : edges) {
    graph.add_edge(first, second, weight);
    weight += 11;
  }
  return graph;
}

std::vector<std::pair<Vertex, Vertex>> random_tree_edges(std::size_t n,
                                                         std::mt19937_64& rng) {
  std::vector<std::pair<Vertex, Vertex>> edges;
  if (n <= 1U) {
    return edges;
  }
  if (n == 2U) {
    return {{0U, 1U}};
  }

  std::vector<Vertex> prufer(n - 2U, 0U);
  std::vector<std::size_t> degree(n, 1U);
  std::uniform_int_distribution<std::size_t> vertex_distribution(0U, n - 1U);
  for (Vertex& value : prufer) {
    value = vertex_distribution(rng);
    ++degree[value];
  }
  for (const Vertex value : prufer) {
    Vertex leaf = 0U;
    while (degree[leaf] != 1U) {
      ++leaf;
    }
    edges.emplace_back(leaf, value);
    --degree[leaf];
    --degree[value];
  }
  Vertex first_leaf = 0U;
  while (degree[first_leaf] != 1U) {
    ++first_leaf;
  }
  Vertex second_leaf = first_leaf + 1U;
  while (degree[second_leaf] != 1U) {
    ++second_leaf;
  }
  edges.emplace_back(first_leaf, second_leaf);
  return edges;
}

Graph relabeled_tree(std::size_t n,
                     const std::vector<std::pair<Vertex, Vertex>>& edges,
                     const std::vector<Vertex>& permutation,
                     std::mt19937_64& rng) {
  Graph graph(n, false);
  std::vector<std::pair<Vertex, Vertex>> shuffled = edges;
  std::shuffle(shuffled.begin(), shuffled.end(), rng);
  std::int64_t weight = 100;
  bool reverse = false;
  for (const auto& [left, right] : shuffled) {
    const Vertex mapped_left = permutation[left];
    const Vertex mapped_right = permutation[right];
    if (reverse) {
      graph.add_edge(mapped_right, mapped_left, weight);
    } else {
      graph.add_edge(mapped_left, mapped_right, weight);
    }
    reverse = !reverse;
    weight -= 13;
  }
  return graph;
}

}  // namespace

TEST_CASE(tree_isomorphism_empty_singleton_and_size_mismatch) {
  const Graph empty_first(0U, false);
  const Graph empty_second(0U, false);
  const auto empty_result = exact_tree_isomorphism(empty_first, empty_second);
  REQUIRE(empty_result.has_value());
  REQUIRE(empty_result->first_to_second.empty());
  REQUIRE(empty_result->first_centers.empty());

  const Graph singleton(1U, false);
  const auto singleton_result = exact_tree_isomorphism(singleton, singleton);
  REQUIRE(singleton_result.has_value());
  REQUIRE_EQ(singleton_result->first_to_second, std::vector<Vertex>{0U});
  REQUIRE_EQ(singleton_result->first_centers, std::vector<Vertex>{0U});

  REQUIRE(!exact_tree_isomorphism(empty_first, singleton).has_value());
}

TEST_CASE(tree_isomorphism_known_shapes_weights_and_determinism) {
  const Graph path = make_tree(6U, {{0U, 1U}, {1U, 2U}, {2U, 3U},
                                       {3U, 4U}, {4U, 5U}});
  const Graph relabeled = make_tree(6U, {{5U, 2U}, {2U, 4U}, {4U, 1U},
                                            {1U, 3U}, {3U, 0U}});
  const auto first = exact_tree_isomorphism(path, relabeled);
  const auto second = exact_tree_isomorphism(path, relabeled);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE(mapping_is_isomorphism(path, relabeled, *first));
  REQUIRE_EQ(first->first_to_second, second->first_to_second);
  REQUIRE_EQ(first->first_centers, std::vector<Vertex>({2U, 3U}));
  REQUIRE_EQ(relabeled.vertex_count(), first->second_to_first.size());

  const Graph star = make_tree(6U, {{0U, 1U}, {0U, 2U}, {0U, 3U},
                                       {0U, 4U}, {0U, 5U}});
  REQUIRE(!exact_tree_isomorphism(path, star).has_value());
}

TEST_CASE(tree_isomorphism_rejects_non_trees) {
  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(exact_tree_isomorphism(directed, directed),
                    std::invalid_argument);

  Graph self_loop(1U, false);
  self_loop.add_edge(0U, 0U);
  REQUIRE_THROWS_AS(exact_tree_isomorphism(self_loop, self_loop),
                    std::invalid_argument);

  Graph parallel(2U, false);
  parallel.add_edge(0U, 1U);
  parallel.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(exact_tree_isomorphism(parallel, parallel),
                    std::invalid_argument);

  Graph cycle(3U, false);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 0U);
  REQUIRE_THROWS_AS(exact_tree_isomorphism(cycle, cycle),
                    std::invalid_argument);

  Graph disconnected(4U, false);
  disconnected.add_edge(0U, 1U);
  disconnected.add_edge(1U, 2U);
  disconnected.add_edge(2U, 0U);  // E=V-1 but vertex 3 stays isolated.
  REQUIRE_THROWS_AS(exact_tree_isomorphism(disconnected, disconnected),
                    std::invalid_argument);
}

TEST_CASE(tree_isomorphism_randomized_bruteforce_differential) {
  std::mt19937_64 rng(0xA4F55EEDULL);
  std::uniform_int_distribution<std::size_t> size_distribution(0U, 8U);

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t n = size_distribution(rng);
    const auto first_edges = random_tree_edges(n, rng);
    const Graph first = make_tree(n, first_edges);

    Graph second(0U, false);
    if (trial % 2U == 0U) {
      std::vector<Vertex> permutation(n);
      std::iota(permutation.begin(), permutation.end(), 0U);
      std::shuffle(permutation.begin(), permutation.end(), rng);
      second = relabeled_tree(n, first_edges, permutation, rng);
    } else {
      second = make_tree(n, random_tree_edges(n, rng));
    }

    const bool expected = brute_force_isomorphic(first, second);
    const auto actual = exact_tree_isomorphism(first, second);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      REQUIRE(mapping_is_isomorphism(first, second, *actual));
      const auto repeated = exact_tree_isomorphism(first, second);
      REQUIRE(repeated.has_value());
      REQUIRE_EQ(actual->first_to_second, repeated->first_to_second);
    }
  }
}

TEST_CASE(tree_isomorphism_deep_path_is_iterative) {
  constexpr std::size_t n = 4096U;
  Graph first(n, false);
  Graph second(n, false);
  for (Vertex vertex = 1U; vertex < n; ++vertex) {
    first.add_edge(vertex - 1U, vertex, static_cast<std::int64_t>(vertex));
    second.add_edge(n - vertex, n - vertex - 1U,
                    -static_cast<std::int64_t>(vertex));
  }
  const auto result = exact_tree_isomorphism(first, second);
  REQUIRE(result.has_value());
  REQUIRE(mapping_is_isomorphism(first, second, *result));
}
