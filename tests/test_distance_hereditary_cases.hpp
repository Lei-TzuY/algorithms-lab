#pragma once

#include "algorithms/graphs/distance_hereditary.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace distance_hereditary_test_detail {

using algorithms::graphs::DistanceHereditaryPruning;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::distance_hereditary_pruning;
using algorithms::graphs::valid_distance_hereditary_pruning;

using Matrix = std::vector<std::vector<unsigned char>>;

inline Matrix simple_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  Matrix adjacency(n, std::vector<unsigned char>(n, 0U));
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        continue;
      }
      adjacency[from][edge.to] = 1U;
      adjacency[edge.to][from] = 1U;
    }
  }
  return adjacency;
}

inline std::vector<std::size_t> restricted_distances(
    const Matrix& adjacency, const std::uint64_t mask, const Vertex source) {
  const std::size_t n = adjacency.size();
  const std::size_t infinity = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> distance(n, infinity);
  std::queue<Vertex> pending;

  distance[source] = 0U;
  pending.push(source);
  while (!pending.empty()) {
    const Vertex from = pending.front();
    pending.pop();
    for (Vertex to = 0U; to < n; ++to) {
      if (((mask >> to) & 1ULL) == 0ULL || adjacency[from][to] == 0U ||
          distance[to] != infinity) {
        continue;
      }
      distance[to] = distance[from] + 1U;
      pending.push(to);
    }
  }
  return distance;
}

inline bool connected_mask(const Matrix& adjacency, const std::uint64_t mask) {
  if (mask == 0ULL) {
    return false;
  }
  const std::size_t n = adjacency.size();
  Vertex first = 0U;
  while (((mask >> first) & 1ULL) == 0ULL) {
    ++first;
  }
  const auto distance = restricted_distances(adjacency, mask, first);
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if (((mask >> vertex) & 1ULL) != 0ULL &&
        distance[vertex] == std::numeric_limits<std::size_t>::max()) {
      return false;
    }
  }
  return true;
}

// Independent definition oracle:
// every connected induced subgraph preserves all pairwise graph distances.
inline bool distance_hereditary_definition(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  REQUIRE(n <= 10U);
  if (n <= 1U) {
    return true;
  }

  const Matrix adjacency = simple_matrix(graph);
  const std::uint64_t full_mask = (1ULL << n) - 1ULL;
  std::vector<std::vector<std::size_t>> original;
  original.reserve(n);
  for (Vertex source = 0U; source < n; ++source) {
    original.push_back(restricted_distances(adjacency, full_mask, source));
  }

  for (std::uint64_t mask = 1ULL; mask <= full_mask; ++mask) {
    if ((mask & (mask - 1ULL)) == 0ULL ||
        !connected_mask(adjacency, mask)) {
      continue;
    }

    for (Vertex source = 0U; source < n; ++source) {
      if (((mask >> source) & 1ULL) == 0ULL) {
        continue;
      }
      const auto induced = restricted_distances(adjacency, mask, source);
      for (Vertex target = 0U; target < n; ++target) {
        if (((mask >> target) & 1ULL) != 0ULL &&
            induced[target] != original[source][target]) {
          return false;
        }
      }
    }
  }
  return true;
}

inline Graph graph_from_edge_mask(const std::size_t n,
                                  const std::uint64_t edge_mask) {
  Graph graph(n, false);
  std::size_t bit = 0U;
  for (Vertex first = 0U; first < n; ++first) {
    for (Vertex second = first + 1U; second < n; ++second) {
      if (((edge_mask >> bit) & 1ULL) != 0ULL) {
        graph.add_edge(first, second);
      }
      ++bit;
    }
  }
  return graph;
}

inline void require_matches_definition(const Graph& graph) {
  const auto pruning = distance_hereditary_pruning(graph);
  const bool expected = distance_hereditary_definition(graph);
  REQUIRE_EQ(pruning.has_value(), expected);
  if (pruning.has_value()) {
    REQUIRE(valid_distance_hereditary_pruning(graph, *pruning));
  }
}

}  // namespace distance_hereditary_test_detail

