#include "algorithms/graphs/transitive_reduction.hpp"

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

std::vector<std::vector<Vertex>> canonical_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<Vertex>> adjacency(n);
  for (Vertex from = 0; from < n; ++from) {
    auto& targets = adjacency[from];
    targets.reserve(graph.neighbors(from).size());
    for (const Edge& edge : graph.neighbors(from)) {
      targets.push_back(edge.to);
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
  }
  return adjacency;
}

std::vector<Vertex> deterministic_topological_order(
    const std::vector<std::vector<Vertex>>& adjacency) {
  const std::size_t n = adjacency.size();
  std::vector<std::size_t> indegree(n, 0);
  for (const auto& targets : adjacency) {
    for (Vertex to : targets) {
      ++indegree[to];
    }
  }

  std::deque<Vertex> ready;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (indegree[vertex] == 0) {
      ready.push_back(vertex);
    }
  }

  std::vector<Vertex> order;
  order.reserve(n);
  while (!ready.empty()) {
    const Vertex from = ready.front();
    ready.pop_front();
    order.push_back(from);
    for (Vertex to : adjacency[from]) {
      --indegree[to];
      if (indegree[to] == 0) {
        ready.push_back(to);
      }
    }
  }

  if (order.size() != n) {
    throw std::invalid_argument("transitive reduction requires a DAG");
  }
  return order;
}

std::vector<std::vector<unsigned char>> reachability_matrix(
    const std::vector<std::vector<Vertex>>& adjacency) {
  const std::size_t n = adjacency.size();
  std::vector<std::vector<unsigned char>> reachable(
      n, std::vector<unsigned char>(n, 0));

  for (Vertex source = 0; source < n; ++source) {
    std::deque<Vertex> queue;
    for (Vertex to : adjacency[source]) {
      if (reachable[source][to] == 0U) {
        reachable[source][to] = 1U;
        queue.push_back(to);
      }
    }
    while (!queue.empty()) {
      const Vertex from = queue.front();
      queue.pop_front();
      for (Vertex to : adjacency[from]) {
        if (reachable[source][to] == 0U) {
          reachable[source][to] = 1U;
          queue.push_back(to);
        }
      }
    }
  }
  return reachable;
}

}  // namespace

TransitiveReductionResult transitive_reduction(const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("transitive reduction requires directed input");
  }

  const auto adjacency = canonical_adjacency(graph);
  TransitiveReductionResult result;
  result.topological_order = deterministic_topological_order(adjacency);

  const auto reachable = reachability_matrix(adjacency);
  for (Vertex from = 0; from < adjacency.size(); ++from) {
    for (Vertex to : adjacency[from]) {
      bool redundant = false;
      for (Vertex first_hop : adjacency[from]) {
        if (first_hop != to && reachable[first_hop][to] != 0U) {
          redundant = true;
          break;
        }
      }
      if (!redundant) {
        result.arcs.push_back(DirectedArc{from, to});
      }
    }
  }
  return result;
}

}  // namespace algorithms::graphs
