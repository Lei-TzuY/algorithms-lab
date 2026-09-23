#pragma once

#include "algorithms/graphs/strong_connectivity_augmentation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::minimum_strong_connectivity_augmentation;

namespace strong_connectivity_augmentation_test_detail {

inline bool independently_strongly_connected(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n <= 1U) {
    return true;
  }

  const auto reaches_all =
      [&](const bool reverse) {
        std::vector<std::vector<Vertex>> reversed;
        if (reverse) {
          reversed.resize(n);
          for (Vertex from = 0U; from < n; ++from) {
            for (const auto& edge : graph.neighbors(from)) {
              reversed[edge.to].push_back(from);
            }
          }
        }

        std::vector<bool> visited(n, false);
        std::vector<Vertex> stack{0U};
        visited[0U] = true;

        while (!stack.empty()) {
          const Vertex current = stack.back();
          stack.pop_back();

          if (!reverse) {
            for (const auto& edge : graph.neighbors(current)) {
              if (!visited[edge.to]) {
                visited[edge.to] = true;
                stack.push_back(edge.to);
              }
            }
          } else {
            for (const Vertex next : reversed[current]) {
              if (!visited[next]) {
                visited[next] = true;
                stack.push_back(next);
              }
            }
          }
        }

        return std::all_of(
            visited.begin(), visited.end(),
            [](const bool value) { return value; });
      };

  return reaches_all(false) && reaches_all(true);
}

inline Graph with_augmentation(
    const Graph& graph,
    const std::vector<std::pair<Vertex, Vertex>>& added_edges) {
  Graph augmented = graph;
  for (const auto& [from, to] : added_edges) {
    augmented.add_edge(from, to);
  }
  return augmented;
}

inline bool has_direct_edge(
    const Graph& graph, const Vertex from, const Vertex to) {
  for (const auto& edge : graph.neighbors(from)) {
    if (edge.to == to) {
      return true;
    }
  }
  return false;
}

inline std::size_t brute_minimum_added_edges(const Graph& graph) {
  if (independently_strongly_connected(graph)) {
    return 0U;
  }

  const std::size_t n = graph.vertex_count();
  std::vector<std::pair<Vertex, Vertex>> missing;
  for (Vertex from = 0U; from < n; ++from) {
    for (Vertex to = 0U; to < n; ++to) {
      if (from != to && !has_direct_edge(graph, from, to)) {
        missing.emplace_back(from, to);
      }
    }
  }

  std::vector<std::pair<Vertex, Vertex>> chosen;

  const auto exists_solution =
      [&](auto&& self, const std::size_t need,
          const std::size_t next_index) -> bool {
        if (chosen.size() == need) {
          return independently_strongly_connected(
              with_augmentation(graph, chosen));
        }

        const std::size_t remaining =
            need - chosen.size();
        if (missing.size() - next_index < remaining) {
          return false;
        }

        for (std::size_t index = next_index;
             index < missing.size(); ++index) {
          chosen.push_back(missing[index]);
          if (self(self, need, index + 1U)) {
            return true;
          }
          chosen.pop_back();
        }
        return false;
      };

  for (std::size_t count = 1U;
       count <= missing.size(); ++count) {
    chosen.clear();
    if (exists_solution(exists_solution, count, 0U)) {
      return count;
    }
  }

  throw std::logic_error(
      "finite directed graph unexpectedly has no strong augmentation");
}

inline void require_valid_minimum_augmentation(const Graph& graph) {
  const auto result =
      minimum_strong_connectivity_augmentation(graph);

  std::vector<std::pair<Vertex, Vertex>> sorted =
      result.added_edges;
  std::sort(sorted.begin(), sorted.end());
  REQUIRE(
      std::adjacent_find(sorted.begin(), sorted.end()) ==
      sorted.end());

  for (const auto& [from, to] : result.added_edges) {
    REQUIRE(from < graph.vertex_count());
    REQUIRE(to < graph.vertex_count());
    REQUIRE(from != to);
    REQUIRE(!has_direct_edge(graph, from, to));
  }

  REQUIRE(independently_strongly_connected(
      with_augmentation(graph, result.added_edges)));
  REQUIRE_EQ(
      result.added_edges.size(),
      brute_minimum_added_edges(graph));
}

inline Graph graph_from_mask(
    const std::size_t n, std::uint64_t mask) {
  Graph graph(n, true);
  std::size_t bit = 0U;
  for (Vertex from = 0U; from < n; ++from) {
    for (Vertex to = 0U; to < n; ++to) {
      if (from == to) {
        continue;
      }
      if ((mask & (UINT64_C(1) << bit)) != 0U) {
        graph.add_edge(from, to);
      }
      ++bit;
    }
  }
  return graph;
}

}  // namespace strong_connectivity_augmentation_test_detail

