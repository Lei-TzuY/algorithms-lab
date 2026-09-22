#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

enum class DistanceHereditaryStepKind : unsigned char {
  pendant,
  false_twin,
  true_twin,
};

struct DistanceHereditaryStep {
  DistanceHereditaryStepKind kind{DistanceHereditaryStepKind::pendant};
  Vertex vertex{};
  Vertex reference{};

  friend bool operator==(const DistanceHereditaryStep&,
                         const DistanceHereditaryStep&) = default;
};

struct DistanceHereditaryPruning {
  std::vector<DistanceHereditaryStep> steps;
  std::optional<Vertex> survivor;

  friend bool operator==(const DistanceHereditaryPruning&,
                         const DistanceHereditaryPruning&) = default;
};

namespace distance_hereditary_detail {

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

[[nodiscard]] inline std::optional<Vertex> unique_active_neighbor(
    const Vertex vertex, const AdjacencyMatrix& adjacency,
    const std::vector<std::uint8_t>& active) {
  std::optional<Vertex> neighbor;
  for (Vertex candidate = 0U; candidate < active.size(); ++candidate) {
    if (candidate == vertex || active[candidate] == 0U ||
        adjacency[vertex][candidate] == 0U) {
      continue;
    }
    if (neighbor.has_value()) {
      return std::nullopt;
    }
    neighbor = candidate;
  }
  return neighbor;
}

[[nodiscard]] inline bool twins(
    const Vertex first, const Vertex second,
    const AdjacencyMatrix& adjacency,
    const std::vector<std::uint8_t>& active) {
  for (Vertex candidate = 0U; candidate < active.size(); ++candidate) {
    if (active[candidate] == 0U || candidate == first ||
        candidate == second) {
      continue;
    }
    if (adjacency[first][candidate] != adjacency[second][candidate]) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool valid_step(
    const DistanceHereditaryStep& step, const AdjacencyMatrix& adjacency,
    const std::vector<std::uint8_t>& active) {
  if (step.vertex >= active.size() || step.reference >= active.size() ||
      step.vertex == step.reference || active[step.vertex] == 0U ||
      active[step.reference] == 0U) {
    return false;
  }

  if (step.kind == DistanceHereditaryStepKind::pendant) {
    const auto neighbor =
        unique_active_neighbor(step.vertex, adjacency, active);
    return neighbor.has_value() && *neighbor == step.reference;
  }

  if (!twins(step.vertex, step.reference, adjacency, active)) {
    return false;
  }
  const bool adjacent = adjacency[step.vertex][step.reference] != 0U;
  if (step.kind == DistanceHereditaryStepKind::false_twin) {
    return !adjacent;
  }
  if (step.kind == DistanceHereditaryStepKind::true_twin) {
    return adjacent;
  }
  return false;
}

}  // namespace distance_hereditary_detail

// Returns a deterministic pendant/twin pruning witness when the structural
// simple graph is distance-hereditary, and nullopt otherwise.
//
// Self-loops are ignored, parallel copies collapse, and weights do not affect
// the structural graph. Directed input is rejected.
[[nodiscard]] inline std::optional<DistanceHereditaryPruning>
distance_hereditary_pruning(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument(
        "distance-hereditary recognition requires an undirected graph");
  }

  const std::size_t n = graph.vertex_count();
  DistanceHereditaryPruning result;
  if (n == 0U) {
    return result;
  }

  const auto adjacency = distance_hereditary_detail::simple_adjacency(graph);
  std::vector<std::uint8_t> active(n, 1U);
  std::size_t active_count = n;
  result.steps.reserve(n - 1U);

  while (active_count > 1U) {
    std::optional<DistanceHereditaryStep> selected;

    // Prefer the smallest active pendant vertex.
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      if (active[vertex] == 0U) {
        continue;
      }
      const auto neighbor =
          distance_hereditary_detail::unique_active_neighbor(
              vertex, adjacency, active);
      if (neighbor.has_value()) {
        selected = DistanceHereditaryStep{
            DistanceHereditaryStepKind::pendant, vertex, *neighbor};
        break;
      }
    }

    // Otherwise choose the lexicographically first active twin pair.
    if (!selected.has_value()) {
      for (Vertex first = 0U; first < n && !selected.has_value(); ++first) {
        if (active[first] == 0U) {
          continue;
        }
        for (Vertex second = first + 1U; second < n; ++second) {
          if (active[second] == 0U ||
              !distance_hereditary_detail::twins(
                  first, second, adjacency, active)) {
            continue;
          }
          selected = DistanceHereditaryStep{
              adjacency[first][second] != 0U
                  ? DistanceHereditaryStepKind::true_twin
                  : DistanceHereditaryStepKind::false_twin,
              first, second};
          break;
        }
      }
    }

    if (!selected.has_value()) {
      return std::nullopt;
    }

    active[selected->vertex] = 0U;
    --active_count;
    result.steps.push_back(*selected);
  }

  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if (active[vertex] != 0U) {
      result.survivor = vertex;
      break;
    }
  }
  return result;
}

// Expensive replay validator for a pruning witness. This checks the witness
// directly against the structural simple graph; it does not rerun recognition.
[[nodiscard]] inline bool valid_distance_hereditary_pruning(
    const Graph& graph, const DistanceHereditaryPruning& pruning) {
  if (graph.directed()) {
    return false;
  }

  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    return pruning.steps.empty() && !pruning.survivor.has_value();
  }
  if (!pruning.survivor.has_value() || *pruning.survivor >= n ||
      pruning.steps.size() != n - 1U) {
    return false;
  }

  const auto adjacency = distance_hereditary_detail::simple_adjacency(graph);
  std::vector<std::uint8_t> active(n, 1U);

  for (const auto& step : pruning.steps) {
    if (!distance_hereditary_detail::valid_step(step, adjacency, active)) {
      return false;
    }
    active[step.vertex] = 0U;
  }

  std::size_t survivors = 0U;
  Vertex survivor = 0U;
  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    if (active[vertex] != 0U) {
      ++survivors;
      survivor = vertex;
    }
  }
  return survivors == 1U && survivor == *pruning.survivor;
}

}  // namespace algorithms::graphs
