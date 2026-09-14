#pragma once

#include "algorithms/graphs/dag_path_cover.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

std::vector<std::pair<algorithms::graphs::Vertex, algorithms::graphs::Vertex>>
dag_path_cover_unique_arcs(const algorithms::graphs::Graph& graph) {
  std::set<std::pair<algorithms::graphs::Vertex, algorithms::graphs::Vertex>> seen;
  for (algorithms::graphs::Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      seen.insert({from, edge.to});
    }
  }
  return {seen.begin(), seen.end()};
}

std::size_t dag_path_cover_exhaustive_max_link_count(
    const algorithms::graphs::Graph& graph) {
  const auto arcs = dag_path_cover_unique_arcs(graph);
  if (arcs.size() >= std::numeric_limits<std::size_t>::digits) {
    throw std::runtime_error("DAG path-cover oracle mask is too wide");
  }

  const std::size_t limit = std::size_t{1} << arcs.size();
  std::size_t best = 0;
  for (std::size_t mask = 0; mask < limit; ++mask) {
    std::vector<bool> has_successor(graph.vertex_count(), false);
    std::vector<bool> has_predecessor(graph.vertex_count(), false);
    std::size_t chosen = 0;
    bool valid = true;

    for (std::size_t index = 0; index < arcs.size(); ++index) {
      if ((mask & (std::size_t{1} << index)) == 0) {
        continue;
      }
      const auto [from, to] = arcs[index];
      if (has_successor[from] || has_predecessor[to]) {
        valid = false;
        break;
      }
      has_successor[from] = true;
      has_predecessor[to] = true;
      ++chosen;
    }
    if (valid) {
      best = std::max(best, chosen);
    }
  }
  return best;
}

void dag_path_cover_replay(const algorithms::graphs::Graph& graph,
                           const algorithms::graphs::DagPathCoverResult& result) {
  REQUIRE_EQ(result.path_count, result.paths.size());
  REQUIRE_EQ(result.path_count + result.matching_cardinality,
             graph.vertex_count());

  std::vector<bool> seen(graph.vertex_count(), false);
  std::size_t edge_count = 0;
  for (const auto& path : result.paths) {
    REQUIRE(!path.vertices.empty());
    REQUIRE_EQ(path.edges.size() + 1, path.vertices.size());
    for (std::size_t index = 0; index < path.vertices.size(); ++index) {
      const auto vertex = path.vertices[index];
      REQUIRE(vertex < graph.vertex_count());
      REQUIRE(!seen[vertex]);
      seen[vertex] = true;
      if (index == 0) {
        continue;
      }

      const auto& witness = path.edges[index - 1];
      REQUIRE_EQ(witness.from, path.vertices[index - 1]);
      REQUIRE_EQ(witness.to, vertex);
      const auto& adjacency = graph.neighbors(witness.from);
      REQUIRE(witness.adjacency_index < adjacency.size());
      REQUIRE_EQ(adjacency[witness.adjacency_index].to, witness.to);
      REQUIRE_EQ(adjacency[witness.adjacency_index].weight, witness.weight);
      ++edge_count;
    }
  }

  REQUIRE(std::all_of(seen.begin(), seen.end(),
                      [](bool value) { return value; }));
  REQUIRE_EQ(edge_count, result.matching_cardinality);
}

TEST_CASE(dag_path_cover_distinguishes_original_arcs_from_dilworth_reachability) {
  algorithms::graphs::Graph graph(5, true);
  graph.add_edge(0, 2, 11);
  graph.add_edge(1, 2, 12);
  graph.add_edge(2, 3, 13);
  graph.add_edge(2, 4, 14);

  const auto result = algorithms::graphs::minimum_dag_path_cover(graph);
  REQUIRE_EQ(result.path_count, std::size_t{3});
  REQUIRE_EQ(result.matching_cardinality, std::size_t{2});
  dag_path_cover_replay(graph, result);
}

TEST_CASE(dag_path_cover_parallel_arc_witness_is_first_insertion_order_copy) {
  algorithms::graphs::Graph graph(3, true);
  graph.add_edge(0, 1, 77);
  graph.add_edge(0, 1, -99);
  graph.add_edge(1, 2, 5);

  const auto first = algorithms::graphs::minimum_dag_path_cover(graph);
  const auto second = algorithms::graphs::minimum_dag_path_cover(graph);
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first.path_count, std::size_t{1});
  REQUIRE_EQ(first.paths.size(), std::size_t{1});
  REQUIRE_EQ(first.paths[0].vertices,
             std::vector<algorithms::graphs::Vertex>({0, 1, 2}));
  REQUIRE_EQ(first.paths[0].edges[0].adjacency_index, std::size_t{0});
  REQUIRE_EQ(first.paths[0].edges[0].weight,
             static_cast<algorithms::graphs::Weight>(77));
  dag_path_cover_replay(graph, first);
}

TEST_CASE(dag_path_cover_validates_shape_and_handles_trivial_dags) {
  algorithms::graphs::Graph empty(0, true);
  const auto empty_result = algorithms::graphs::minimum_dag_path_cover(empty);
  REQUIRE_EQ(empty_result.path_count, std::size_t{0});
  REQUIRE(empty_result.paths.empty());

  algorithms::graphs::Graph isolated(4, true);
  const auto isolated_result =
      algorithms::graphs::minimum_dag_path_cover(isolated);
  REQUIRE_EQ(isolated_result.path_count, std::size_t{4});
  dag_path_cover_replay(isolated, isolated_result);

  algorithms::graphs::Graph cycle(2, true);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_dag_path_cover(cycle),
                    std::invalid_argument);

  algorithms::graphs::Graph self_loop(1, true);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_dag_path_cover(self_loop),
                    std::invalid_argument);

  algorithms::graphs::Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::minimum_dag_path_cover(undirected),
                    std::invalid_argument);
}

TEST_CASE(dag_path_cover_randomized_differential_against_arc_subset_oracle) {
  std::mt19937_64 rng(0xDAD0CAFEULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(rng() % std::uint64_t{10});
    algorithms::graphs::Graph graph(vertex_count, true);
    std::size_t unique_arc_count = 0;

    for (algorithms::graphs::Vertex from = 0; from < vertex_count; ++from) {
      for (algorithms::graphs::Vertex to = from + 1; to < vertex_count; ++to) {
        if (unique_arc_count >= 12 ||
            (rng() % std::uint64_t{100}) >= std::uint64_t{28}) {
          continue;
        }
        const std::size_t copies =
            1 + static_cast<std::size_t>(rng() % std::uint64_t{3});
        for (std::size_t copy = 0; copy < copies; ++copy) {
          static_cast<void>(copy);
          const auto weight = static_cast<std::int64_t>(
                                  rng() % std::uint64_t{201}) -
                              100;
          graph.add_edge(from, to, weight);
        }
        ++unique_arc_count;
      }
    }

    const auto result = algorithms::graphs::minimum_dag_path_cover(graph);
    const std::size_t best_links =
        dag_path_cover_exhaustive_max_link_count(graph);
    REQUIRE_EQ(result.matching_cardinality, best_links);
    REQUIRE_EQ(result.path_count, vertex_count - best_links);
    dag_path_cover_replay(graph, result);
    REQUIRE_EQ(result, algorithms::graphs::minimum_dag_path_cover(graph));
  }
}

}  // namespace
