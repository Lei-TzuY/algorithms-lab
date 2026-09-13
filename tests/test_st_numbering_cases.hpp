#pragma once

#include "algorithms/graphs/st_numbering.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace st_numbering_test_detail {
using algorithms::graphs::Graph;
using algorithms::graphs::STNumberingResult;
using algorithms::graphs::STNumberingStatus;
using algorithms::graphs::Vertex;

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

inline bool adjacent(const Graph& graph, Vertex first, Vertex second) {
  for (const auto& edge : graph.neighbors(first)) {
    if (edge.to == second) {
      return true;
    }
  }
  return false;
}

inline bool replay(const Graph& graph, const STNumberingResult& result) {
  const std::size_t vertex_count = graph.vertex_count();
  if (result.status != STNumberingStatus::success ||
      result.order.size() != vertex_count || result.rank.size() != vertex_count ||
      vertex_count < 3) {
    return false;
  }

  std::vector<bool> seen(vertex_count, false);
  for (std::size_t index = 0; index < result.order.size(); ++index) {
    const Vertex vertex = result.order[index];
    if (vertex >= vertex_count || seen[vertex] || result.rank[vertex] != index) {
      return false;
    }
    seen[vertex] = true;
  }

  const Vertex source = result.order.front();
  const Vertex sink = result.order.back();
  if (!adjacent(graph, source, sink)) {
    return false;
  }

  for (std::size_t index = 1; index + 1 < result.order.size(); ++index) {
    const Vertex vertex = result.order[index];
    bool has_lower = false;
    bool has_higher = false;
    for (const auto& edge : graph.neighbors(vertex)) {
      has_lower = has_lower || result.rank[edge.to] < index;
      has_higher = has_higher || result.rank[edge.to] > index;
    }
    if (!has_lower || !has_higher) {
      return false;
    }
  }
  return true;
}

}  // namespace st_numbering_test_detail

TEST_CASE(st_numbering_rejects_open_ear_out_of_domain_graphs) {
  using namespace st_numbering_test_detail;
  Graph directed(3, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::st_numbering(directed),
                    std::invalid_argument);

  Graph self_loop(3, false);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::st_numbering(self_loop),
                    std::invalid_argument);

  Graph parallel(3, false);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::st_numbering(parallel),
                    std::invalid_argument);
}

TEST_CASE(st_numbering_reports_non_biconnected_graphs) {
  using namespace st_numbering_test_detail;
  Graph path(3, false);
  path.add_edge(0, 1);
  path.add_edge(1, 2);
  const auto result = algorithms::graphs::st_numbering(path);
  REQUIRE_EQ(result.status, STNumberingStatus::not_two_vertex_connected);
  REQUIRE_EQ(result.blocking_articulation, std::optional<Vertex>{1});
  REQUIRE(result.order.empty());
  REQUIRE(result.rank.empty());
}

TEST_CASE(st_numbering_replays_deterministic_bipolar_ordering) {
  using namespace st_numbering_test_detail;
  Graph graph(5, false);
  graph.add_edge(0, 1, 17);
  graph.add_edge(1, 2, -9);
  graph.add_edge(2, 0, 4);
  graph.add_edge(0, 3, 5);
  graph.add_edge(3, 4, 6);
  graph.add_edge(4, 1, 7);
  graph.add_edge(2, 4, 99);

  const auto first = algorithms::graphs::st_numbering(graph);
  const auto second = algorithms::graphs::st_numbering(graph);
  REQUIRE_EQ(first.status, STNumberingStatus::success);
  REQUIRE(replay(graph, first));
  REQUIRE_EQ(first, second);
}

TEST_CASE(st_numbering_randomized_differential_against_vertex_removal_oracle) {
  using namespace st_numbering_test_detail;
  std::mt19937_64 random(0x57A11B1B0ULL);

  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = trial % 9;
    Graph graph(vertex_count, false);
    for (Vertex first = 0; first < vertex_count; ++first) {
      for (Vertex second = first + 1; second < vertex_count; ++second) {
        if ((random() % 1000U) < 340U) {
          const std::int64_t weight =
              static_cast<std::int64_t>(random() % 41U) - 20;
          graph.add_edge(first, second, weight);
        }
      }
    }

    const bool expected = exact_two_vertex_connected(graph);
    const auto result = algorithms::graphs::st_numbering(graph);
    REQUIRE_EQ(result.status == STNumberingStatus::success, expected);
    if (expected) {
      REQUIRE(replay(graph, result));
    } else if (result.blocking_articulation.has_value()) {
      REQUIRE(*result.blocking_articulation < vertex_count);
      REQUIRE(!connected_after_removing(graph, *result.blocking_articulation));
    }
  }
}
