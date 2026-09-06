#include "algorithms/graphs/minimum_spanning_tree.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/data_structures/disjoint_set_union.hpp"

namespace algorithms::graphs {
namespace {

void require_undirected(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "minimum spanning forest requires an undirected graph");
  }
}

Weight checked_add(Weight left, Weight right) {
  if ((right > 0 && left > std::numeric_limits<Weight>::max() - right) ||
      (right < 0 && left < std::numeric_limits<Weight>::min() - right)) {
    throw std::overflow_error("minimum spanning forest total weight overflow");
  }
  return left + right;
}

std::vector<WeightedEdge> logical_undirected_edges(const Graph& graph) {
  std::vector<WeightedEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.push_back(WeightedEdge{from, edge.to, edge.weight});
      }
    }
  }
  return edges;
}

struct PrimCandidate {
  Vertex from;
  Vertex to;
  Weight weight;
};

struct PrimCandidateLess {
  bool operator()(const PrimCandidate& left,
                  const PrimCandidate& right) const noexcept {
    if (left.weight != right.weight) {
      return left.weight < right.weight;
    }
    if (left.from != right.from) {
      return left.from < right.from;
    }
    return left.to < right.to;
  }
};

}  // namespace

MinimumSpanningForest kruskal_minimum_spanning_forest(const Graph& graph) {
  require_undirected(graph);

  std::vector<WeightedEdge> edges = logical_undirected_edges(graph);
  std::sort(edges.begin(), edges.end(),
            [](const WeightedEdge& left, const WeightedEdge& right) {
              if (left.weight != right.weight) {
                return left.weight < right.weight;
              }
              if (left.from != right.from) {
                return left.from < right.from;
              }
              return left.to < right.to;
            });

  data_structures::DisjointSetUnion dsu(graph.vertex_count());
  MinimumSpanningForest result;
  result.edges.reserve(graph.vertex_count());

  for (const WeightedEdge& edge : edges) {
    if (!dsu.unite(edge.from, edge.to)) {
      continue;
    }
    result.edges.push_back(edge);
    result.total_weight = checked_add(result.total_weight, edge.weight);
  }

  result.component_count = dsu.components();
  return result;
}

MinimumSpanningForest prim_minimum_spanning_forest(const Graph& graph) {
  require_undirected(graph);

  MinimumSpanningForest result;
  std::vector<bool> visited(graph.vertex_count(), false);
  data_structures::BinaryHeap<PrimCandidate, PrimCandidateLess> frontier;

  const auto push_frontier = [&graph, &visited, &frontier](Vertex from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (!visited[edge.to]) {
        frontier.push(PrimCandidate{from, edge.to, edge.weight});
      }
    }
  };

  for (Vertex root = 0; root < graph.vertex_count(); ++root) {
    if (visited[root]) {
      continue;
    }

    ++result.component_count;
    visited[root] = true;
    push_frontier(root);

    while (!frontier.empty()) {
      const PrimCandidate candidate = frontier.pop();
      if (visited[candidate.to]) {
        continue;
      }

      visited[candidate.to] = true;
      result.edges.push_back(
          WeightedEdge{candidate.from, candidate.to, candidate.weight});
      result.total_weight =
          checked_add(result.total_weight, candidate.weight);
      push_frontier(candidate.to);
    }
  }

  return result;
}

}  // namespace algorithms::graphs