TEST_CASE(strong_connectivity_augmentation_empty_single_and_strong) {
  using namespace strong_connectivity_augmentation_test_detail;

  const Graph empty(0U, true);
  REQUIRE(
      minimum_strong_connectivity_augmentation(empty)
          .added_edges.empty());

  const Graph singleton(1U, true);
  REQUIRE(
      minimum_strong_connectivity_augmentation(singleton)
          .added_edges.empty());

  Graph cycle(4U, true);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 3U);
  cycle.add_edge(3U, 0U);

  REQUIRE(independently_strongly_connected(cycle));
  REQUIRE(
      minimum_strong_connectivity_augmentation(cycle)
          .added_edges.empty());
}

TEST_CASE(strong_connectivity_augmentation_known_minimum_witnesses) {
  using namespace strong_connectivity_augmentation_test_detail;

  Graph chain(3U, true);
  chain.add_edge(0U, 1U);
  chain.add_edge(1U, 2U);
  const auto chain_result =
      minimum_strong_connectivity_augmentation(chain);
  REQUIRE_EQ(chain_result.added_edges.size(), 1U);
  const std::vector<std::pair<Vertex, Vertex>> expected_chain{
      {2U, 0U}};
  REQUIRE(chain_result.added_edges == expected_chain);
  require_valid_minimum_augmentation(chain);

  Graph two_isolated(2U, true);
  const auto isolated_result =
      minimum_strong_connectivity_augmentation(two_isolated);
  const std::vector<std::pair<Vertex, Vertex>> expected_isolated{
      {0U, 1U}, {1U, 0U}};
  REQUIRE(isolated_result.added_edges == expected_isolated);
  require_valid_minimum_augmentation(two_isolated);

  Graph out_star(4U, true);
  out_star.add_edge(0U, 1U);
  out_star.add_edge(0U, 2U);
  out_star.add_edge(0U, 3U);
  REQUIRE_EQ(
      minimum_strong_connectivity_augmentation(out_star)
          .added_edges.size(),
      3U);
  require_valid_minimum_augmentation(out_star);

  Graph in_star(4U, true);
  in_star.add_edge(1U, 0U);
  in_star.add_edge(2U, 0U);
  in_star.add_edge(3U, 0U);
  REQUIRE_EQ(
      minimum_strong_connectivity_augmentation(in_star)
          .added_edges.size(),
      3U);
  require_valid_minimum_augmentation(in_star);
}

TEST_CASE(strong_connectivity_augmentation_uses_original_vertex_representatives) {
  using namespace strong_connectivity_augmentation_test_detail;

  Graph graph(4U, true);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 0U);
  graph.add_edge(2U, 3U);
  graph.add_edge(3U, 2U);
  graph.add_edge(1U, 2U);

  const auto result =
      minimum_strong_connectivity_augmentation(graph);
  REQUIRE_EQ(result.added_edges.size(), 1U);
  const std::vector<std::pair<Vertex, Vertex>> expected{
      {2U, 0U}};
  REQUIRE(result.added_edges == expected);
  require_valid_minimum_augmentation(graph);
}

TEST_CASE(strong_connectivity_augmentation_ignores_loops_and_parallel_edges) {
  using namespace strong_connectivity_augmentation_test_detail;

  Graph graph(3U, true);
  graph.add_edge(0U, 0U);
  graph.add_edge(0U, 1U);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(1U, 2U);

  require_valid_minimum_augmentation(graph);
}

TEST_CASE(strong_connectivity_augmentation_rejects_undirected_graphs) {
  const Graph graph(3U, false);
  REQUIRE_THROWS_AS(
      minimum_strong_connectivity_augmentation(graph),
      std::invalid_argument);
}

TEST_CASE(strong_connectivity_augmentation_exhaustive_three_vertex_oracle) {
  using namespace strong_connectivity_augmentation_test_detail;

  // There are 3*2 = 6 possible non-loop directed edges.
  for (std::uint64_t mask = 0U; mask < UINT64_C(64); ++mask) {
    require_valid_minimum_augmentation(
        graph_from_mask(3U, mask));
  }
}

TEST_CASE(strong_connectivity_augmentation_random_four_vertex_oracle) {
  using namespace strong_connectivity_augmentation_test_detail;

  std::mt19937_64 random(0x57A0C0DEULL);
  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    const std::uint64_t mask =
        random() & ((UINT64_C(1) << 12U) - UINT64_C(1));
    require_valid_minimum_augmentation(
        graph_from_mask(4U, mask));
  }
}
