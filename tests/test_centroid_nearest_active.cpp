#include "algorithms/graphs/centroid_nearest_active.hpp"
#include "algorithms/graphs/graph.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::graphs::CentroidNearestActiveIndex;
using algorithms::graphs::Graph;
using algorithms::graphs::NearestActiveVertex;
using algorithms::graphs::Vertex;

namespace {

std::optional<NearestActiveVertex> nearest_active_oracle(
    const Graph& graph, const std::vector<bool>& active, Vertex source) {
  graph.validate_vertex(source);

  const std::size_t unreachable = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> distance(graph.vertex_count(), unreachable);
  std::queue<Vertex> pending;
  distance[source] = 0U;
  pending.push(source);

  while (!pending.empty()) {
    const Vertex vertex = pending.front();
    pending.pop();
    for (const auto& edge : graph.neighbors(vertex)) {
      if (distance[edge.to] != unreachable) {
        continue;
      }
      distance[edge.to] = distance[vertex] + 1U;
      pending.push(edge.to);
    }
  }

  std::optional<NearestActiveVertex> best;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if (!active[vertex]) {
      continue;
    }
    const NearestActiveVertex candidate{vertex, distance[vertex]};
    if (!best.has_value() || candidate.distance < best->distance ||
        (candidate.distance == best->distance &&
         candidate.vertex < best->vertex)) {
      best = candidate;
    }
  }
  return best;
}

}  // namespace

TEST_CASE(centroid_nearest_active_basic_and_ties) {
  Graph empty(0U, false);
  CentroidNearestActiveIndex empty_index(empty);
  REQUIRE_EQ(empty_index.vertex_count(), 0U);
  REQUIRE_THROWS_AS(empty_index.nearest_active(0U), std::out_of_range);

  Graph graph(5U, false);
  graph.add_edge(0U, 1U, 99);
  graph.add_edge(1U, 2U, -4);
  graph.add_edge(2U, 3U, 7);
  graph.add_edge(3U, 4U, 0);

  CentroidNearestActiveIndex index(graph);
  REQUIRE(!index.nearest_active(2U).has_value());
  REQUIRE(index.activate(0U));
  REQUIRE(!index.activate(0U));
  REQUIRE(index.activate(4U));
  REQUIRE_EQ(index.nearest_active(2U),
             std::optional<NearestActiveVertex>(NearestActiveVertex{0U, 2U}));
  REQUIRE_EQ(index.nearest_active(3U),
             std::optional<NearestActiveVertex>(NearestActiveVertex{4U, 1U}));
  REQUIRE(index.deactivate(0U));
  REQUIRE(!index.deactivate(0U));
  REQUIRE_EQ(index.nearest_active(2U),
             std::optional<NearestActiveVertex>(NearestActiveVertex{4U, 2U}));
}

TEST_CASE(centroid_nearest_active_rejects_non_trees) {
  Graph directed(2U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(CentroidNearestActiveIndex(directed), std::invalid_argument);

  Graph loop(1U, false);
  loop.add_edge(0U, 0U);
  REQUIRE_THROWS_AS(CentroidNearestActiveIndex(loop), std::invalid_argument);

  Graph parallel(2U, false);
  parallel.add_edge(0U, 1U);
  parallel.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(CentroidNearestActiveIndex(parallel), std::invalid_argument);

  Graph cycle(3U, false);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 2U);
  cycle.add_edge(2U, 0U);
  REQUIRE_THROWS_AS(CentroidNearestActiveIndex(cycle), std::invalid_argument);

  Graph disconnected(4U, false);
  disconnected.add_edge(0U, 1U);
  disconnected.add_edge(2U, 3U);
  REQUIRE_THROWS_AS(CentroidNearestActiveIndex(disconnected),
                    std::invalid_argument);
}

TEST_CASE(centroid_decomposition_witness_and_deep_chain) {
  constexpr std::size_t vertex_count = 4096U;
  Graph graph(vertex_count, false);
  for (Vertex vertex = 1U; vertex < graph.vertex_count(); ++vertex) {
    graph.add_edge(vertex - 1U, vertex);
  }

  CentroidNearestActiveIndex index(graph);
  const auto& parent = index.centroid_parent();
  std::size_t root_count = 0U;
  for (const auto centroid_parent : parent) {
    if (!centroid_parent.has_value()) {
      ++root_count;
    } else {
      REQUIRE(*centroid_parent < graph.vertex_count());
    }
  }
  REQUIRE_EQ(root_count, 1U);

  REQUIRE(index.activate(0U));
  REQUIRE(index.activate(vertex_count - 1U));
  REQUIRE_EQ(index.nearest_active(2047U),
             std::optional<NearestActiveVertex>(
                 NearestActiveVertex{0U, 2047U}));
  REQUIRE_EQ(index.nearest_active(2048U),
             std::optional<NearestActiveVertex>(NearestActiveVertex{
                 vertex_count - 1U, 2047U}));
}

TEST_CASE(centroid_nearest_active_randomized_bfs_differential) {
  std::mt19937_64 random(0xC3A7D01DULL);

  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 60U);
    Graph graph(vertex_count, false);
    for (Vertex vertex = 1U; vertex < vertex_count; ++vertex) {
      const Vertex parent = static_cast<Vertex>(random() % vertex);
      const std::int64_t ignored_weight =
          static_cast<std::int64_t>(random() % 201U) - 100;
      graph.add_edge(parent, vertex, ignored_weight);
    }

    CentroidNearestActiveIndex index(graph);
    std::vector<bool> active(vertex_count, false);

    for (int step = 0; step < 180; ++step) {
      const Vertex vertex = static_cast<Vertex>(random() % vertex_count);
      const unsigned operation = static_cast<unsigned>(random() % 3U);

      if (operation == 0U) {
        const bool changed = index.activate(vertex);
        REQUIRE(changed != active[vertex]);
        active[vertex] = true;
      } else if (operation == 1U) {
        const bool changed = index.deactivate(vertex);
        REQUIRE(changed == active[vertex]);
        active[vertex] = false;
      } else {
        REQUIRE_EQ(index.nearest_active(vertex),
                   nearest_active_oracle(graph, active, vertex));
      }
      REQUIRE_EQ(index.is_active(vertex), active[vertex]);
    }

    for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
      REQUIRE_EQ(index.nearest_active(vertex),
                 nearest_active_oracle(graph, active, vertex));
    }
  }
}
