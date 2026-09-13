#pragma once

#include "algorithms/graphs/open_ear_decomposition.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace open_ear_test_detail {
using algorithms::graphs::Graph;
using algorithms::graphs::OpenEarDecompositionResult;
using algorithms::graphs::OpenEarStatus;
using algorithms::graphs::Vertex;
using Pair = std::pair<Vertex, Vertex>;

inline Pair canonical(Vertex first, Vertex second) {
  return first < second ? Pair{first, second} : Pair{second, first};
}

inline std::set<Pair> structural_edges(const Graph& graph) {
  std::set<Pair> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.insert({from, edge.to});
      }
    }
  }
  return edges;
}

inline bool connected_after_removing(const Graph& graph, Vertex removed) {
  const std::size_t vertex_count = graph.vertex_count();
  Vertex start = vertex_count;
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != removed) {
      start = vertex;
      break;
    }
  }
  if (start == vertex_count) {
    return true;
  }

  std::vector<bool> seen(vertex_count, false);
  seen[removed] = true;
  seen[start] = true;
  std::queue<Vertex> queue;
  queue.push(start);
  std::size_t reached = 0;
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    ++reached;
    for (const auto& edge : graph.neighbors(vertex)) {
      if (!seen[edge.to]) {
        seen[edge.to] = true;
        queue.push(edge.to);
      }
    }
  }
  return reached == vertex_count - 1;
}

inline bool exact_two_vertex_connected(const Graph& graph) {
  if (graph.directed() || graph.vertex_count() < 3) {
    return false;
  }
  for (Vertex removed = 0; removed < graph.vertex_count(); ++removed) {
    if (!connected_after_removing(graph, removed)) {
      return false;
    }
  }
  return true;
}

inline bool replay(const Graph& graph,
                   const OpenEarDecompositionResult& result) {
  if (result.status != OpenEarStatus::success || result.ears.empty()) {
    return false;
  }
  const std::set<Pair> all_edges = structural_edges(graph);
  std::set<Pair> used_edges;
  std::vector<bool> present(graph.vertex_count(), false);

  const auto& cycle = result.ears.front().vertices;
  if (cycle.size() < 4 || cycle.front() != cycle.back()) {
    return false;
  }
  std::set<Vertex> cycle_vertices;
  for (std::size_t index = 0; index + 1 < cycle.size(); ++index) {
    const Vertex from = cycle[index];
    const Vertex to = cycle[index + 1];
    const Pair edge = canonical(from, to);
    if (from == to || !all_edges.contains(edge) || used_edges.contains(edge)) {
      return false;
    }
    if (cycle_vertices.contains(from)) {
      return false;
    }
    used_edges.insert(edge);
    cycle_vertices.insert(from);
    present[from] = true;
  }

  for (std::size_t ear_index = 1; ear_index < result.ears.size(); ++ear_index) {
    const auto& path = result.ears[ear_index].vertices;
    if (path.size() < 2 || path.front() == path.back()) {
      return false;
    }
    if (!present[path.front()] || !present[path.back()]) {
      return false;
    }
    std::set<Vertex> internal_vertices;
    for (std::size_t index = 1; index + 1 < path.size(); ++index) {
      if (present[path[index]] || internal_vertices.contains(path[index])) {
        return false;
      }
      internal_vertices.insert(path[index]);
    }
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
      const Pair edge = canonical(path[index], path[index + 1]);
      if (path[index] == path[index + 1] || !all_edges.contains(edge) ||
          used_edges.contains(edge)) {
        return false;
      }
      used_edges.insert(edge);
    }
    for (Vertex vertex : internal_vertices) {
      present[vertex] = true;
    }
  }

  return used_edges == all_edges &&
         std::all_of(present.begin(), present.end(), [](bool value) {
           return value;
         });
}

}  // namespace open_ear_test_detail

TEST_CASE(open_ear_rejects_out_of_domain_graphs) {
  using namespace open_ear_test_detail;
  Graph directed(3, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::open_ear_decomposition(directed),
                    std::invalid_argument);

  Graph self_loop(3, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::open_ear_decomposition(self_loop),
                    std::invalid_argument);

  Graph parallel(3, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::open_ear_decomposition(parallel),
                    std::invalid_argument);
}

TEST_CASE(open_ear_reports_non_biconnected_graphs) {
  using namespace open_ear_test_detail;
  Graph two_vertices(2, false);
  two_vertices.add_edge(0, 1);
  REQUIRE_EQ(algorithms::graphs::open_ear_decomposition(two_vertices).status,
             OpenEarStatus::not_two_vertex_connected);

  Graph path(3, false);
  path.add_edge(0, 1);
  path.add_edge(1, 2);
  const auto result = algorithms::graphs::open_ear_decomposition(path);
  REQUIRE_EQ(result.status, OpenEarStatus::not_two_vertex_connected);
  REQUIRE_EQ(result.blocking_articulation, std::optional<Vertex>{1});
}

TEST_CASE(open_ear_replays_deterministic_constructive_witnesses) {
  using namespace open_ear_test_detail;
  Graph graph(5, false);
  graph.add_edge(0, 1, 17);
  graph.add_edge(1, 2, -9);
  graph.add_edge(2, 0, 4);
  graph.add_edge(0, 3, 5);
  graph.add_edge(3, 4, 6);
  graph.add_edge(4, 1, 7);
  graph.add_edge(2, 4, 99);

  const auto first = algorithms::graphs::open_ear_decomposition(graph);
  const auto second = algorithms::graphs::open_ear_decomposition(graph);
  REQUIRE_EQ(first.status, OpenEarStatus::success);
  REQUIRE(replay(graph, first));
  REQUIRE_EQ(first.ears, second.ears);
}

TEST_CASE(open_ear_randomized_differential_against_vertex_removal_oracle) {
  using namespace open_ear_test_detail;
  std::mt19937_64 random(0xEA4D3C0ULL);
  std::bernoulli_distribution add_edge(0.34);
  std::uniform_int_distribution<int> weight(-20, 20);

  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = trial % 9;
    Graph graph(vertex_count, false);
    for (Vertex first = 0; first < vertex_count; ++first) {
      for (Vertex second = first + 1; second < vertex_count; ++second) {
        if (add_edge(random)) {
          graph.add_edge(first, second,
                         static_cast<std::int64_t>(weight(random)));
        }
      }
    }

    const bool expected = exact_two_vertex_connected(graph);
    const auto result = algorithms::graphs::open_ear_decomposition(graph);
    REQUIRE_EQ(result.status == OpenEarStatus::success, expected);
    if (expected) {
      REQUIRE(replay(graph, result));
    } else if (result.blocking_articulation.has_value()) {
      REQUIRE(*result.blocking_articulation < vertex_count);
      REQUIRE(!connected_after_removing(graph, *result.blocking_articulation));
    }
  }
}
