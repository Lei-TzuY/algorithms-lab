#include "test_framework.hpp"

#include "algorithms/graphs/offline_dynamic_bipartiteness.hpp"
#include "algorithms/graphs/offline_dynamic_connectivity.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::DynamicBipartiteOperation;
using algorithms::graphs::DynamicBipartiteOperationKind;
using algorithms::graphs::DynamicConnectivityOperation;
using algorithms::graphs::DynamicConnectivityOperationKind;
using algorithms::graphs::offline_dynamic_bipartiteness;
using algorithms::graphs::offline_dynamic_connectivity;

using EdgeKey = std::pair<std::size_t, std::size_t>;

[[nodiscard]] EdgeKey canonical_edge(std::size_t first, std::size_t second) {
  if (first <= second) {
    return {first, second};
  }
  return {second, first};
}

[[nodiscard]] bool naive_connected(
    std::size_t vertex_count, const std::map<EdgeKey, std::size_t>& active,
    std::size_t source, std::size_t target) {
  std::vector<std::vector<std::size_t>> adjacency(vertex_count);
  for (const auto& [edge, multiplicity] : active) {
    if (multiplicity == 0 || edge.first == edge.second) {
      continue;
    }
    adjacency[edge.first].push_back(edge.second);
    adjacency[edge.second].push_back(edge.first);
  }

  std::vector<bool> seen(vertex_count, false);
  std::queue<std::size_t> queue;
  seen[source] = true;
  queue.push(source);
  while (!queue.empty()) {
    const std::size_t vertex = queue.front();
    queue.pop();
    for (const std::size_t neighbor : adjacency[vertex]) {
      if (!seen[neighbor]) {
        seen[neighbor] = true;
        queue.push(neighbor);
      }
    }
  }
  return seen[target];
}

[[nodiscard]] bool naive_bipartite(
    std::size_t vertex_count, const std::map<EdgeKey, std::size_t>& active) {
  std::vector<std::vector<std::size_t>> adjacency(vertex_count);
  for (const auto& [edge, multiplicity] : active) {
    if (multiplicity == 0) {
      continue;
    }
    if (edge.first == edge.second) {
      return false;
    }
    adjacency[edge.first].push_back(edge.second);
    adjacency[edge.second].push_back(edge.first);
  }

  std::vector<int> color(vertex_count, -1);
  std::queue<std::size_t> queue;
  for (std::size_t source = 0; source < vertex_count; ++source) {
    if (color[source] != -1) {
      continue;
    }
    color[source] = 0;
    queue.push(source);
    while (!queue.empty()) {
      const std::size_t vertex = queue.front();
      queue.pop();
      for (const std::size_t neighbor : adjacency[vertex]) {
        if (color[neighbor] == -1) {
          color[neighbor] = 1 - color[vertex];
          queue.push(neighbor);
        } else if (color[neighbor] == color[vertex]) {
          return false;
        }
      }
    }
  }
  return true;
}

TEST_CASE(offline_connectivity_add_remove_query_timeline) {
  using Kind = DynamicConnectivityOperationKind;
  const std::vector<DynamicConnectivityOperation> operations{
      {Kind::QueryConnected, 0, 2},
      {Kind::AddEdge, 0, 1},
      {Kind::QueryConnected, 0, 2},
      {Kind::AddEdge, 1, 2},
      {Kind::QueryConnected, 0, 2},
      {Kind::RemoveEdge, 1, 2},
      {Kind::QueryConnected, 0, 2},
      {Kind::RemoveEdge, 1, 0},
      {Kind::QueryConnected, 0, 1},
  };
  const auto result = offline_dynamic_connectivity(3, operations);
  REQUIRE_EQ(result.query_answers,
             (std::vector<bool>{false, false, true, false, false}));

  const std::vector<DynamicConnectivityOperation> empty;
  REQUIRE(offline_dynamic_connectivity(0, empty).query_answers.empty());
}

