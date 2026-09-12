#include "algorithms/graphs/biconnected_components.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace algorithms::graphs {
namespace {
struct StructuralEdge { Vertex first; Vertex second; };
struct AdjacentEdge { Vertex to; std::size_t edge_id; };
struct Frame { Vertex vertex; std::size_t next_index; };

std::vector<StructuralEdge> canonical_edges(const Graph& graph) {
  std::vector<StructuralEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (from < edge.to) edges.push_back({from, edge.to});
    }
  }
  std::sort(edges.begin(), edges.end(), [](const StructuralEdge& a, const StructuralEdge& b) {
    return std::pair{a.first, a.second} < std::pair{b.first, b.second};
  });
  edges.erase(std::unique(edges.begin(), edges.end(), [](const StructuralEdge& a, const StructuralEdge& b) {
    return a.first == b.first && a.second == b.second;
  }), edges.end());
  return edges;
}

std::vector<std::vector<AdjacentEdge>> build_adjacency(std::size_t n, const std::vector<StructuralEdge>& edges) {
  std::vector<std::vector<AdjacentEdge>> adjacency(n);
  for (std::size_t id = 0; id < edges.size(); ++id) {
    const auto& edge = edges[id];
    adjacency[edge.first].push_back({edge.second, id});
    adjacency[edge.second].push_back({edge.first, id});
  }
  for (auto& list : adjacency) {
    std::sort(list.begin(), list.end(), [](const AdjacentEdge& a, const AdjacentEdge& b) {
      if (a.to != b.to) return a.to < b.to;
      return a.edge_id < b.edge_id;
    });
  }
  return adjacency;
}

void append_block_until(std::vector<std::size_t>& edge_stack, std::size_t stop_edge,
                        const std::vector<StructuralEdge>& edges,
                        std::vector<std::vector<Vertex>>& blocks) {
  std::vector<Vertex> vertices;
  while (true) {
    if (edge_stack.empty()) throw std::logic_error("biconnected edge stack invariant violated");
    const std::size_t id = edge_stack.back();
    edge_stack.pop_back();
    vertices.push_back(edges[id].first);
    vertices.push_back(edges[id].second);
    if (id == stop_edge) break;
  }
  std::sort(vertices.begin(), vertices.end());
  vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
  blocks.push_back(std::move(vertices));
}
}

VertexBiconnectedDecomposition vertex_biconnected_decomposition(const Graph& graph) {
  if (graph.directed()) throw std::invalid_argument("vertex-biconnected decomposition requires undirected input");
  const std::size_t n = graph.vertex_count();
  const auto edges = canonical_edges(graph);
  const auto adjacency = build_adjacency(n, edges);
  const std::size_t none = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> discovery(n, none), low(n, 0), parent_edge(n, none);
  std::vector<std::size_t> edge_stack;
  std::vector<std::vector<Vertex>> blocks;
  std::size_t time = 0;

  for (Vertex root = 0; root < n; ++root) {
    if (discovery[root] != none) continue;
    if (adjacency[root].empty()) {
      discovery[root] = time++;
      low[root] = discovery[root];
      blocks.push_back({root});
      continue;
    }
    discovery[root] = time++;
    low[root] = discovery[root];
    std::vector<Frame> stack{{root, 0}};
    while (!stack.empty()) {
      Frame& frame = stack.back();
      const Vertex u = frame.vertex;
      if (frame.next_index < adjacency[u].size()) {
        const AdjacentEdge entry = adjacency[u][frame.next_index++];
        if (entry.edge_id == parent_edge[u]) continue;
        const Vertex v = entry.to;
        if (discovery[v] == none) {
          edge_stack.push_back(entry.edge_id);
          parent_edge[v] = entry.edge_id;
          discovery[v] = time++;
          low[v] = discovery[v];
          stack.push_back({v, 0});
          continue;
        }
        if (discovery[v] < discovery[u]) {
          edge_stack.push_back(entry.edge_id);
          low[u] = std::min(low[u], discovery[v]);
        }
        continue;
      }
      stack.pop_back();
      if (parent_edge[u] != none) {
        const std::size_t tree_edge = parent_edge[u];
        const StructuralEdge& edge = edges[tree_edge];
        const Vertex parent = (edge.first == u) ? edge.second : edge.first;
        low[parent] = std::min(low[parent], low[u]);
        if (low[u] >= discovery[parent]) {
          append_block_until(edge_stack, tree_edge, edges, blocks);
        }
      }
    }
    if (!edge_stack.empty()) throw std::logic_error("biconnected DFS left residual edges");
  }

  std::sort(blocks.begin(), blocks.end());
  blocks.erase(std::unique(blocks.begin(), blocks.end()), blocks.end());
  std::vector<std::vector<std::size_t>> memberships(n);
  for (std::size_t block = 0; block < blocks.size(); ++block) {
    for (Vertex vertex : blocks[block]) memberships[vertex].push_back(block);
  }
  VertexBiconnectedDecomposition result;
  result.blocks = std::move(blocks);
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (memberships[vertex].size() > 1) {
      result.articulation_vertices.push_back(vertex);
      for (std::size_t block : memberships[vertex]) result.block_cut_incidence.push_back({vertex, block});
    }
  }
  return result;
}
}
