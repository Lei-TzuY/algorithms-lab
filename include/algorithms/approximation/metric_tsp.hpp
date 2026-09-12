#pragma once

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/minimum_spanning_tree.hpp"

namespace algorithms::approximation {

struct MetricTspApproximationResult {
  std::vector<graphs::Vertex> tour;
  graphs::Weight total_weight{0};
  std::vector<graphs::WeightedEdge> minimum_spanning_tree_edges;
  graphs::Weight minimum_spanning_tree_weight{0};
};

namespace metric_tsp_detail {

inline graphs::Weight checked_nonnegative_add(graphs::Weight left,
                                              graphs::Weight right) {
  if (right < 0) {
    throw std::logic_error("metric TSP internal negative distance");
  }
  if (left > std::numeric_limits<graphs::Weight>::max() - right) {
    throw std::overflow_error("metric TSP tour weight overflow");
  }
  return left + right;
}

inline void validate_metric_matrix(
    const std::vector<std::vector<graphs::Weight>>& distance) {
  const std::size_t n = distance.size();
  for (const auto& row : distance) {
    if (row.size() != n) {
      throw std::invalid_argument("metric TSP distance matrix must be square");
    }
  }

  for (graphs::Vertex from = 0; from < n; ++from) {
    if (distance[from][from] != 0) {
      throw std::invalid_argument("metric TSP diagonal must be zero");
    }
    for (graphs::Vertex to = 0; to < n; ++to) {
      const graphs::Weight value = distance[from][to];
      if (value < 0) {
        throw std::invalid_argument("metric TSP distances must be non-negative");
      }
      if (from != to && value == 0) {
        throw std::invalid_argument(
            "metric TSP distinct vertices must have positive distance");
      }
      if (value != distance[to][from]) {
        throw std::invalid_argument("metric TSP distances must be symmetric");
      }
    }
  }

  for (graphs::Vertex from = 0; from < n; ++from) {
    for (graphs::Vertex via = 0; via < n; ++via) {
      for (graphs::Vertex to = 0; to < n; ++to) {
        const graphs::Weight first = distance[from][via];
        const graphs::Weight second = distance[via][to];
        if (first <= std::numeric_limits<graphs::Weight>::max() - second &&
            distance[from][to] > first + second) {
          throw std::invalid_argument(
              "metric TSP triangle inequality violated");
        }
      }
    }
  }
}

}  // namespace metric_tsp_detail

// Deterministic double-tree 2-approximation for symmetric metric TSP.
//
// Preconditions are validated explicitly: the matrix is square and symmetric,
// diagonal entries are zero, distinct-vertex distances are positive, and the
// triangle inequality holds. Empty and singleton instances use canonical
// zero-cost trivial tours. For n >= 2, the returned tour is rooted at vertex 0,
// returns to 0, and visits every other vertex exactly once.
//
// Proof obligation: MST <= OPT; doubling an MST yields a 2*MST Euler walk; the
// triangle inequality makes shortcutting repeated vertices non-increasing.
// Hence the returned tour costs at most 2*OPT.
//
// This direct educational implementation validates all O(V^3) triangle
// inequalities, materializes the complete O(V^2)-edge graph, reuses the sealed
// Kruskal MST, and then shortcuts a deterministic DFS doubled-tree walk.
[[nodiscard]] inline MetricTspApproximationResult
approximate_metric_tsp_double_tree(
    const std::vector<std::vector<graphs::Weight>>& distance) {
  metric_tsp_detail::validate_metric_matrix(distance);
  const std::size_t n = distance.size();

  MetricTspApproximationResult result;
  if (n == 0) {
    return result;
  }
  if (n == 1) {
    result.tour.push_back(0);
    return result;
  }

  graphs::Graph complete(n, false);
  for (graphs::Vertex from = 0; from < n; ++from) {
    for (graphs::Vertex to = from + 1; to < n; ++to) {
      complete.add_edge(from, to, distance[from][to]);
    }
  }

  const graphs::MinimumSpanningForest forest =
      graphs::kruskal_minimum_spanning_forest(complete);
  if (forest.component_count != 1 || forest.edges.size() != n - 1) {
    throw std::logic_error(
        "complete metric graph did not produce one spanning tree");
  }
  result.minimum_spanning_tree_edges = forest.edges;
  result.minimum_spanning_tree_weight = forest.total_weight;

  std::vector<std::vector<graphs::Vertex>> tree(n);
  for (const graphs::WeightedEdge& edge : forest.edges) {
    tree[edge.from].push_back(edge.to);
    tree[edge.to].push_back(edge.from);
  }
  for (auto& neighbors : tree) {
    std::sort(neighbors.begin(), neighbors.end());
  }

  std::vector<bool> visited(n, false);
  std::vector<graphs::Vertex> stack{0};
  result.tour.reserve(n + 1);
  while (!stack.empty()) {
    const graphs::Vertex current = stack.back();
    stack.pop_back();
    if (visited[current]) {
      continue;
    }
    visited[current] = true;
    result.tour.push_back(current);
    const auto& neighbors = tree[current];
    for (auto it = neighbors.rbegin(); it != neighbors.rend(); ++it) {
      if (!visited[*it]) {
        stack.push_back(*it);
      }
    }
  }
  if (result.tour.size() != n) {
    throw std::logic_error("metric TSP spanning-tree DFS missed vertices");
  }
  result.tour.push_back(0);

  for (std::size_t index = 1; index < result.tour.size(); ++index) {
    result.total_weight = metric_tsp_detail::checked_nonnegative_add(
        result.total_weight,
        distance[result.tour[index - 1]][result.tour[index]]);
  }
  return result;
}

}  // namespace algorithms::approximation