TEST_CASE(offline_connectivity_multiedges_self_loops_and_validation) {
  using Kind = DynamicConnectivityOperationKind;
  const std::vector<DynamicConnectivityOperation> duplicates{
      {Kind::AddEdge, 0, 1},       {Kind::AddEdge, 1, 0},
      {Kind::RemoveEdge, 0, 1},    {Kind::QueryConnected, 0, 1},
      {Kind::RemoveEdge, 1, 0},    {Kind::QueryConnected, 0, 1},
      {Kind::AddEdge, 0, 0},       {Kind::QueryConnected, 0, 0},
      {Kind::RemoveEdge, 0, 0},
  };
  const auto result = offline_dynamic_connectivity(2, duplicates);
  REQUIRE_EQ(result.query_answers, (std::vector<bool>{true, false, true}));

  const std::vector<DynamicConnectivityOperation> inactive_remove{
      {Kind::RemoveEdge, 0, 1}};
  REQUIRE_THROWS_AS(offline_dynamic_connectivity(2, inactive_remove),
                    std::invalid_argument);

  const std::vector<DynamicConnectivityOperation> invalid_vertex{
      {Kind::QueryConnected, 0, 2}};
  REQUIRE_THROWS_AS(offline_dynamic_connectivity(2, invalid_vertex),
                    std::out_of_range);

  const auto unknown = static_cast<DynamicConnectivityOperationKind>(99);
  const std::vector<DynamicConnectivityOperation> invalid_kind{{unknown, 0, 0}};
  REQUIRE_THROWS_AS(offline_dynamic_connectivity(1, invalid_kind),
                    std::invalid_argument);
}

TEST_CASE(offline_connectivity_randomized_against_rebuilt_graph) {
  using Kind = DynamicConnectivityOperationKind;
  std::mt19937_64 rng(0x0FF11EULL);

  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::size_t vertex_count =
        1 + static_cast<std::size_t>(rng() % 10U);
    std::vector<DynamicConnectivityOperation> operations;
    operations.reserve(120);
    std::map<EdgeKey, std::size_t> active;
    std::vector<EdgeKey> active_copies;
    std::vector<bool> expected;

    for (std::size_t step = 0; step < 120; ++step) {
      const std::size_t first =
          static_cast<std::size_t>(rng() % vertex_count);
      const std::size_t second =
          static_cast<std::size_t>(rng() % vertex_count);
      const std::uint64_t choice = rng() % 100U;

      if (choice < 40U) {
        const EdgeKey edge = canonical_edge(first, second);
        operations.push_back({Kind::AddEdge, first, second});
        ++active[edge];
        active_copies.push_back(edge);
        continue;
      }

      if (choice < 65U && !active_copies.empty()) {
        const std::size_t index =
            static_cast<std::size_t>(rng() % active_copies.size());
        const EdgeKey edge = active_copies[index];
        operations.push_back({Kind::RemoveEdge, edge.first, edge.second});
        active_copies[index] = active_copies.back();
        active_copies.pop_back();
        auto count = active.find(edge);
        REQUIRE(count != active.end());
        --count->second;
        if (count->second == 0) {
          active.erase(count);
        }
        continue;
      }

      operations.push_back({Kind::QueryConnected, first, second});
      expected.push_back(
          naive_connected(vertex_count, active, first, second));
    }

    const auto actual =
        offline_dynamic_connectivity(vertex_count, operations).query_answers;
    REQUIRE_EQ(actual, expected);
  }
}

TEST_CASE(offline_dynamic_bipartiteness_timeline_and_recovery) {
  using Kind = DynamicBipartiteOperationKind;
  const std::vector<DynamicBipartiteOperation> operations{
      {Kind::QueryBipartite},
      {Kind::AddEdge, 0, 1},
      {Kind::AddEdge, 1, 2},
      {Kind::QueryBipartite},
      {Kind::AddEdge, 2, 0},
      {Kind::QueryBipartite},
      {Kind::AddEdge, 3, 4},
      {Kind::QueryBipartite},
      {Kind::RemoveEdge, 0, 2},
      {Kind::QueryBipartite},
      {Kind::AddEdge, 2, 3},
      {Kind::AddEdge, 4, 0},
      {Kind::QueryBipartite},
  };
  const auto result = offline_dynamic_bipartiteness(5, operations);
  REQUIRE_EQ(result.query_answers,
             (std::vector<bool>{true, true, false, false, true, false}));

  const std::vector<DynamicBipartiteOperation> empty_graph_query{
      {Kind::QueryBipartite}};
  REQUIRE_EQ(offline_dynamic_bipartiteness(0, empty_graph_query).query_answers,
             (std::vector<bool>{true}));
}