TEST_CASE(distance_hereditary_empty_single_and_directed_boundaries) {
  using namespace distance_hereditary_test_detail;

  Graph empty(0U, false);
  const auto empty_pruning = distance_hereditary_pruning(empty);
  REQUIRE(empty_pruning.has_value());
  REQUIRE(empty_pruning->steps.empty());
  REQUIRE(!empty_pruning->survivor.has_value());
  REQUIRE(valid_distance_hereditary_pruning(empty, *empty_pruning));

  Graph singleton(1U, false);
  const auto singleton_pruning = distance_hereditary_pruning(singleton);
  REQUIRE(singleton_pruning.has_value());
  REQUIRE(singleton_pruning->steps.empty());
  REQUIRE(singleton_pruning->survivor.has_value());
  REQUIRE_EQ(*singleton_pruning->survivor, 0U);
  REQUIRE(valid_distance_hereditary_pruning(singleton, *singleton_pruning));

  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(distance_hereditary_pruning(directed),
                    std::invalid_argument);
  REQUIRE(!valid_distance_hereditary_pruning(
      directed, DistanceHereditaryPruning{}));
}

TEST_CASE(distance_hereditary_witness_handles_pendants_twins_and_components) {
  using namespace distance_hereditary_test_detail;

  Graph graph(8U, false);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 3U);
  graph.add_edge(1U, 4U);
  graph.add_edge(2U, 4U);  // true-twin style relation in the remaining graph.
  graph.add_edge(5U, 6U);
  // Vertex 7 is isolated; disconnected distance-hereditary components are valid.

  require_matches_definition(graph);
  const auto pruning = distance_hereditary_pruning(graph);
  REQUIRE(pruning.has_value());
  REQUIRE_EQ(pruning->steps.size(), 7U);
}

TEST_CASE(distance_hereditary_rejects_induced_cycle_five) {
  using namespace distance_hereditary_test_detail;

  Graph cycle(5U, false);
  for (Vertex vertex = 0U; vertex < 5U; ++vertex) {
    cycle.add_edge(vertex, (vertex + 1U) % 5U);
  }

  REQUIRE(!distance_hereditary_definition(cycle));
  REQUIRE(!distance_hereditary_pruning(cycle).has_value());
}

TEST_CASE(distance_hereditary_collapses_parallel_edges_loops_and_weights) {
  using namespace distance_hereditary_test_detail;

  Graph simple(4U, false);
  simple.add_edge(0U, 1U);
  simple.add_edge(1U, 2U);
  simple.add_edge(2U, 3U);

  Graph multi(4U, false);
  multi.add_edge(0U, 1U, -7);
  multi.add_edge(0U, 1U, 99);
  multi.add_edge(1U, 2U, 5);
  multi.add_edge(2U, 3U, -123);
  multi.add_edge(2U, 2U, 42);
  multi.add_edge(3U, 3U, -1);

  const auto simple_pruning = distance_hereditary_pruning(simple);
  const auto multi_pruning = distance_hereditary_pruning(multi);
  REQUIRE(simple_pruning.has_value());
  REQUIRE(multi_pruning.has_value());
  REQUIRE(*simple_pruning == *multi_pruning);
  REQUIRE(valid_distance_hereditary_pruning(multi, *multi_pruning));
}

TEST_CASE(distance_hereditary_exhaustive_simple_graphs_up_to_five_vertices) {
  using namespace distance_hereditary_test_detail;

  for (std::size_t n = 0U; n <= 5U; ++n) {
    const std::size_t edge_count =
        n < 2U ? 0U : n * (n - 1U) / 2U;
    const std::uint64_t graph_count = 1ULL << edge_count;
    for (std::uint64_t mask = 0ULL; mask < graph_count; ++mask) {
      require_matches_definition(graph_from_edge_mask(n, mask));
    }
  }
}

TEST_CASE(distance_hereditary_randomized_definition_differential) {
  using namespace distance_hereditary_test_detail;

  std::mt19937_64 random(0xD157AACEULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(random() % 8U);
    Graph graph(n, false);

    for (Vertex first = 0U; first < n; ++first) {
      if ((random() % 7U) == 0U) {
        graph.add_edge(first, first,
                       static_cast<std::int64_t>(random()));
      }
      for (Vertex second = first + 1U; second < n; ++second) {
        if ((random() & 1ULL) == 0ULL) {
          continue;
        }
        graph.add_edge(first, second,
                       static_cast<std::int64_t>(random()));
        if ((random() % 5U) == 0U) {
          graph.add_edge(first, second,
                         static_cast<std::int64_t>(random()));
        }
      }
    }

    require_matches_definition(graph);
  }
}
