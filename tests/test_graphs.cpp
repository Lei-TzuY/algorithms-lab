#include "test_framework.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
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
