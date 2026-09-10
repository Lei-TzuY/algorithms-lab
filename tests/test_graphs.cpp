#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/traversal.hpp"

using namespace algorithms::graphs;

TEST_CASE(graph_validation_direction_parallel_and_self_loop) {
  Graph undirected(3, false);
  undirected.add_edge(0, 1);
  undirected.add_edge(0, 1, 7);
  undirected.add_edge(2, 2, 4);
  REQUIRE_EQ(undirected.neighbors(0).size(), std::size_t{2});
  REQUIRE_EQ(undirected.neighbors(1).size(), std::size_t{2});
  REQUIRE_EQ(undirected.neighbors(2).size(), std::size_t{1});
  REQUIRE_THROWS_AS(undirected.add_edge(3, 0), std::out_of_range);
  REQUIRE_THROWS_AS(undirected.neighbors(3), std::out_of_range);
}

TEST_CASE(bfs_shortest_path_reconstruction_and_disconnected_vertices) {
  Graph graph(7, false);
  graph.add_edge(0, 1);
  graph.add_edge(0, 2);
  graph.add_edge(1, 3);
  graph.add_edge(2, 3);
  graph.add_edge(3, 4);
  graph.add_edge(5, 6);

  const auto bfs = breadth_first_search(graph, 0);
  REQUIRE_EQ(bfs.order, (std::vector<Vertex>{0, 1, 2, 3, 4}));
  REQUIRE_EQ(*bfs.distance[4], std::size_t{3});
  REQUIRE(!bfs.distance[5].has_value());

  const auto path = shortest_unweighted_path(graph, 0, 4);
  REQUIRE(path.has_value());
  REQUIRE_EQ(*path, (std::vector<Vertex>{0, 1, 3, 4}));
  REQUIRE(!shortest_unweighted_path(graph, 0, 6).has_value());
}

TEST_CASE(dfs_cycles_reachability_and_components) {
  Graph graph(7, false);
  graph.add_edge(0, 1);
  graph.add_edge(1, 2);
  graph.add_edge(2, 0);
  graph.add_edge(3, 4);
  graph.add_edge(5, 5);

  REQUIRE_EQ(depth_first_search(graph, 0),
             (std::vector<Vertex>{0, 1, 2}));
  REQUIRE(has_cycle(graph));
  REQUIRE(is_reachable(graph, 0, 2));
  REQUIRE(!is_reachable(graph, 0, 4));

  const auto components = connected_components(graph);
  REQUIRE_EQ(components.size(), std::size_t{4});
  REQUIRE_EQ(components[0], (std::vector<Vertex>{0, 1, 2}));
  REQUIRE_EQ(components[1], (std::vector<Vertex>{3, 4}));
  REQUIRE_EQ(components[2], (std::vector<Vertex>{5}));
  REQUIRE_EQ(components[3], (std::vector<Vertex>{6}));
}

TEST_CASE(undirected_parallel_edge_is_cycle) {
  Graph graph(2, false);
  graph.add_edge(0, 1);
  graph.add_edge(0, 1);
  REQUIRE(has_cycle(graph));
}

namespace {
struct OracleEdge {
  Vertex first;
  Vertex second;
  Weight weight;
};

std::size_t oracle_component_count(
    std::size_t vertex_count, const std::vector<OracleEdge>& edges,
    std::optional<std::size_t> removed_edge,
    std::optional<Vertex> removed_vertex) {
  std::vector<std::vector<Vertex>> adjacency(vertex_count);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (removed_edge.has_value() && index == *removed_edge) {
      continue;
    }
    const OracleEdge& edge = edges[index];
    if (removed_vertex.has_value() &&
        (edge.first == *removed_vertex || edge.second == *removed_vertex)) {
      continue;
    }
    if (edge.first == edge.second) {
      continue;
    }
    adjacency[edge.first].push_back(edge.second);
    adjacency[edge.second].push_back(edge.first);
  }

  std::vector<bool> seen(vertex_count, false);
  std::size_t count = 0;
  for (Vertex seed = 0; seed < vertex_count; ++seed) {
    if ((removed_vertex.has_value() && seed == *removed_vertex) || seen[seed]) {
      continue;
    }
    ++count;
    std::vector<Vertex> stack{seed};
    seen[seed] = true;
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      for (const Vertex next : adjacency[vertex]) {
        if (!seen[next]) {
          seen[next] = true;
          stack.push_back(next);
        }
      }
    }
  }
  return count;
}

