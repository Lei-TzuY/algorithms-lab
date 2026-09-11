#pragma once

#include "algorithms/graphs/maximum_clique.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

struct ExactColoringResult {
  std::size_t chromatic_number = 0;
  std::vector<std::size_t> colors;
  std::vector<Vertex> clique_lower_bound_witness;
  std::size_t greedy_upper_bound = 0;
  std::size_t search_nodes = 0;
};

namespace exact_coloring_detail {

using AdjacencyMatrix = std::vector<std::vector<std::uint8_t>>;
inline constexpr std::size_t kUncolored = std::numeric_limits<std::size_t>::max();

struct SimpleGraph {
  AdjacencyMatrix adjacent;
  std::vector<std::size_t> degree;
  bool has_self_loop = false;
};

[[nodiscard]] inline SimpleGraph simplify(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  SimpleGraph simple{AdjacencyMatrix(n, std::vector<std::uint8_t>(n, 0U)),
                     std::vector<std::size_t>(n, 0U), false};
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (from == edge.to) {
        simple.has_self_loop = true;
        continue;
      }
      simple.adjacent[from][edge.to] = 1U;
      simple.adjacent[edge.to][from] = 1U;
    }
  }
  for (Vertex from = 0; from < n; ++from) {
    for (Vertex to = 0; to < n; ++to) {
      if (simple.adjacent[from][to] != 0U) ++simple.degree[from];
    }
  }
  return simple;
}

[[nodiscard]] inline std::size_t saturation_degree(
    Vertex vertex, const AdjacencyMatrix& adjacent,
    const std::vector<std::size_t>& colors) {
  std::vector<std::uint8_t> seen(colors.size(), 0U);
  std::size_t saturation = 0;
  for (Vertex neighbor = 0; neighbor < colors.size(); ++neighbor) {
    const std::size_t color = colors[neighbor];
    if (adjacent[vertex][neighbor] == 0U || color == kUncolored || seen[color] != 0U) {
      continue;
    }
    seen[color] = 1U;
    ++saturation;
  }
  return saturation;
}

[[nodiscard]] inline Vertex choose_vertex(
    const AdjacencyMatrix& adjacent, const std::vector<std::size_t>& degree,
    const std::vector<std::size_t>& colors) {
  Vertex best = 0;
  bool have_best = false;
  std::size_t best_saturation = 0;
  std::size_t best_degree = 0;
  for (Vertex vertex = 0; vertex < colors.size(); ++vertex) {
    if (colors[vertex] != kUncolored) continue;
    const std::size_t saturation = saturation_degree(vertex, adjacent, colors);
    if (!have_best || saturation > best_saturation ||
        (saturation == best_saturation &&
         (degree[vertex] > best_degree ||
          (degree[vertex] == best_degree && vertex < best)))) {
      best = vertex;
      have_best = true;
      best_saturation = saturation;
      best_degree = degree[vertex];
    }
  }
  return best;
}

[[nodiscard]] inline bool color_allowed(
    Vertex vertex, std::size_t color, const AdjacencyMatrix& adjacent,
    const std::vector<std::size_t>& colors) {
  for (Vertex neighbor = 0; neighbor < colors.size(); ++neighbor) {
    if (adjacent[vertex][neighbor] != 0U && colors[neighbor] == color) return false;
  }
  return true;
}

[[nodiscard]] inline std::vector<std::size_t> greedy_dsatur(
    const AdjacencyMatrix& adjacent, const std::vector<std::size_t>& degree) {
  std::vector<std::size_t> colors(adjacent.size(), kUncolored);
  std::size_t used_colors = 0;
  for (std::size_t colored = 0; colored < adjacent.size(); ++colored) {
    const Vertex vertex = choose_vertex(adjacent, degree, colors);
    std::size_t color = 0;
    while (color < used_colors && !color_allowed(vertex, color, adjacent, colors)) ++color;
    if (color == used_colors) ++used_colors;
    colors[vertex] = color;
  }
  return colors;
}

[[nodiscard]] inline std::size_t used_color_count(const std::vector<std::size_t>& colors) {
  std::size_t count = 0;
  for (const std::size_t color : colors) {
    if (color != kUncolored) count = std::max(count, color + 1U);
  }
  return count;
}

inline bool search_k_coloring(const AdjacencyMatrix& adjacent,
                              const std::vector<std::size_t>& degree,
                              std::size_t color_limit,
                              std::vector<std::size_t>& colors,
                              std::size_t colored_count,
                              std::size_t used_colors,
                              std::size_t& search_nodes) {
  ++search_nodes;
  if (colored_count == colors.size()) return true;

  const Vertex vertex = choose_vertex(adjacent, degree, colors);
  for (std::size_t color = 0; color < used_colors; ++color) {
    if (!color_allowed(vertex, color, adjacent, colors)) continue;
    colors[vertex] = color;
    if (search_k_coloring(adjacent, degree, color_limit, colors, colored_count + 1U,
                          used_colors, search_nodes)) {
      return true;
    }
    colors[vertex] = kUncolored;
  }

  // Color names are interchangeable. Introducing only the next unused label
  // enumerates one representative of every color-label equivalence class.
  if (used_colors < color_limit) {
    colors[vertex] = used_colors;
    if (search_k_coloring(adjacent, degree, color_limit, colors, colored_count + 1U,
                          used_colors + 1U, search_nodes)) {
      return true;
    }
    colors[vertex] = kUncolored;
  }
  return false;
}

}  // namespace exact_coloring_detail

// Exact proper coloring of an undirected multigraph. Edge weights are ignored
// and parallel copies collapse to one adjacency relation. A self-loop makes a
// proper coloring impossible and returns std::nullopt.
[[nodiscard]] inline std::optional<ExactColoringResult> exact_graph_coloring_dsatur(
    const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("exact graph coloring requires an undirected graph");
  }

  const auto simple = exact_coloring_detail::simplify(graph);
  if (simple.has_self_loop) return std::nullopt;

  ExactColoringResult result;
  if (graph.vertex_count() == 0) return result;

  result.clique_lower_bound_witness = maximum_clique_bron_kerbosch(graph).vertices;
  std::vector<std::size_t> greedy =
      exact_coloring_detail::greedy_dsatur(simple.adjacent, simple.degree);
  result.greedy_upper_bound = exact_coloring_detail::used_color_count(greedy);

  if (result.clique_lower_bound_witness.size() == result.greedy_upper_bound) {
    result.chromatic_number = result.greedy_upper_bound;
    result.colors = std::move(greedy);
    return result;
  }

  for (std::size_t color_limit = result.clique_lower_bound_witness.size();
       color_limit < result.greedy_upper_bound; ++color_limit) {
    std::vector<std::size_t> candidate(graph.vertex_count(),
                                       exact_coloring_detail::kUncolored);
    std::size_t local_search_nodes = 0;
    if (exact_coloring_detail::search_k_coloring(
            simple.adjacent, simple.degree, color_limit, candidate, 0, 0,
            local_search_nodes)) {
      result.search_nodes += local_search_nodes;
      result.chromatic_number = color_limit;
      result.colors = std::move(candidate);
      return result;
    }
    result.search_nodes += local_search_nodes;
  }

  result.chromatic_number = result.greedy_upper_bound;
  result.colors = std::move(greedy);
  return result;
}

}  // namespace algorithms::graphs
