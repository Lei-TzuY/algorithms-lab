#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

enum class CactusBlockKind {
  kBridge,
  kCycle,
  kComplex,
};

struct CactusBlock {
  CactusBlockKind kind{};
  std::vector<std::pair<Vertex, Vertex>> edges;
  // Populated only for cycle blocks. The first vertex is the smallest vertex in
  // the cycle and the chosen direction starts with its smaller cycle neighbor.
  std::vector<Vertex> cycle_vertices;

  friend bool operator==(const CactusBlock&, const CactusBlock&) = default;
};

struct CactusAnalysis {
  bool is_cactus{};
  std::size_t connected_components{};
  std::vector<CactusBlock> blocks;

  friend bool operator==(const CactusAnalysis&, const CactusAnalysis&) = default;
};

// Analyze an undirected simple graph as a cactus forest. Disconnected inputs are
// supported. A graph is a cactus forest iff every vertex-biconnected edge block
// is either a single bridge edge or a simple cycle. Edge weights are ignored.
// Directed graphs, self-loops, and parallel edges are rejected because this API
// intentionally uses the standard simple-graph cactus definition.
[[nodiscard]] inline CactusAnalysis analyze_cactus_forest(const Graph& graph);

}  // namespace algorithms::graphs

namespace algorithms::graphs {
namespace {

using UndirectedEdge = std::pair<Vertex, Vertex>;

struct IndexedNeighbor {
  Vertex to{};
  std::size_t edge_id{};
};

struct DfsFrame {
  Vertex vertex{};
  std::size_t next_neighbor{};
};

constexpr std::size_t kNoEdge = std::numeric_limits<std::size_t>::max();
constexpr Vertex kNoVertex = std::numeric_limits<Vertex>::max();

inline std::vector<UndirectedEdge> collect_simple_edges(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("cactus analysis requires an undirected graph");
  }

  std::set<UndirectedEdge> unique_edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      const Vertex to = edge.to;
      if (from == to) {
        throw std::invalid_argument(
            "cactus analysis requires a simple graph without self-loops");
      }
      if (from < to) {
        const UndirectedEdge normalized{from, to};
        if (!unique_edges.insert(normalized).second) {
          throw std::invalid_argument(
              "cactus analysis requires a simple graph without parallel edges");
        }
      }
    }
  }
  return {unique_edges.begin(), unique_edges.end()};
}

inline CactusBlock classify_block(const std::vector<std::size_t>& block_edge_ids,
                           const std::vector<UndirectedEdge>& all_edges) {
  CactusBlock block;
  block.edges.reserve(block_edge_ids.size());
  for (const std::size_t edge_id : block_edge_ids) {
    block.edges.push_back(all_edges[edge_id]);
  }
  std::sort(block.edges.begin(), block.edges.end());

  if (block.edges.size() == 1) {
    block.kind = CactusBlockKind::kBridge;
    return block;
  }

  std::map<Vertex, std::vector<Vertex>> local_adjacency;
  for (const auto& [first, second] : block.edges) {
    local_adjacency[first].push_back(second);
    local_adjacency[second].push_back(first);
  }

  bool simple_cycle = block.edges.size() == local_adjacency.size() &&
                      local_adjacency.size() >= 3;
  if (simple_cycle) {
    for (auto& [vertex, neighbors] : local_adjacency) {
      static_cast<void>(vertex);
      std::sort(neighbors.begin(), neighbors.end());
      if (neighbors.size() != 2) {
        simple_cycle = false;
        break;
      }
    }
  }

  if (!simple_cycle) {
    block.kind = CactusBlockKind::kComplex;
    return block;
  }

  block.kind = CactusBlockKind::kCycle;
  const Vertex start = local_adjacency.begin()->first;
  Vertex previous = start;
  Vertex current = local_adjacency.at(start).front();
  block.cycle_vertices.push_back(start);

  while (current != start) {
    if (block.cycle_vertices.size() >= local_adjacency.size()) {
      block.kind = CactusBlockKind::kComplex;
      block.cycle_vertices.clear();
      return block;
    }
    block.cycle_vertices.push_back(current);
    const auto& neighbors = local_adjacency.at(current);
    const Vertex next = neighbors[0] == previous ? neighbors[1] : neighbors[0];
    previous = current;
    current = next;
  }

  if (block.cycle_vertices.size() != local_adjacency.size()) {
    block.kind = CactusBlockKind::kComplex;
    block.cycle_vertices.clear();
  }
  return block;
}

}  // namespace