std::vector<std::size_t> oracle_bridge_indices(
    std::size_t vertex_count, const std::vector<OracleEdge>& edges) {
  const std::size_t baseline = oracle_component_count(
      vertex_count, edges, std::nullopt, std::nullopt);
  std::vector<std::size_t> result;
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (oracle_component_count(vertex_count, edges, index, std::nullopt) >
        baseline) {
      result.push_back(index);
    }
  }
  return result;
}

std::vector<Vertex> oracle_articulation_vertices(
    std::size_t vertex_count, const std::vector<OracleEdge>& edges) {
  const std::size_t baseline = oracle_component_count(
      vertex_count, edges, std::nullopt, std::nullopt);
  std::vector<Vertex> result;
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (oracle_component_count(vertex_count, edges, std::nullopt, vertex) >
        baseline) {
      result.push_back(vertex);
    }
  }
  return result;
}

std::vector<std::size_t> oracle_bridge_components(
    std::size_t vertex_count, const std::vector<OracleEdge>& edges,
    const std::vector<std::size_t>& bridge_indices) {
  std::vector<bool> bridge(edges.size(), false);
  for (const std::size_t index : bridge_indices) {
    bridge[index] = true;
  }

  std::vector<std::vector<Vertex>> adjacency(vertex_count);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (bridge[index] || edges[index].first == edges[index].second) {
      continue;
    }
    adjacency[edges[index].first].push_back(edges[index].second);
    adjacency[edges[index].second].push_back(edges[index].first);
  }

  const std::size_t none = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> component_of(vertex_count, none);
  std::size_t component = 0;
  for (Vertex seed = 0; seed < vertex_count; ++seed) {
    if (component_of[seed] != none) {
      continue;
    }
    component_of[seed] = component;
    std::vector<Vertex> stack{seed};
    while (!stack.empty()) {
      const Vertex vertex = stack.back();
      stack.pop_back();
      for (const Vertex next : adjacency[vertex]) {
        if (component_of[next] == none) {
          component_of[next] = component;
          stack.push_back(next);
        }
      }
    }
    ++component;
  }
  return component_of;
}

std::vector<UndirectedEdgeWitness> oracle_bridge_witnesses(
    const std::vector<OracleEdge>& edges,
    const std::vector<std::size_t>& bridge_indices) {
  std::vector<UndirectedEdgeWitness> result;
  for (const std::size_t index : bridge_indices) {
    const OracleEdge& edge = edges[index];
    result.push_back(UndirectedEdgeWitness{
        std::min(edge.first, edge.second), std::max(edge.first, edge.second),
        edge.weight});
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return std::tie(left.first, left.second, left.weight) <
           std::tie(right.first, right.second, right.weight);
  });
  return result;
}

std::vector<UndirectedEdgeWitness> sorted_bridge_witnesses(
    std::vector<UndirectedEdgeWitness> bridges) {
  std::sort(bridges.begin(), bridges.end(), [](const auto& left, const auto& right) {
    return std::tie(left.first, left.second, left.weight) <
           std::tie(right.first, right.second, right.weight);
  });
  return bridges;
}

Graph make_oracle_graph(std::size_t vertex_count,
                        const std::vector<OracleEdge>& edges) {
  Graph graph(vertex_count, false);
  for (const OracleEdge& edge : edges) {
    graph.add_edge(edge.first, edge.second, edge.weight);
  }
  return graph;
}
}  // namespace

TEST_CASE(low_link_rejects_directed_graphs) {
  Graph graph(2, true);
  graph.add_edge(0, 1);
  REQUIRE_THROWS_AS(analyze_undirected_low_link(graph), std::invalid_argument);
}

TEST_CASE(low_link_handles_empty_singleton_and_self_loop) {
  const auto empty = analyze_undirected_low_link(Graph(0, false));
  REQUIRE(empty.bridges.empty());
  REQUIRE(empty.articulation_vertices.empty());
  REQUIRE(empty.bridge_component_of.empty());
  REQUIRE_EQ(empty.bridge_component_count, std::size_t{0});

  Graph singleton(1, false);
  singleton.add_edge(0, 0, -7);
  const auto one = analyze_undirected_low_link(singleton);
  REQUIRE(one.bridges.empty());
  REQUIRE(one.articulation_vertices.empty());
  REQUIRE_EQ(one.bridge_component_of, (std::vector<std::size_t>{0}));
  REQUIRE_EQ(one.bridge_component_count, std::size_t{1});
}

