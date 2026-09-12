#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/incremental_topological_order.hpp"

using namespace algorithms::graphs;

namespace {
bool oracle_has_cycle(std::size_t vertex_count,
                      const std::vector<std::pair<Vertex, Vertex>>& edges) {
  std::vector<std::vector<Vertex>> adjacency(vertex_count);
  std::vector<std::size_t> indegree(vertex_count, 0U);
  for (const auto& [from, to] : edges) {
    adjacency[from].push_back(to);
    ++indegree[to];
  }
  std::vector<Vertex> ready;
  ready.reserve(vertex_count);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (indegree[vertex] == 0U) {
      ready.push_back(vertex);
    }
  }
  std::size_t cursor = 0;
  while (cursor < ready.size()) {
    const Vertex vertex = ready[cursor++];
    for (const Vertex next : adjacency[vertex]) {
      --indegree[next];
      if (indegree[next] == 0U) {
        ready.push_back(next);
      }
    }
  }
  return ready.size() != vertex_count;
}

void require_valid_order(
    const IncrementalTopologicalOrder& index,
    const std::vector<std::pair<Vertex, Vertex>>& accepted_edges) {
  REQUIRE_EQ(index.order().size(), index.vertex_count());
  std::vector<unsigned char> seen(index.vertex_count(), 0U);
  for (std::size_t position = 0; position < index.order().size(); ++position) {
    const Vertex vertex = index.order()[position];
    REQUIRE(vertex < index.vertex_count());
    REQUIRE_EQ(seen[vertex], static_cast<unsigned char>(0U));
    seen[vertex] = 1U;
    REQUIRE_EQ(index.position(vertex), position);
  }
  for (const auto& [from, to] : accepted_edges) {
    REQUIRE(index.position(from) < index.position(to));
  }
  REQUIRE_EQ(index.edge_count(), accepted_edges.size());
}
}  // namespace

TEST_CASE(incremental_topological_order_reorders_only_when_needed) {
  IncrementalTopologicalOrder index(6);
  std::vector<std::pair<Vertex, Vertex>> accepted;

  REQUIRE(index.try_add_edge(4, 1));
  accepted.emplace_back(4, 1);
  REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
             (std::vector<Vertex>{0, 2, 3, 4, 1, 5}));
  require_valid_order(index, accepted);

  REQUIRE(index.try_add_edge(1, 5));
  accepted.emplace_back(1, 5);
  REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
             (std::vector<Vertex>{0, 2, 3, 4, 1, 5}));
  require_valid_order(index, accepted);

  REQUIRE(index.try_add_edge(3, 2));
  accepted.emplace_back(3, 2);
  REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
             (std::vector<Vertex>{0, 3, 2, 4, 1, 5}));
  require_valid_order(index, accepted);
}

TEST_CASE(incremental_topological_order_rejects_cycles_transactionally) {
  IncrementalTopologicalOrder index(4);
  std::vector<std::pair<Vertex, Vertex>> accepted;
  REQUIRE(index.try_add_edge(0, 1));
  accepted.emplace_back(0, 1);
  REQUIRE(index.try_add_edge(1, 2));
  accepted.emplace_back(1, 2);
  REQUIRE(index.try_add_edge(2, 3));
  accepted.emplace_back(2, 3);

  const std::vector<Vertex> before(index.order().begin(), index.order().end());
  const std::size_t edge_count = index.edge_count();
  REQUIRE(!index.try_add_edge(3, 0));
  REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
             before);
  REQUIRE_EQ(index.edge_count(), edge_count);
  REQUIRE(!index.try_add_edge(2, 2));
  REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
             before);
  REQUIRE_EQ(index.edge_count(), edge_count);
  require_valid_order(index, accepted);
}

TEST_CASE(incremental_topological_order_supports_parallel_edges_and_validation) {
  IncrementalTopologicalOrder index(3);
  std::vector<std::pair<Vertex, Vertex>> accepted;
  REQUIRE(index.try_add_edge(2, 0));
  accepted.emplace_back(2, 0);
  REQUIRE(index.try_add_edge(2, 0));
  accepted.emplace_back(2, 0);
  require_valid_order(index, accepted);
  REQUIRE_THROWS_AS(index.try_add_edge(3, 0), std::out_of_range);
  REQUIRE_THROWS_AS(index.position(3), std::out_of_range);
}

TEST_CASE(incremental_topological_order_randomized_matches_kahn_cycle_oracle) {
  std::mt19937_64 random(0x1A2B3C4D5E6FULL);
  for (std::size_t trial = 0; trial < 600; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(random() % 14U);
    IncrementalTopologicalOrder index(vertex_count);
    std::vector<std::pair<Vertex, Vertex>> accepted;

    for (std::size_t step = 0; step < 160; ++step) {
      if (vertex_count == 0U) {
        break;
      }
      const Vertex from = static_cast<Vertex>(random() % vertex_count);
      const Vertex to = static_cast<Vertex>(random() % vertex_count);
      auto proposed = accepted;
      proposed.emplace_back(from, to);
      const bool expected =
          from != to && !oracle_has_cycle(vertex_count, proposed);
      const std::vector<Vertex> before(index.order().begin(), index.order().end());
      const std::size_t before_edges = index.edge_count();

      const bool actual = index.try_add_edge(from, to);
      REQUIRE_EQ(actual, expected);
      if (actual) {
        accepted.emplace_back(from, to);
      } else {
        REQUIRE_EQ(index.edge_count(), before_edges);
        REQUIRE_EQ(std::vector<Vertex>(index.order().begin(), index.order().end()),
                   before);
      }
      require_valid_order(index, accepted);
    }
  }
}