inline CactusAnalysis analyze_cactus_forest(const Graph& graph) {
  const std::vector<UndirectedEdge> edges = collect_simple_edges(graph);
  const std::size_t vertex_count = graph.vertex_count();

  std::vector<std::vector<IndexedNeighbor>> adjacency(vertex_count);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    const auto [first, second] = edges[edge_id];
    adjacency[first].push_back(IndexedNeighbor{second, edge_id});
    adjacency[second].push_back(IndexedNeighbor{first, edge_id});
  }
  for (auto& neighbors : adjacency) {
    std::sort(neighbors.begin(), neighbors.end(),
              [](const IndexedNeighbor& left, const IndexedNeighbor& right) {
                if (left.to != right.to) {
                  return left.to < right.to;
                }
                return left.edge_id < right.edge_id;
              });
  }

  std::vector<std::size_t> discovery(vertex_count, 0);
  std::vector<std::size_t> low(vertex_count, 0);
  std::vector<Vertex> parent(vertex_count, kNoVertex);
  std::vector<std::size_t> parent_edge(vertex_count, kNoEdge);
  std::vector<std::size_t> edge_stack;
  edge_stack.reserve(edges.size());
  std::vector<DfsFrame> dfs_stack;
  dfs_stack.reserve(vertex_count);

  CactusAnalysis analysis{true, 0, {}};
  std::size_t timer = 0;

  for (Vertex root = 0; root < vertex_count; ++root) {
    if (discovery[root] != 0) {
      continue;
    }
    ++analysis.connected_components;
    discovery[root] = ++timer;
    low[root] = discovery[root];
    dfs_stack.push_back(DfsFrame{root, 0});

    while (!dfs_stack.empty()) {
      DfsFrame& frame = dfs_stack.back();
      const Vertex vertex = frame.vertex;

      if (frame.next_neighbor < adjacency[vertex].size()) {
        const IndexedNeighbor next = adjacency[vertex][frame.next_neighbor++];
        if (next.edge_id == parent_edge[vertex]) {
          continue;
        }

        if (discovery[next.to] == 0) {
          edge_stack.push_back(next.edge_id);
          parent[next.to] = vertex;
          parent_edge[next.to] = next.edge_id;
          discovery[next.to] = ++timer;
          low[next.to] = discovery[next.to];
          dfs_stack.push_back(DfsFrame{next.to, 0});
          continue;
        }

        if (discovery[next.to] < discovery[vertex]) {
          edge_stack.push_back(next.edge_id);
          low[vertex] = std::min(low[vertex], discovery[next.to]);
        }
        continue;
      }

      dfs_stack.pop_back();
      if (parent_edge[vertex] == kNoEdge) {
        continue;
      }

      const Vertex parent_vertex = parent[vertex];
      low[parent_vertex] = std::min(low[parent_vertex], low[vertex]);
      if (low[vertex] < discovery[parent_vertex]) {
        continue;
      }

      std::vector<std::size_t> block_edge_ids;
      while (!edge_stack.empty()) {
        const std::size_t edge_id = edge_stack.back();
        edge_stack.pop_back();
        block_edge_ids.push_back(edge_id);
        if (edge_id == parent_edge[vertex]) {
          break;
        }
      }
      if (block_edge_ids.empty() ||
          block_edge_ids.back() != parent_edge[vertex]) {
        throw std::logic_error("cactus Tarjan edge-stack invariant violated");
      }

      CactusBlock block = classify_block(block_edge_ids, edges);
      if (block.kind == CactusBlockKind::kComplex) {
        analysis.is_cactus = false;
      }
      analysis.blocks.push_back(std::move(block));
    }

    if (!edge_stack.empty()) {
      throw std::logic_error("cactus Tarjan component left residual edges");
    }
  }

  std::sort(analysis.blocks.begin(), analysis.blocks.end(),
            [](const CactusBlock& left, const CactusBlock& right) {
              if (left.edges != right.edges) {
                return left.edges < right.edges;
              }
              return static_cast<int>(left.kind) < static_cast<int>(right.kind);
            });
  return analysis;
}

}  // namespace algorithms::graphs

