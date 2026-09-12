#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct GeneralGraphIsomorphismResult {
  std::vector<Vertex> first_to_second;
  std::vector<Vertex> second_to_first;
  std::size_t search_nodes = 0;
};

namespace detail {

using MultiplicityMatrix = std::vector<std::vector<std::size_t>>;

inline MultiplicityMatrix gi_multiplicity_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  MultiplicityMatrix matrix(n, std::vector<std::size_t>(n, 0));
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (matrix[from][edge.to] == static_cast<std::size_t>(-1)) {
        throw std::overflow_error("graph edge multiplicity is not representable");
      }
      ++matrix[from][edge.to];
    }
  }
  return matrix;
}

inline std::vector<std::size_t> gi_static_signature(
    const MultiplicityMatrix& matrix, Vertex vertex) {
  std::vector<std::size_t> row = matrix[vertex];
  std::sort(row.begin(), row.end());
  std::vector<std::size_t> signature;
  signature.reserve(row.size() + 2);
  signature.push_back(matrix[vertex][vertex]);
  signature.push_back(0);
  for (std::size_t value : matrix[vertex]) {
    if (signature[1] > static_cast<std::size_t>(-1) - value) {
      throw std::overflow_error("graph degree multiplicity is not representable");
    }
    signature[1] += value;
  }
  signature.insert(signature.end(), row.begin(), row.end());
  return signature;
}

inline bool gi_refine_joint_colors(const MultiplicityMatrix& first,
                                   const MultiplicityMatrix& second,
                                   std::vector<std::size_t>& first_color,
                                   std::vector<std::size_t>& second_color) {
  const std::size_t n = first.size();
  std::map<std::vector<std::size_t>, std::size_t> initial_ids;
  for (std::size_t graph_index = 0; graph_index < 2; ++graph_index) {
    const MultiplicityMatrix& matrix = graph_index == 0 ? first : second;
    std::vector<std::size_t>& colors = graph_index == 0 ? first_color : second_color;
    for (Vertex v = 0; v < n; ++v) {
      const std::vector<std::size_t> signature = gi_static_signature(matrix, v);
      const auto [it, inserted] = initial_ids.emplace(signature, initial_ids.size());
      (void)inserted;
      colors[v] = it->second;
    }
  }

  for (;;) {
    const std::size_t color_count = initial_ids.size();
    std::map<std::vector<std::size_t>, std::size_t> ids;
    std::vector<std::size_t> next_first(n, 0);
    std::vector<std::size_t> next_second(n, 0);
    for (std::size_t graph_index = 0; graph_index < 2; ++graph_index) {
      const MultiplicityMatrix& matrix = graph_index == 0 ? first : second;
      const std::vector<std::size_t>& colors =
          graph_index == 0 ? first_color : second_color;
      std::vector<std::size_t>& next = graph_index == 0 ? next_first : next_second;
      for (Vertex v = 0; v < n; ++v) {
        std::vector<std::size_t> signature(color_count + 1, 0);
        signature[0] = colors[v];
        for (Vertex u = 0; u < n; ++u) {
          const std::size_t slot = colors[u] + 1;
          if (signature[slot] > static_cast<std::size_t>(-1) - matrix[v][u]) {
            throw std::overflow_error("graph refinement multiplicity is not representable");
          }
          signature[slot] += matrix[v][u];
        }
        const auto [it, inserted] = ids.emplace(signature, ids.size());
        (void)inserted;
        next[v] = it->second;
      }
    }

    std::map<std::size_t, std::pair<std::size_t, std::size_t>> counts;
    for (std::size_t color : next_first) {
      ++counts[color].first;
    }
    for (std::size_t color : next_second) {
      ++counts[color].second;
    }
    for (const auto& [color, count] : counts) {
      (void)color;
      if (count.first != count.second) {
        return false;
      }
    }

    if (next_first == first_color && next_second == second_color) {
      return true;
    }
    first_color = std::move(next_first);
    second_color = std::move(next_second);
    initial_ids.clear();
    for (std::size_t color : first_color) {
      initial_ids.emplace(std::vector<std::size_t>{color}, color);
    }
    for (std::size_t color : second_color) {
      initial_ids.emplace(std::vector<std::size_t>{color}, color);
    }
  }
}

struct GiSearchState {
  const MultiplicityMatrix& first;
  const MultiplicityMatrix& second;
  const std::vector<std::size_t>& first_color;
  const std::vector<std::size_t>& second_color;
  std::vector<std::optional<Vertex>> mapping;
  std::vector<std::optional<Vertex>> inverse;
  std::vector<Vertex> assigned_first;
  std::vector<Vertex> assigned_second;
  std::size_t search_nodes = 0;
};

inline std::vector<std::size_t> gi_dynamic_signature(
    const MultiplicityMatrix& matrix, const std::vector<std::size_t>& colors,
    Vertex vertex, const std::vector<Vertex>& assigned) {
  std::vector<std::size_t> signature;
  signature.reserve(assigned.size() + 1);
  signature.push_back(colors[vertex]);
  for (Vertex fixed : assigned) {
    signature.push_back(matrix[vertex][fixed]);
  }
  return signature;
}

