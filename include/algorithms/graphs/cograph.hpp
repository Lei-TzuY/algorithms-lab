#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

enum class CotreeNodeKind : unsigned char {
  leaf,
  disjoint_union,
  complete_join,
};

struct CotreeNode {
  CotreeNodeKind kind{CotreeNodeKind::leaf};
  std::optional<Vertex> vertex;
  std::vector<std::size_t> children;

  friend bool operator==(const CotreeNode&, const CotreeNode&) = default;
};

struct CographCotree {
  std::vector<CotreeNode> nodes;
  std::optional<std::size_t> root;

  friend bool operator==(const CographCotree&, const CographCotree&) = default;
};

namespace cograph_detail {

using AdjacencyMatrix = std::vector<std::vector<std::uint8_t>>;

[[nodiscard]] inline AdjacencyMatrix simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  AdjacencyMatrix adjacency(n, std::vector<std::uint8_t>(n, 0U));
  for (Vertex from = 0U; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        continue;
      }
      adjacency[from][edge.to] = 1U;
      adjacency[edge.to][from] = 1U;
    }
  }
  return adjacency;
}

[[nodiscard]] inline std::vector<std::vector<Vertex>> induced_components(
    const std::vector<Vertex>& vertices, const AdjacencyMatrix& adjacency,
    const bool complement) {
  const std::size_t n = adjacency.size();
  std::vector<std::uint8_t> seen(n, 0U);
  std::vector<std::vector<Vertex>> components;

  for (const Vertex start : vertices) {
    if (seen[start] != 0U) {
      continue;
    }
    std::vector<Vertex> component;
    std::vector<Vertex> stack{start};
    seen[start] = 1U;
    while (!stack.empty()) {
      const Vertex current = stack.back();
      stack.pop_back();
      component.push_back(current);
      for (const Vertex candidate : vertices) {
        if (candidate == current || seen[candidate] != 0U) {
          continue;
        }
        const bool adjacent = adjacency[current][candidate] != 0U;
        const bool connected = complement ? !adjacent : adjacent;
        if (connected) {
          seen[candidate] = 1U;
          stack.push_back(candidate);
        }
      }
    }
    std::sort(component.begin(), component.end());
    components.push_back(std::move(component));
  }

  std::sort(components.begin(), components.end(),
            [](const auto& left, const auto& right) {
              return left.front() < right.front();
            });
  return components;
}

[[nodiscard]] inline std::optional<std::size_t> build_cotree(
    const std::vector<Vertex>& vertices, const AdjacencyMatrix& adjacency,
    std::vector<CotreeNode>& nodes) {
  if (vertices.size() == 1U) {
    const std::size_t id = nodes.size();
    nodes.push_back(
        CotreeNode{CotreeNodeKind::leaf, vertices.front(), {}});
    return id;
  }

  auto parts = induced_components(vertices, adjacency, false);
  CotreeNodeKind kind = CotreeNodeKind::disjoint_union;
  if (parts.size() == 1U) {
    parts = induced_components(vertices, adjacency, true);
    kind = CotreeNodeKind::complete_join;
  }
  if (parts.size() == 1U) {
    return std::nullopt;
  }

  std::vector<std::size_t> children;
  children.reserve(parts.size());
  for (const auto& part : parts) {
    const auto child = build_cotree(part, adjacency, nodes);
    if (!child.has_value()) {
      return std::nullopt;
    }
    children.push_back(*child);
  }

  const std::size_t id = nodes.size();
  nodes.push_back(CotreeNode{kind, std::nullopt, std::move(children)});
  return id;
}

struct ValidationState {
  const AdjacencyMatrix& adjacency;
  const CographCotree& cotree;
  std::vector<std::uint8_t> node_state;
  std::vector<std::uint8_t> seen_vertex;
};

