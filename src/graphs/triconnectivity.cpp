#include "algorithms/graphs/triconnectivity.hpp"

#include "algorithms/graphs/biconnected_components.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {
namespace {

struct StructuralBlockGraph {
  std::vector<std::vector<std::size_t>> adjacency;
  std::vector<std::size_t> local_index;
};

StructuralBlockGraph build_structural_adjacency(
    const Graph& graph, const std::vector<Vertex>& block) {
  const std::size_t n = graph.vertex_count();
  const std::size_t none = n;
  StructuralBlockGraph result;
  result.adjacency.resize(block.size());
  result.local_index.assign(n, none);
  for (std::size_t index = 0; index < block.size(); ++index) {
    const Vertex vertex = block[index];
    if (vertex >= n) {
      throw std::logic_error("triconnectivity block contains invalid vertex");
    }
    result.local_index[vertex] = index;
  }
  for (std::size_t from_index = 0; from_index < block.size(); ++from_index) {
    const Vertex from = block[from_index];
    auto& neighbors = result.adjacency[from_index];
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from || edge.to >= n) {
        continue;
      }
      const std::size_t to_index = result.local_index[edge.to];
      if (to_index != none) {
        neighbors.push_back(to_index);
      }
    }
    std::sort(neighbors.begin(), neighbors.end());
    neighbors.erase(std::unique(neighbors.begin(), neighbors.end()),
                    neighbors.end());
  }
  return result;
}

std::vector<std::vector<Vertex>> components_after_removing(
    const std::vector<Vertex>& block, const StructuralBlockGraph& graph,
    std::size_t first, std::size_t second) {
  std::vector<std::vector<Vertex>> components;
  std::vector<unsigned char> visited(block.size(), 0);

  for (std::size_t start = 0; start < block.size(); ++start) {
    if (start == first || start == second || visited[start] != 0) {
      continue;
    }
    std::vector<Vertex> component;
    std::vector<std::size_t> stack{start};
    visited[start] = 1;
    while (!stack.empty()) {
      const std::size_t vertex = stack.back();
      stack.pop_back();
      component.push_back(block[vertex]);
      for (auto it = graph.adjacency[vertex].rbegin();
           it != graph.adjacency[vertex].rend(); ++it) {
        const std::size_t to = *it;
        if (to == first || to == second || visited[to] != 0) {
          continue;
        }
        visited[to] = 1;
        stack.push_back(to);
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }
  std::sort(components.begin(), components.end());
  return components;
}

BiconnectedBlockKind classify_block(
    std::size_t size, bool has_separation_pair) {
  if (size <= 1) {
    return BiconnectedBlockKind::singleton;
  }
  if (size == 2) {
    return BiconnectedBlockKind::bridge;
  }
  if (size == 3) {
    return BiconnectedBlockKind::triangle;
  }
  return has_separation_pair
             ? BiconnectedBlockKind::split_pair_decomposable
             : BiconnectedBlockKind::triconnected;
}

}  // namespace

TriconnectivityAnalysis analyze_triconnectivity(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "triconnectivity analysis requires undirected input");
  }

  const VertexBiconnectedDecomposition block_cut =
      vertex_biconnected_decomposition(graph);

  TriconnectivityAnalysis result;
  result.articulation_vertices = block_cut.articulation_vertices;
  result.blocks.reserve(block_cut.blocks.size());

  for (const std::vector<Vertex>& block : block_cut.blocks) {
    BiconnectedBlockTriconnectivity analysis;
    analysis.vertices = block;

    if (block.size() >= 4) {
      const StructuralBlockGraph adjacency =
          build_structural_adjacency(graph, block);
      for (std::size_t first_index = 0; first_index < block.size();
           ++first_index) {
        for (std::size_t second_index = first_index + 1;
             second_index < block.size(); ++second_index) {
          const Vertex first = block[first_index];
          const Vertex second = block[second_index];
          std::vector<std::vector<Vertex>> components =
              components_after_removing(block, adjacency, first_index,
                                        second_index);
          if (components.size() > 1) {
            analysis.separation_pairs.push_back(SeparationPairWitness{
                first, second, std::move(components)});
          }
        }
      }
    }

    analysis.kind = classify_block(block.size(),
                                   !analysis.separation_pairs.empty());
    result.blocks.push_back(std::move(analysis));
  }

  return result;
}

}  // namespace algorithms::graphs
