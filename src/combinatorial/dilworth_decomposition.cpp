#include "algorithms/combinatorial/dilworth_decomposition.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/bipartite_matching.hpp"
#include "algorithms/graphs/traversal.hpp"

namespace algorithms::combinatorial {

DilworthDecompositionResult dilworth_decomposition(const graphs::Graph& dag) {
  if (!dag.directed()) {
    throw std::invalid_argument(
        "dilworth_decomposition requires a directed graph");
  }
  if (!graphs::topological_sort(dag).has_value()) {
    throw std::invalid_argument(
        "dilworth_decomposition requires an acyclic graph");
  }

  const std::size_t vertex_count = dag.vertex_count();
  std::vector<std::vector<bool>> reachable(
      vertex_count, std::vector<bool>(vertex_count, false));
  std::vector<graphs::Vertex> stack;
  stack.reserve(vertex_count);

  for (graphs::Vertex source = 0; source < vertex_count; ++source) {
    stack.clear();
    for (const graphs::Edge& edge : dag.neighbors(source)) {
      if (!reachable[source][edge.to]) {
        reachable[source][edge.to] = true;
        stack.push_back(edge.to);
      }
    }
    while (!stack.empty()) {
      const graphs::Vertex current = stack.back();
      stack.pop_back();
      for (const graphs::Edge& edge : dag.neighbors(current)) {
        if (!reachable[source][edge.to]) {
          reachable[source][edge.to] = true;
          stack.push_back(edge.to);
        }
      }
    }
  }

  std::vector<graphs::BipartiteEdge> comparability_edges;
  for (std::size_t left = 0; left < vertex_count; ++left) {
    for (std::size_t right = 0; right < vertex_count; ++right) {
      if (reachable[left][right]) {
        comparability_edges.push_back(graphs::BipartiteEdge{left, right});
      }
    }
  }

  const graphs::BipartiteMatchingResult matching = graphs::hopcroft_karp(
      vertex_count, vertex_count, comparability_edges);

  DilworthDecompositionResult result;
  result.matching_cardinality = matching.cardinality;
  result.width = vertex_count - matching.cardinality;

  std::vector<bool> seen(vertex_count, false);
  for (std::size_t start = 0; start < vertex_count; ++start) {
    if (matching.right_match[start].has_value()) {
      continue;
    }

    std::vector<std::size_t> chain;
    std::size_t current = start;
    while (true) {
      if (seen[current]) {
        throw std::logic_error(
            "matching reconstruction formed a chain cycle");
      }
      seen[current] = true;
      chain.push_back(current);
      if (!matching.left_match[current].has_value()) {
        break;
      }
      current = *matching.left_match[current];
    }
    result.minimum_chain_decomposition.push_back(std::move(chain));
  }

  for (const bool vertex_seen : seen) {
    if (!vertex_seen) {
      throw std::logic_error(
          "matching reconstruction omitted a poset element");
    }
  }
  if (result.minimum_chain_decomposition.size() != result.width) {
    throw std::logic_error(
        "minimum chain decomposition disagrees with matching dual");
  }

  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    if (!matching.left_in_min_vertex_cover[vertex] &&
        !matching.right_in_min_vertex_cover[vertex]) {
      result.maximum_antichain.push_back(vertex);
    }
  }
  if (result.maximum_antichain.size() != result.width) {
    throw std::logic_error(
        "maximum antichain reconstruction disagrees with Dilworth width");
  }

  return result;
}

}  // namespace algorithms::combinatorial