TEST_CASE(low_link_handles_parallel_edges_and_disconnected_blocks) {
  const std::vector<OracleEdge> edges{
      {0, 1, 5}, {0, 1, 9}, {1, 2, 17}, {1, 1, -4},
      {3, 4, 1}, {4, 5, 1}, {5, 3, 1}};
  const auto result = analyze_undirected_low_link(make_oracle_graph(7, edges));
  REQUIRE_EQ(sorted_bridge_witnesses(result.bridges),
             oracle_bridge_witnesses(edges, {2}));
  REQUIRE_EQ(result.articulation_vertices, (std::vector<Vertex>{1}));
  REQUIRE_EQ(result.bridge_component_of,
             (std::vector<std::size_t>{0, 0, 1, 2, 2, 2, 3}));
  REQUIRE_EQ(result.bridge_component_count, std::size_t{4});
}

TEST_CASE(low_link_path_marks_every_edge_and_internal_vertex) {
  const std::vector<OracleEdge> edges{{0, 1, 11}, {1, 2, 12}, {2, 3, 13}};
  const auto result = analyze_undirected_low_link(make_oracle_graph(4, edges));
  REQUIRE_EQ(sorted_bridge_witnesses(result.bridges),
             oracle_bridge_witnesses(edges, {0, 1, 2}));
  REQUIRE_EQ(result.articulation_vertices, (std::vector<Vertex>{1, 2}));
  REQUIRE_EQ(result.bridge_component_of,
             (std::vector<std::size_t>{0, 1, 2, 3}));
}

TEST_CASE(low_link_randomized_matches_independent_removal_oracles) {
  std::mt19937_64 random(0x10A11A5ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 9);
  std::uniform_int_distribution<int> edge_count_distribution(0, 18);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(
        vertex_count_distribution(random));
    std::vector<OracleEdge> edges;
    if (vertex_count != 0) {
      std::uniform_int_distribution<std::size_t> vertex_distribution(
          0, vertex_count - 1);
      const int edge_count = edge_count_distribution(random);
      edges.reserve(static_cast<std::size_t>(edge_count));
      for (int index = 0; index < edge_count; ++index) {
        edges.push_back(OracleEdge{
            vertex_distribution(random), vertex_distribution(random),
            static_cast<Weight>(weight_distribution(random))});
      }
    }

    const auto result =
        analyze_undirected_low_link(make_oracle_graph(vertex_count, edges));
    const auto bridge_indices = oracle_bridge_indices(vertex_count, edges);
    REQUIRE_EQ(sorted_bridge_witnesses(result.bridges),
               oracle_bridge_witnesses(edges, bridge_indices));
    REQUIRE_EQ(result.articulation_vertices,
               oracle_articulation_vertices(vertex_count, edges));
    const auto expected_components =
        oracle_bridge_components(vertex_count, edges, bridge_indices);
    REQUIRE_EQ(result.bridge_component_of, expected_components);

    std::size_t expected_component_count = 0;
    for (const std::size_t component : expected_components) {
      expected_component_count =
          std::max(expected_component_count, component + 1);
    }
    REQUIRE_EQ(result.bridge_component_count, expected_component_count);
  }
}

TEST_CASE(topological_sort_orders_dag_and_rejects_cycle) {
  Graph dag(6, true);
  dag.add_edge(5, 2);
  dag.add_edge(5, 0);
  dag.add_edge(4, 0);
  dag.add_edge(4, 1);
  dag.add_edge(2, 3);
  dag.add_edge(3, 1);

  const auto order = topological_sort(dag);
  REQUIRE(order.has_value());
  std::vector<std::size_t> position(6);
  for (std::size_t index = 0; index < order->size(); ++index) {
    position[(*order)[index]] = index;
  }
  for (Vertex from = 0; from < dag.vertex_count(); ++from) {
    for (const Edge& edge : dag.neighbors(from)) {
      REQUIRE(position[from] < position[edge.to]);
    }
  }
  REQUIRE(!has_cycle(dag));

  Graph cyclic(3, true);
  cyclic.add_edge(0, 1);
  cyclic.add_edge(1, 2);
  cyclic.add_edge(2, 0);
  REQUIRE(has_cycle(cyclic));
  REQUIRE(!topological_sort(cyclic).has_value());

  Graph undirected(2, false);
  REQUIRE_THROWS_AS(topological_sort(undirected), std::invalid_argument);
  REQUIRE_THROWS_AS(connected_components(dag), std::invalid_argument);
}
