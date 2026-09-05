#include <iostream>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/shortest_paths.hpp"
#include "algorithms/graphs/traversal.hpp"

int main() {
  using namespace algorithms::graphs;
  Graph graph(5, true);
  graph.add_edge(0, 1, 4);
  graph.add_edge(0, 2, 1);
  graph.add_edge(2, 1, 2);
  graph.add_edge(1, 3, 1);
  graph.add_edge(2, 3, 5);

  const auto bfs = breadth_first_search(graph, 0);
  std::cout << "BFS order:";
  for (Vertex vertex : bfs.order) {
    std::cout << ' ' << vertex;
  }
  std::cout << '\n';

  const auto shortest = dijkstra(graph, 0);
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    std::cout << "distance(0," << vertex << ")=";
    if (shortest.distance[vertex].has_value()) {
      std::cout << *shortest.distance[vertex];
    } else {
      std::cout << "unreachable";
    }
    std::cout << '\n';
  }
}
