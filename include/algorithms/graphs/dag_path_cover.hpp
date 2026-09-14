#pragma once

#include "algorithms/graphs/bipartite_matching.hpp"
#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/traversal.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct DagPathCoverEdgeWitness {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;

  friend bool operator==(const DagPathCoverEdgeWitness&,
                         const DagPathCoverEdgeWitness&) = default;
};

struct DagPathCoverPath {
  std::vector<Vertex> vertices;
  std::vector<DagPathCoverEdgeWitness> edges;

  friend bool operator==(const DagPathCoverPath&,
                         const DagPathCoverPath&) = default;
};

struct DagPathCoverResult {
  std::size_t path_count;
  std::size_t matching_cardinality;
  std::vector<DagPathCoverPath> paths;

  friend bool operator==(const DagPathCoverResult&,
                         const DagPathCoverResult&) = default;
};

// Exact minimum vertex-disjoint path cover using only original directed arcs.
// The input must be a DAG. Edge weights are provenance only and do not affect
// the cardinality objective. Parallel u->v arcs collapse to one matching
// relation; the first insertion-order copy is retained as the replay witness.
[[nodiscard]] inline DagPathCoverResult minimum_dag_path_cover(
    const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("DAG path cover requires a directed graph");
  }
  const auto order = topological_sort(graph);
  if (!order.has_value()) {
    throw std::invalid_argument("DAG path cover requires an acyclic graph");
  }

  const std::size_t n = graph.vertex_count();
  std::vector<BipartiteEdge> relations;
  std::map<std::pair<Vertex, Vertex>, DagPathCoverEdgeWitness> witnesses;

  for (Vertex from = 0; from < n; ++from) {
    const auto& neighbors = graph.neighbors(from);
    for (std::size_t index = 0; index < neighbors.size(); ++index) {
      const auto& edge = neighbors[index];
      const auto key = std::pair<Vertex, Vertex>{from, edge.to};
      const auto [iterator, inserted] = witnesses.emplace(
          key, DagPathCoverEdgeWitness{from, index, edge.to, edge.weight});
      static_cast<void>(iterator);
      if (inserted) {
        relations.push_back(BipartiteEdge{from, edge.to});
      }
    }
  }

  const auto matching = hopcroft_karp(n, n, relations);
  if (matching.left_match.size() != n || matching.right_match.size() != n ||
      matching.cardinality > n) {
    throw std::logic_error("bipartite matching returned an invalid shape");
  }

  std::vector<std::optional<Vertex>> successor(n);
  std::vector<std::optional<Vertex>> predecessor(n);
  std::size_t matched_count = 0;
  for (Vertex from = 0; from < n; ++from) {
    if (!matching.left_match[from].has_value()) {
      continue;
    }
    const Vertex to = *matching.left_match[from];
    if (to >= n || predecessor[to].has_value() ||
        !matching.right_match[to].has_value() ||
        *matching.right_match[to] != from) {
      throw std::logic_error("bipartite matching witness was not reciprocal");
    }
    if (witnesses.find({from, to}) == witnesses.end()) {
      throw std::logic_error("bipartite matching referenced a missing DAG arc");
    }
    successor[from] = to;
    predecessor[to] = from;
    ++matched_count;
  }
  if (matched_count != matching.cardinality) {
    throw std::logic_error("bipartite matching cardinality did not match witness");
  }

  std::vector<bool> covered(n, false);
  std::vector<DagPathCoverPath> paths;
  paths.reserve(n - matching.cardinality);
  std::size_t covered_count = 0;

  for (Vertex start = 0; start < n; ++start) {
    if (predecessor[start].has_value()) {
      continue;
    }
    DagPathCoverPath path;
    Vertex current = start;
    while (true) {
      if (covered[current]) {
        throw std::logic_error("matching links formed a directed cycle");
      }
      covered[current] = true;
      ++covered_count;
      path.vertices.push_back(current);
      if (!successor[current].has_value()) {
        break;
      }
      const Vertex next = *successor[current];
      path.edges.push_back(witnesses.at({current, next}));
      current = next;
    }
    paths.push_back(std::move(path));
  }

  if (covered_count != n || paths.size() != n - matching.cardinality) {
    throw std::logic_error("matching did not reconstruct a complete path cover");
  }

  return DagPathCoverResult{paths.size(), matching.cardinality,
                            std::move(paths)};
}

}  // namespace algorithms::graphs