inline bool gi_partition_compatible(const GiSearchState& state) {
  std::map<std::vector<std::size_t>, std::pair<std::size_t, std::size_t>> counts;
  const std::size_t n = state.first.size();
  for (Vertex v = 0; v < n; ++v) {
    if (!state.mapping[v].has_value()) {
      ++counts[gi_dynamic_signature(state.first, state.first_color, v,
                                    state.assigned_first)]
            .first;
    }
    if (!state.inverse[v].has_value()) {
      ++counts[gi_dynamic_signature(state.second, state.second_color, v,
                                    state.assigned_second)]
            .second;
    }
  }
  for (const auto& [signature, count] : counts) {
    (void)signature;
    if (count.first != count.second) {
      return false;
    }
  }
  return true;
}

inline bool gi_leaf_matches(const GiSearchState& state) {
  const std::size_t n = state.first.size();
  for (Vertex u = 0; u < n; ++u) {
    if (!state.mapping[u].has_value()) {
      return false;
    }
    for (Vertex v = 0; v < n; ++v) {
      if (!state.mapping[v].has_value() ||
          state.first[u][v] !=
              state.second[*state.mapping[u]][*state.mapping[v]]) {
        return false;
      }
    }
  }
  return true;
}

inline bool gi_search(GiSearchState& state) {
  if (state.search_nodes == static_cast<std::size_t>(-1)) {
    throw std::overflow_error("graph-isomorphism search counter overflow");
  }
  ++state.search_nodes;
  if (!gi_partition_compatible(state)) {
    return false;
  }

  const std::size_t n = state.first.size();
  if (state.assigned_first.size() == n) {
    return gi_leaf_matches(state);
  }

  Vertex chosen = n;
  std::vector<Vertex> candidates;
  for (Vertex a = 0; a < n; ++a) {
    if (state.mapping[a].has_value()) {
      continue;
    }
    const auto signature = gi_dynamic_signature(state.first, state.first_color, a,
                                                state.assigned_first);
    std::vector<Vertex> current;
    for (Vertex b = 0; b < n; ++b) {
      if (!state.inverse[b].has_value() &&
          gi_dynamic_signature(state.second, state.second_color, b,
                               state.assigned_second) == signature) {
        current.push_back(b);
      }
    }
    if (chosen == n || current.size() < candidates.size() ||
        (current.size() == candidates.size() && a < chosen)) {
      chosen = a;
      candidates = std::move(current);
    }
  }

  if (chosen == n || candidates.empty()) {
    return false;
  }

  for (Vertex candidate : candidates) {
    bool compatible = true;
    for (std::size_t i = 0; i < state.assigned_first.size(); ++i) {
      if (state.first[chosen][state.assigned_first[i]] !=
          state.second[candidate][state.assigned_second[i]]) {
        compatible = false;
        break;
      }
    }
    if (!compatible || state.first[chosen][chosen] != state.second[candidate][candidate]) {
      continue;
    }

    state.mapping[chosen] = candidate;
    state.inverse[candidate] = chosen;
    state.assigned_first.push_back(chosen);
    state.assigned_second.push_back(candidate);
    if (gi_search(state)) {
      return true;
    }
    state.assigned_second.pop_back();
    state.assigned_first.pop_back();
    state.inverse[candidate].reset();
    state.mapping[chosen].reset();
  }
  return false;
}

}  // namespace detail

// Exact isomorphism for undirected multigraphs under edge-multiplicity semantics.
// Edge weights are intentionally ignored. Self-loops and parallel edges are
// preserved by multiplicity. Directed inputs are rejected. The direct
// individualization/refinement baseline is exponential in the worst case and
// makes no polynomial- or quasi-polynomial-time graph-isomorphism claim.
[[nodiscard]] inline std::optional<GeneralGraphIsomorphismResult>
exact_general_graph_isomorphism(const Graph& first, const Graph& second) {
  if (first.directed() || second.directed()) {
    throw std::invalid_argument("general graph isomorphism requires undirected graphs");
  }
  if (first.vertex_count() != second.vertex_count()) {
    return std::nullopt;
  }
  const std::size_t n = first.vertex_count();
  const detail::MultiplicityMatrix first_matrix = detail::gi_multiplicity_matrix(first);
  const detail::MultiplicityMatrix second_matrix = detail::gi_multiplicity_matrix(second);
  std::vector<std::size_t> first_color(n, 0);
  std::vector<std::size_t> second_color(n, 0);
  if (!detail::gi_refine_joint_colors(first_matrix, second_matrix, first_color,
                                      second_color)) {
    return std::nullopt;
  }

  detail::GiSearchState state{first_matrix,
                              second_matrix,
                              first_color,
                              second_color,
                              std::vector<std::optional<Vertex>>(n),
                              std::vector<std::optional<Vertex>>(n),
                              {},
                              {},
                              0};
  if (!detail::gi_search(state)) {
    return std::nullopt;
  }

  GeneralGraphIsomorphismResult result;
  result.first_to_second.resize(n);
  result.second_to_first.resize(n);
  for (Vertex v = 0; v < n; ++v) {
    result.first_to_second[v] = *state.mapping[v];
    result.second_to_first[v] = *state.inverse[v];
  }
  result.search_nodes = state.search_nodes;
  return result;
}

}  // namespace algorithms::graphs