TEST_CASE(offline_dynamic_bipartiteness_multiedges_loops_and_validation) {
  using Kind = DynamicBipartiteOperationKind;
  const std::vector<DynamicBipartiteOperation> duplicates{
      {Kind::AddEdge, 0, 1},
      {Kind::AddEdge, 1, 0},
      {Kind::QueryBipartite},
      {Kind::RemoveEdge, 0, 1},
      {Kind::QueryBipartite},
      {Kind::AddEdge, 0, 0},
      {Kind::QueryBipartite},
      {Kind::RemoveEdge, 0, 0},
      {Kind::QueryBipartite},
      {Kind::RemoveEdge, 1, 0},
      {Kind::QueryBipartite},
  };
  REQUIRE_EQ(offline_dynamic_bipartiteness(2, duplicates).query_answers,
             (std::vector<bool>{true, true, false, true, true}));

  const std::vector<DynamicBipartiteOperation> inactive_remove{
      {Kind::RemoveEdge, 0, 1}};
  REQUIRE_THROWS_AS(offline_dynamic_bipartiteness(2, inactive_remove),
                    std::invalid_argument);

  const std::vector<DynamicBipartiteOperation> invalid_vertex{
      {Kind::AddEdge, 0, 2}};
  REQUIRE_THROWS_AS(offline_dynamic_bipartiteness(2, invalid_vertex),
                    std::out_of_range);

  const auto unknown = static_cast<DynamicBipartiteOperationKind>(99);
  const std::vector<DynamicBipartiteOperation> invalid_kind{{unknown, 0, 0}};
  REQUIRE_THROWS_AS(offline_dynamic_bipartiteness(1, invalid_kind),
                    std::invalid_argument);
}

TEST_CASE(offline_dynamic_bipartiteness_randomized_against_rebuilt_coloring) {
  using Kind = DynamicBipartiteOperationKind;
  std::mt19937_64 rng(0xB1A2A17EULL);

  for (std::size_t trial = 0; trial < 400; ++trial) {
    const std::size_t vertex_count =
        1 + static_cast<std::size_t>(rng() % 10U);
    std::vector<DynamicBipartiteOperation> operations;
    operations.reserve(120);
    std::map<EdgeKey, std::size_t> active;
    std::vector<EdgeKey> active_copies;
    std::vector<bool> expected;

    for (std::size_t step = 0; step < 120; ++step) {
      const std::size_t first =
          static_cast<std::size_t>(rng() % vertex_count);
      const std::size_t second =
          static_cast<std::size_t>(rng() % vertex_count);
      const std::uint64_t choice = rng() % 100U;

      if (choice < 42U) {
        const EdgeKey edge = canonical_edge(first, second);
        operations.push_back({Kind::AddEdge, first, second});
        ++active[edge];
        active_copies.push_back(edge);
        continue;
      }

      if (choice < 67U && !active_copies.empty()) {
        const std::size_t index =
            static_cast<std::size_t>(rng() % active_copies.size());
        const EdgeKey edge = active_copies[index];
        operations.push_back({Kind::RemoveEdge, edge.first, edge.second});
        active_copies[index] = active_copies.back();
        active_copies.pop_back();
        auto count = active.find(edge);
        REQUIRE(count != active.end());
        --count->second;
        if (count->second == 0) {
          active.erase(count);
        }
        continue;
      }

      operations.push_back({Kind::QueryBipartite});
      expected.push_back(naive_bipartite(vertex_count, active));
    }

    const auto actual =
        offline_dynamic_bipartiteness(vertex_count, operations).query_answers;
    REQUIRE_EQ(actual, expected);
  }
}

}  // namespace
