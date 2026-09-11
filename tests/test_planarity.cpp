#include "algorithms/graphs/planarity.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::PlanarRotationEmbedding;
using algorithms::graphs::Vertex;
using algorithms::graphs::exact_planar_rotation_embedding;

struct SmallSimpleGraph {
  std::size_t n{};
  std::vector<std::uint64_t> rows;
};

SmallSimpleGraph simplify_for_oracle(const Graph& graph) {
  SmallSimpleGraph result{graph.vertex_count(),
                          std::vector<std::uint64_t>(graph.vertex_count(), 0U)};
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from == edge.to) {
        continue;
      }
      result.rows[from] |= (std::uint64_t{1} << edge.to);
      result.rows[edge.to] |= (std::uint64_t{1} << from);
    }
  }
  return result;
}

bool connected_branch_set(const SmallSimpleGraph& graph, std::uint64_t mask) {
  if (mask == 0U) {
    return false;
  }
  const std::size_t start = static_cast<std::size_t>(std::countr_zero(mask));
  std::uint64_t seen = std::uint64_t{1} << start;
  std::uint64_t frontier = seen;
  while (frontier != 0U) {
    const std::size_t vertex =
        static_cast<std::size_t>(std::countr_zero(frontier));
    frontier &= frontier - 1U;
    const std::uint64_t next = graph.rows[vertex] & mask & ~seen;
    seen |= next;
    frontier |= next;
  }
  return seen == mask;
}

bool branch_sets_adjacent(const SmallSimpleGraph& graph, std::uint64_t first,
                          std::uint64_t second) {
  std::uint64_t remaining = first;
  while (remaining != 0U) {
    const std::size_t vertex =
        static_cast<std::size_t>(std::countr_zero(remaining));
    remaining &= remaining - 1U;
    if ((graph.rows[vertex] & second) != 0U) {
      return true;
    }
  }
  return false;
}

enum class ForbiddenMinor { K5, K33 };

bool contains_forbidden_minor(const SmallSimpleGraph& graph,
                              ForbiddenMinor target) {
  const std::size_t groups = target == ForbiddenMinor::K5 ? 5U : 6U;
  if (graph.n < groups) {
    return false;
  }

  std::uint64_t assignments = 1U;
  for (std::size_t vertex = 0; vertex < graph.n; ++vertex) {
    assignments *= static_cast<std::uint64_t>(groups + 1U);
  }

  for (std::uint64_t code = 0U; code < assignments; ++code) {
    std::uint64_t value = code;
    std::vector<std::uint64_t> branch_sets(groups, 0U);
    for (std::size_t vertex = 0; vertex < graph.n; ++vertex) {
      const std::size_t label = static_cast<std::size_t>(
          value % static_cast<std::uint64_t>(groups + 1U));
      value /= static_cast<std::uint64_t>(groups + 1U);
      if (label < groups) {
        branch_sets[label] |= std::uint64_t{1} << vertex;
      }
    }

    bool valid = true;
    for (const std::uint64_t branch_set : branch_sets) {
      if (!connected_branch_set(graph, branch_set)) {
        valid = false;
        break;
      }
    }
    if (!valid) {
      continue;
    }

    if (target == ForbiddenMinor::K5) {
      for (std::size_t first = 0; first < groups && valid; ++first) {
        for (std::size_t second = first + 1U; second < groups; ++second) {
          if (!branch_sets_adjacent(graph, branch_sets[first],
                                    branch_sets[second])) {
            valid = false;
            break;
          }
        }
      }
    } else {
      for (std::size_t left = 0; left < 3U && valid; ++left) {
        for (std::size_t right = 3U; right < 6U; ++right) {
          if (!branch_sets_adjacent(graph, branch_sets[left],
                                    branch_sets[right])) {
            valid = false;
            break;
          }
        }
      }
    }
    if (valid) {
      return true;
    }
  }
  return false;
}

bool exact_planarity_oracle(const Graph& graph) {
  const SmallSimpleGraph simple = simplify_for_oracle(graph);
  return !contains_forbidden_minor(simple, ForbiddenMinor::K5) &&
         !contains_forbidden_minor(simple, ForbiddenMinor::K33);
}

std::vector<std::vector<Vertex>> simple_neighbors(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        adjacent[from][edge.to] = true;
        adjacent[edge.to][from] = true;
      }
    }
  }
  std::vector<std::vector<Vertex>> result(n);
  for (Vertex first = 0; first < n; ++first) {
    for (Vertex second = 0; second < n; ++second) {
      if (adjacent[first][second]) {
        result[first].push_back(second);
      }
    }
  }
  return result;
}

void verify_embedding(const Graph& graph, const PlanarRotationEmbedding& embedding) {
  const auto expected = simple_neighbors(graph);
  REQUIRE_EQ(embedding.clockwise_neighbors.size(), graph.vertex_count());
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    auto actual = embedding.clockwise_neighbors[vertex];
    std::sort(actual.begin(), actual.end());
    REQUIRE_EQ(actual, expected[vertex]);
  }

  std::vector<bool> seen(graph.vertex_count(), false);
  std::size_t component_index = 0U;
  for (Vertex start = 0; start < graph.vertex_count(); ++start) {
    if (seen[start]) {
      continue;
    }
    std::vector<Vertex> component;
    std::vector<Vertex> stack{start};
    seen[start] = true;
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      component.push_back(vertex);
      for (const Vertex next : expected[vertex]) {
        if (!seen[next]) {
          seen[next] = true;
          stack.push_back(next);
        }
      }
    }

    std::size_t degree_sum = 0U;
    for (const Vertex vertex : component) {
      degree_sum += expected[vertex].size();
    }
    const std::size_t edges = degree_sum / 2U;
    const std::size_t faces = embedding.component_face_counts[component_index];
    REQUIRE_EQ(component.size() + faces, edges + 2U);
    ++component_index;
  }
  REQUIRE_EQ(component_index, embedding.component_face_counts.size());
}

