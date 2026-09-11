#include "algorithms/graphs/chordal_graph.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

using AdjacencyMatrix = std::vector<std::vector<std::uint8_t>>;

[[nodiscard]] AdjacencyMatrix simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  AdjacencyMatrix adjacent(n, std::vector<std::uint8_t>(n, std::uint8_t{0}));
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from) continue;
      adjacent[from][edge.to] = 1U;
      adjacent[edge.to][from] = 1U;
    }
  }
  return adjacent;
}

[[nodiscard]] std::vector<Vertex> maximum_cardinality_search(
    const AdjacencyMatrix& adjacent) {
  const std::size_t n = adjacent.size();
  std::vector<std::size_t> score(n, 0);
  std::vector<std::uint8_t> selected(n, std::uint8_t{0});
  std::vector<Vertex> order;
  order.reserve(n);

  for (std::size_t step = 0; step < n; ++step) {
    Vertex best = n;
    std::size_t best_score = 0;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (selected[vertex] != 0U) continue;
      if (best == n || score[vertex] > best_score ||
          (score[vertex] == best_score && vertex < best)) {
        best = vertex;
        best_score = score[vertex];
      }
    }

    selected[best] = 1U;
    order.push_back(best);
    for (Vertex neighbor = 0; neighbor < n; ++neighbor) {
      if (selected[neighbor] == 0U && adjacent[best][neighbor] != 0U) {
        ++score[neighbor];
      }
    }
  }
  return order;
}

}  // namespace

ChordalRecognitionResult recognize_chordal_graph(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("chordal recognition requires an undirected graph");
  }

  const AdjacencyMatrix adjacent = simple_adjacency(graph);
  const std::size_t n = adjacent.size();

  ChordalRecognitionResult result;
  result.mcs_selection_order = maximum_cardinality_search(adjacent);
  result.candidate_elimination_order.assign(result.mcs_selection_order.rbegin(),
                                            result.mcs_selection_order.rend());

  std::vector<std::size_t> position(n, 0);
  for (std::size_t index = 0; index < n; ++index) {
    position[result.candidate_elimination_order[index]] = index;
  }

  std::size_t maximum_clique_size = n == 0 ? 0 : 1;
  for (std::size_t index = 0; index < n; ++index) {
    const Vertex vertex = result.candidate_elimination_order[index];
    std::vector<Vertex> later_neighbors;
    for (Vertex neighbor = 0; neighbor < n; ++neighbor) {
      if (adjacent[vertex][neighbor] != 0U && position[neighbor] > index) {
        later_neighbors.push_back(neighbor);
      }
    }

    for (std::size_t first = 0; first < later_neighbors.size(); ++first) {
      for (std::size_t second = first + 1; second < later_neighbors.size(); ++second) {
        if (adjacent[later_neighbors[first]][later_neighbors[second]] == 0U) {
          result.chordal = false;
          result.violation = PerfectEliminationViolation{
              vertex, later_neighbors[first], later_neighbors[second]};
          result.maximum_clique_size.reset();
          return result;
        }
      }
    }
    maximum_clique_size =
        std::max(maximum_clique_size, later_neighbors.size() + std::size_t{1});
  }

  result.chordal = true;
  result.violation.reset();
  result.maximum_clique_size = maximum_clique_size;
  return result;
}

}  // namespace algorithms::graphs
