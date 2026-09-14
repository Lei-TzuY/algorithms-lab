#pragma once

#include "algorithms/graphs/suurballe.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <utility>
#include <vector>

namespace suurballe_test_detail {

using algorithms::graphs::Graph;
using algorithms::graphs::SuurballeResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct OracleEdge {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};
  std::size_t id{};
};

struct OraclePath {
  std::uint64_t cost{};
  std::vector<std::size_t> edge_ids;
};

inline std::vector<OracleEdge> flatten_edges(const Graph& graph) {
  std::vector<OracleEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& adjacency = graph.neighbors(from);
    for (std::size_t index = 0; index < adjacency.size(); ++index) {
      edges.push_back(OracleEdge{from, index, adjacency[index].to,
                                 adjacency[index].weight, edges.size()});
    }
  }
  return edges;
}

inline std::vector<OraclePath> enumerate_simple_paths(const Graph& graph,
                                                       Vertex source,
                                                       Vertex target) {
  const auto edges = flatten_edges(graph);
  std::vector<std::vector<std::size_t>> outgoing(graph.vertex_count());
  for (const auto& edge : edges) {
    outgoing[edge.from].push_back(edge.id);
  }

  std::vector<OraclePath> result;
  std::vector<bool> visited(graph.vertex_count(), false);
  std::vector<std::size_t> current;
  visited[source] = true;

  const auto dfs = [&](auto&& self, Vertex at, std::uint64_t cost) -> void {
    if (at == target) {
      result.push_back(OraclePath{cost, current});
      return;
    }
    for (const std::size_t edge_id : outgoing[at]) {
      const auto& edge = edges[edge_id];
      if (edge.to == at || visited[edge.to]) {
        continue;
      }
      visited[edge.to] = true;
      current.push_back(edge_id);
      self(self, edge.to,
           cost + static_cast<std::uint64_t>(edge.weight));
      current.pop_back();
      visited[edge.to] = false;
    }
  };
  dfs(dfs, source, 0);
  return result;
}

inline std::optional<std::uint64_t> exhaustive_best_pair(const Graph& graph,
                                                          Vertex source,
                                                          Vertex target) {
  const auto paths = enumerate_simple_paths(graph, source, target);
  std::optional<std::uint64_t> best;
  for (std::size_t first_index = 0; first_index < paths.size(); ++first_index) {
    const std::set<std::size_t> first_edges(paths[first_index].edge_ids.begin(),
                                            paths[first_index].edge_ids.end());
    for (std::size_t second_index = first_index + 1;
         second_index < paths.size(); ++second_index) {
      bool edge_disjoint = true;
      for (const std::size_t edge_id : paths[second_index].edge_ids) {
        if (first_edges.contains(edge_id)) {
          edge_disjoint = false;
          break;
        }
      }
      if (!edge_disjoint) {
        continue;
      }
      const std::uint64_t total =
          paths[first_index].cost + paths[second_index].cost;
      if (!best.has_value() || total < *best) {
        best = total;
      }
    }
  }
  return best;
}

inline void replay_result(const Graph& graph, Vertex source, Vertex target,
                          const SuurballeResult& result) {
  std::set<std::pair<Vertex, std::size_t>> used_edges;
  std::uint64_t total = 0;
  for (const auto& path : result.paths) {
    REQUIRE(!path.vertices.empty());
    REQUIRE_EQ(path.vertices.front(), source);
    REQUIRE_EQ(path.vertices.back(), target);
    REQUIRE_EQ(path.vertices.size(), path.edges.size() + 1);

    std::set<Vertex> vertices;
    std::uint64_t path_cost = 0;
    for (std::size_t index = 0; index < path.vertices.size(); ++index) {
      REQUIRE(vertices.insert(path.vertices[index]).second);
      if (index == path.edges.size()) {
        break;
      }
      const auto& witness = path.edges[index];
      REQUIRE_EQ(witness.from, path.vertices[index]);
      REQUIRE_EQ(witness.to, path.vertices[index + 1]);
      const auto& adjacency = graph.neighbors(witness.from);
      REQUIRE(witness.adjacency_index < adjacency.size());
      REQUIRE_EQ(adjacency[witness.adjacency_index].to, witness.to);
      REQUIRE_EQ(adjacency[witness.adjacency_index].weight, witness.weight);
      REQUIRE(used_edges.insert({witness.from, witness.adjacency_index}).second);
      path_cost += static_cast<std::uint64_t>(witness.weight);
    }
    REQUIRE_EQ(path_cost, static_cast<std::uint64_t>(path.total_cost));
    total += path_cost;
  }
  REQUIRE_EQ(total, static_cast<std::uint64_t>(result.total_cost));
}

}  // namespace suurballe_test_detail

