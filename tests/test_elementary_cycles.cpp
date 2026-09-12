#include "algorithms/graphs/elementary_cycles.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

#include "test_framework.hpp"

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::johnson_elementary_cycles;

namespace {

std::vector<std::vector<Vertex>> structural_adjacency(const Graph& graph) {
  std::vector<std::vector<Vertex>> adjacency(graph.vertex_count());
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      adjacency[from].push_back(edge.to);
    }
    std::sort(adjacency[from].begin(), adjacency[from].end());
    adjacency[from].erase(
        std::unique(adjacency[from].begin(), adjacency[from].end()),
        adjacency[from].end());
  }
  return adjacency;
}

std::vector<std::vector<Vertex>> brute_force_cycles(const Graph& graph) {
  const auto adjacency = structural_adjacency(graph);
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<Vertex>> cycles;
  for (Vertex start = 0; start < n; ++start) {
    std::vector<bool> used(n, false);
    std::vector<Vertex> path{start};
    used[start] = true;
    std::function<void(Vertex)> dfs = [&](Vertex current) {
      for (const Vertex next : adjacency[current]) {
        if (next < start) continue;
        if (next == start) {
          cycles.push_back(path);
        } else if (!used[next]) {
          used[next] = true;
          path.push_back(next);
          dfs(next);
          path.pop_back();
          used[next] = false;
        }
      }
    };
    dfs(start);
  }
  std::sort(cycles.begin(), cycles.end());
  cycles.erase(std::unique(cycles.begin(), cycles.end()), cycles.end());
  return cycles;
}

void require_valid_cycles(
    const Graph& graph, const std::vector<std::vector<Vertex>>& cycles) {
  const auto adjacency = structural_adjacency(graph);
  REQUIRE(std::is_sorted(cycles.begin(), cycles.end()));
  for (const auto& cycle : cycles) {
    REQUIRE(!cycle.empty());
    REQUIRE_EQ(cycle.front(), *std::min_element(cycle.begin(), cycle.end()));
    auto unique = cycle;
    std::sort(unique.begin(), unique.end());
    REQUIRE(std::adjacent_find(unique.begin(), unique.end()) == unique.end());
    for (std::size_t i = 0; i < cycle.size(); ++i) {
      const Vertex from = cycle[i];
      const Vertex to = cycle[(i + 1) % cycle.size()];
      REQUIRE(std::binary_search(adjacency[from].begin(), adjacency[from].end(),
                                 to));
    }
  }
}

}  // namespace

TEST_CASE(elementary_cycles_empty_and_reject_undirected) {
  Graph empty(0, true);
  REQUIRE(johnson_elementary_cycles(empty).empty());
  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(johnson_elementary_cycles(undirected),
                    std::invalid_argument);
}

TEST_CASE(elementary_cycles_self_loops_parallel_and_weights_are_structural) {
  Graph graph(4, true);
  graph.add_edge(0, 0, 17);
  graph.add_edge(0, 1, -9);
  graph.add_edge(0, 1, 42);
  graph.add_edge(1, 0, 3);
  graph.add_edge(1, 2, 5);
  graph.add_edge(2, 3, 7);
  graph.add_edge(3, 1, 11);
  graph.add_edge(3, 3, -1);
  const std::vector<std::vector<Vertex>> expected{{0}, {0, 1}, {1, 2, 3},
                                                   {3}};
  const auto actual = johnson_elementary_cycles(graph);
  REQUIRE_EQ(actual, expected);
  require_valid_cycles(graph, actual);
}

TEST_CASE(elementary_cycles_overlapping_shapes_and_determinism) {
  Graph graph(5, true);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(2, 0);
  graph.add_edge(0, 3);
  graph.add_edge(3, 2);
  graph.add_edge(2, 4);
  graph.add_edge(4, 0);
  graph.add_edge(1, 3);
  graph.add_edge(3, 1);
  const auto expected = brute_force_cycles(graph);
  const auto first = johnson_elementary_cycles(graph);
  const auto second = johnson_elementary_cycles(graph);
  REQUIRE_EQ(first, expected);
  REQUIRE_EQ(second, first);
  REQUIRE(first.size() >= 5);
  require_valid_cycles(graph, first);
}

TEST_CASE(elementary_cycles_randomized_exact_differential) {
  std::mt19937_64 rng(0x4A4F484E534F4EULL);
  for (std::size_t trial = 0; trial < 900; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8ULL);
    Graph graph(n, true);
    const std::size_t copies =
        n == 0 ? 0 : static_cast<std::size_t>(rng() % 24ULL);
    for (std::size_t i = 0; i < copies; ++i) {
      const Vertex from = static_cast<Vertex>(rng() % n);
      const Vertex to = static_cast<Vertex>(rng() % n);
      const std::int64_t weight =
          static_cast<std::int64_t>(rng() % 41ULL) - 20;
      graph.add_edge(from, to, weight);
      if ((rng() & 7ULL) == 0ULL) graph.add_edge(from, to, -weight);
    }
    const auto expected = brute_force_cycles(graph);
    const auto actual = johnson_elementary_cycles(graph);
    REQUIRE_EQ(actual, expected);
    require_valid_cycles(graph, actual);
    REQUIRE_EQ(johnson_elementary_cycles(graph), actual);
  }
}