[[nodiscard]] inline std::optional<std::vector<Vertex>> validate_node(
    const std::size_t id, ValidationState& state) {
  if (id >= state.cotree.nodes.size() || state.node_state[id] != 0U) {
    return std::nullopt;
  }
  state.node_state[id] = 1U;
  const CotreeNode& node = state.cotree.nodes[id];

  if (node.kind == CotreeNodeKind::leaf) {
    if (!node.vertex.has_value() || !node.children.empty() ||
        *node.vertex >= state.adjacency.size() ||
        state.seen_vertex[*node.vertex] != 0U) {
      return std::nullopt;
    }
    state.seen_vertex[*node.vertex] = 1U;
    state.node_state[id] = 2U;
    return std::vector<Vertex>{*node.vertex};
  }

  if (node.vertex.has_value() || node.children.size() < 2U) {
    return std::nullopt;
  }

  std::vector<std::vector<Vertex>> child_vertices;
  child_vertices.reserve(node.children.size());
  Vertex previous_minimum = 0U;
  bool have_previous = false;
  for (const std::size_t child_id : node.children) {
    if (child_id >= state.cotree.nodes.size()) {
      return std::nullopt;
    }
    const CotreeNode& child_node = state.cotree.nodes[child_id];
    if (child_node.kind == node.kind) {
      return std::nullopt;
    }
    auto leaves = validate_node(child_id, state);
    if (!leaves.has_value() || leaves->empty()) {
      return std::nullopt;
    }
    std::sort(leaves->begin(), leaves->end());
    if (have_previous && previous_minimum >= leaves->front()) {
      return std::nullopt;
    }
    previous_minimum = leaves->front();
    have_previous = true;
    child_vertices.push_back(std::move(*leaves));
  }

  for (std::size_t first = 0U; first < child_vertices.size(); ++first) {
    for (std::size_t second = first + 1U; second < child_vertices.size();
         ++second) {
      for (const Vertex left : child_vertices[first]) {
        for (const Vertex right : child_vertices[second]) {
          const bool edge = state.adjacency[left][right] != 0U;
          if (node.kind == CotreeNodeKind::disjoint_union) {
            if (edge) {
              return std::nullopt;
            }
          } else if (node.kind == CotreeNodeKind::complete_join) {
            if (!edge) {
              return std::nullopt;
            }
          } else {
            return std::nullopt;
          }
        }
      }
    }
  }

  std::vector<Vertex> all;
  for (const auto& group : child_vertices) {
    all.insert(all.end(), group.begin(), group.end());
  }
  std::sort(all.begin(), all.end());
  state.node_state[id] = 2U;
  return all;
}

}  // namespace cograph_detail

// Returns a deterministic reduced cotree when graph is a cograph and nullopt
// otherwise. The structural simple graph is used: self-loops are ignored,
// parallel copies collapse, and stored weights do not affect the decomposition.
[[nodiscard]] inline std::optional<CographCotree> cograph_cotree(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("cograph decomposition requires an undirected graph");
  }

  CographCotree result;
  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    return result;
  }

  const auto adjacency = cograph_detail::simple_adjacency(graph);
  std::vector<Vertex> vertices(n);
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    vertices[vertex] = vertex;
  }

  const auto root =
      cograph_detail::build_cotree(vertices, adjacency, result.nodes);
  if (!root.has_value()) {
    return std::nullopt;
  }
  result.root = *root;
  return result;
}

// Expensive replay validator for the deterministic reduced cotree witness.
[[nodiscard]] inline bool valid_cograph_cotree(const Graph& graph,
                                                const CographCotree& cotree) {
  if (graph.directed()) {
    return false;
  }
  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    return !cotree.root.has_value() && cotree.nodes.empty();
  }
  if (!cotree.root.has_value() || *cotree.root >= cotree.nodes.size()) {
    return false;
  }

  const auto adjacency = cograph_detail::simple_adjacency(graph);
  cograph_detail::ValidationState state{
      adjacency, cotree, std::vector<std::uint8_t>(cotree.nodes.size(), 0U),
      std::vector<std::uint8_t>(n, 0U)};
  auto vertices = cograph_detail::validate_node(*cotree.root, state);
  if (!vertices.has_value() || vertices->size() != n) {
    return false;
  }
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if ((*vertices)[vertex] != vertex || state.seen_vertex[vertex] == 0U) {
      return false;
    }
  }
  return std::all_of(state.node_state.begin(), state.node_state.end(),
                     [](const std::uint8_t value) { return value == 2U; });
}

}  // namespace algorithms::graphs