TEST_CASE(suurballe_validates_domain_and_global_nonnegative_weights) {
  using namespace suurballe_test_detail;
  Graph undirected(2, false);
  undirected.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
          undirected, 0, 1),
      std::invalid_argument);

  Graph negative(3, true);
  negative.add_edge(0, 1, 1);
  negative.add_edge(2, 2, -1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
          negative, 0, 1),
      std::invalid_argument);

  Graph graph(2, true);
  REQUIRE_THROWS_AS(
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(graph, 0,
                                                                      0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(graph, 0,
                                                                      2),
      std::out_of_range);
}

TEST_CASE(suurballe_reversal_is_required_for_the_optimal_pair) {
  using namespace suurballe_test_detail;
  Graph graph(4, true);
  // 0=s, 1=a, 2=b, 3=t. Insertion order makes the first shortest path
  // s-a-b-t (cost 2). Merely deleting that path destroys the optimum pair;
  // Suurballe must reverse/cancel a->b and return s-a-t plus s-b-t.
  graph.add_edge(0, 1, 1);
  graph.add_edge(0, 2, 1);
  graph.add_edge(1, 2, 0);
  graph.add_edge(1, 3, 1);
  graph.add_edge(2, 3, 1);

  const auto result =
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(graph, 0,
                                                                      3);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->total_cost, 4);
  replay_result(graph, 0, 3, *result);
}

TEST_CASE(suurballe_handles_parallel_edges_self_loops_and_representability) {
  using namespace suurballe_test_detail;
  Graph parallel(2, true);
  parallel.add_edge(0, 1, 1);
  parallel.add_edge(0, 1, 2);
  parallel.add_edge(0, 0, 0);
  const auto result =
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(parallel,
                                                                      0, 1);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->total_cost, 3);
  replay_result(parallel, 0, 1, *result);

  Graph single_path(3, true);
  single_path.add_edge(0, 1, 1);
  single_path.add_edge(1, 2, 1);
  REQUIRE(!algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
               single_path, 0, 2)
               .has_value());

  Graph exact_max(2, true);
  exact_max.add_edge(0, 1, std::numeric_limits<Weight>::max());
  exact_max.add_edge(0, 1, 0);
  const auto max_result =
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(exact_max,
                                                                      0, 1);
  REQUIRE(max_result.has_value());
  REQUIRE_EQ(max_result->total_cost, std::numeric_limits<Weight>::max());

  Graph overflow(2, true);
  overflow.add_edge(0, 1, std::numeric_limits<Weight>::max());
  overflow.add_edge(0, 1, std::numeric_limits<Weight>::max());
  REQUIRE_THROWS_AS(
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(overflow,
                                                                      0, 1),
      std::overflow_error);

  Graph nonoptimal_huge(3, true);
  nonoptimal_huge.add_edge(0, 2, 1);
  nonoptimal_huge.add_edge(0, 2, 2);
  nonoptimal_huge.add_edge(0, 1, std::numeric_limits<Weight>::max());
  nonoptimal_huge.add_edge(1, 2, std::numeric_limits<Weight>::max());
  const auto finite =
      algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
          nonoptimal_huge, 0, 2);
  REQUIRE(finite.has_value());
  REQUIRE_EQ(finite->total_cost, 3);
}

TEST_CASE(suurballe_matches_exhaustive_simple_path_pairs) {
  using namespace suurballe_test_detail;
  std::mt19937_64 random(0x5A11BA11EULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = 2 + static_cast<std::size_t>(random() % 6U);
    Graph graph(vertex_count, true);
    const std::size_t edge_count = static_cast<std::size_t>(random() % 14U);
    for (std::size_t index = 0; index < edge_count; ++index) {
      const Vertex from = static_cast<Vertex>(random() % vertex_count);
      const Vertex to = static_cast<Vertex>(random() % vertex_count);
      const Weight weight = static_cast<Weight>(random() % 11U);
      graph.add_edge(from, to, weight);
    }

    const Vertex source = static_cast<Vertex>(random() % vertex_count);
    Vertex target = static_cast<Vertex>(random() % vertex_count);
    if (target == source) {
      target = (target + 1U) % vertex_count;
    }

    const auto expected = exhaustive_best_pair(graph, source, target);
    const auto actual =
        algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
            graph, source, target);
    if (!expected.has_value()) {
      REQUIRE(!actual.has_value());
      continue;
    }

    REQUIRE(actual.has_value());
    replay_result(graph, source, target, *actual);
    REQUIRE_EQ(static_cast<std::uint64_t>(actual->total_cost), *expected);
    const auto repeated =
        algorithms::graphs::suurballe_two_edge_disjoint_shortest_paths(
            graph, source, target);
    REQUIRE(repeated == actual);
  }
}