Graph complete_graph(std::size_t n) {
  Graph graph(n, false);
  for (Vertex first = 0; first < n; ++first) {
    for (Vertex second = first + 1U; second < n; ++second) {
      graph.add_edge(first, second);
    }
  }
  return graph;
}

Graph complete_bipartite(std::size_t left, std::size_t right) {
  Graph graph(left + right, false);
  for (Vertex first = 0; first < left; ++first) {
    for (Vertex second = 0; second < right; ++second) {
      graph.add_edge(first, left + second);
    }
  }
  return graph;
}

}  // namespace

TEST_CASE(planarity_basic_planar_and_nonplanar_shapes) {
  Graph empty(0, false);
  const auto empty_result = exact_planar_rotation_embedding(empty);
  REQUIRE(empty_result.has_value());
  verify_embedding(empty, *empty_result);

  Graph triangle(3, false);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  const auto triangle_result = exact_planar_rotation_embedding(triangle);
  REQUIRE(triangle_result.has_value());
  verify_embedding(triangle, *triangle_result);

  Graph tree(7, false);
  for (Vertex vertex = 1; vertex < 7; ++vertex) {
    tree.add_edge(0, vertex);
  }
  const auto tree_result = exact_planar_rotation_embedding(tree);
  REQUIRE(tree_result.has_value());
  verify_embedding(tree, *tree_result);

  REQUIRE(!exact_planar_rotation_embedding(complete_graph(5)).has_value());
  REQUIRE(!exact_planar_rotation_embedding(complete_bipartite(3, 3)).has_value());
}

TEST_CASE(planarity_dense_boundary_and_disconnected_components) {
  Graph planar_dense(5, false);
  for (Vertex first = 0; first < 5; ++first) {
    for (Vertex second = first + 1U; second < 5; ++second) {
      if (!(first == 3U && second == 4U)) {
        planar_dense.add_edge(first, second);
      }
    }
  }
  const auto dense_result = exact_planar_rotation_embedding(planar_dense);
  REQUIRE(dense_result.has_value());
  verify_embedding(planar_dense, *dense_result);

  Graph disconnected(9, false);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(1, 2);
  disconnected.add_edge(2, 0);
  for (Vertex left = 3; left < 6; ++left) {
    for (Vertex right = 6; right < 9; ++right) {
      disconnected.add_edge(left, right);
    }
  }
  REQUIRE(!exact_planar_rotation_embedding(disconnected).has_value());
}

TEST_CASE(planarity_multigraph_weights_determinism_and_validation) {
  Graph multigraph(4, false);
  multigraph.add_edge(0, 0, -99);
  multigraph.add_edge(0, 1, 7);
  multigraph.add_edge(0, 1, -13);
  multigraph.add_edge(1, 2, 500);
  multigraph.add_edge(2, 3, -2);
  multigraph.add_edge(3, 0, 17);
  multigraph.add_edge(0, 2, 1);

  const auto first = exact_planar_rotation_embedding(multigraph);
  const auto second = exact_planar_rotation_embedding(multigraph);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE_EQ(first->clockwise_neighbors, second->clockwise_neighbors);
  REQUIRE_EQ(first->component_face_counts, second->component_face_counts);
  REQUIRE_EQ(first->rotation_systems_tested, second->rotation_systems_tested);
  verify_embedding(multigraph, *first);

  Graph reordered(4, false);
  reordered.add_edge(0, 2, 9999);
  reordered.add_edge(3, 0, 1);
  reordered.add_edge(2, 3, 1);
  reordered.add_edge(1, 2, 1);
  reordered.add_edge(0, 1, 1);
  reordered.add_edge(0, 0, 123);
  const auto reordered_result = exact_planar_rotation_embedding(reordered);
  REQUIRE(reordered_result.has_value());
  REQUIRE_EQ(first->clockwise_neighbors, reordered_result->clockwise_neighbors);

  Graph directed(3, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(exact_planar_rotation_embedding(directed),
                    std::invalid_argument);
}

TEST_CASE(planarity_subdivision_of_k33_is_nonplanar) {
  Graph graph(7, false);
  for (Vertex left = 0; left < 3; ++left) {
    for (Vertex right = 3; right < 6; ++right) {
      if (left == 0U && right == 3U) {
        continue;
      }
      graph.add_edge(left, right);
    }
  }
  graph.add_edge(0, 6);
  graph.add_edge(6, 3);
  REQUIRE(!exact_planar_rotation_embedding(graph).has_value());
}

TEST_CASE(planarity_randomized_wagner_minor_differential) {
  std::mt19937_64 rng(0x51A9A17ULL);
  for (std::size_t trial = 0; trial < 220U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 7U);
    Graph graph(n, false);
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if ((rng() % 17U) == 0U) {
        graph.add_edge(vertex, vertex,
                       static_cast<std::int64_t>(rng() % 101U) - 50);
      }
      for (Vertex other = vertex + 1U; other < n; ++other) {
        if ((rng() % 100U) < 27U) {
          graph.add_edge(vertex, other,
                         static_cast<std::int64_t>(rng() % 101U) - 50);
          if ((rng() % 11U) == 0U) {
            graph.add_edge(vertex, other,
                           static_cast<std::int64_t>(rng() % 101U) - 50);
          }
        }
      }
    }

    const bool expected = exact_planarity_oracle(graph);
    const auto actual = exact_planar_rotation_embedding(graph);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      verify_embedding(graph, *actual);
    }
  }
}
