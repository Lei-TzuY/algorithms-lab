#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/max_flow.hpp"

namespace algorithms::graphs {

struct MaximumWeightClosureResult {
  std::int64_t maximum_weight;
  std::int64_t min_cut_capacity;
  std::int64_t total_positive_weight;
  std::vector<Vertex> selected_vertices;
};

// Exact maximum-weight closure in a directed implication graph.
// Selecting u requires selecting every out-neighbor v. Stored edge weights,
// parallel copies, and self-loops do not change the closure relation.
//
// The empty set is always feasible. If the optimum value is zero, this
// function returns the empty set canonically.
//
// This reduction reuses the signed-64 Dinic surface, so the sum of all positive
// vertex weights must itself fit in int64_t. Negative INT64_MIN weights are
// supported: sink capacities larger than the total positive weight are safely
// capped because such penalties cannot participate in a positive optimum.
[[nodiscard]] inline MaximumWeightClosureResult maximum_weight_closure(
    const Graph& implication_graph,
    std::span<const std::int64_t> vertex_weights) {
  if (!implication_graph.directed()) {
    throw std::invalid_argument("maximum-weight closure requires a directed graph");
  }
  const std::size_t vertex_count = implication_graph.vertex_count();
  if (vertex_weights.size() != vertex_count) {
    throw std::invalid_argument("maximum-weight closure weight count must match vertices");
  }
  if (vertex_count > std::numeric_limits<std::size_t>::max() - 2U) {
    throw std::length_error("maximum-weight closure vertex count is too large");
  }

  std::int64_t positive_sum = 0;
  for (const auto weight : vertex_weights) {
    if (weight > 0) {
      if (positive_sum > std::numeric_limits<std::int64_t>::max() - weight) {
        throw std::overflow_error(
            "maximum-weight closure positive-weight sum is not representable");
      }
      positive_sum += weight;
    }
  }

  if (positive_sum == 0) {
    return MaximumWeightClosureResult{0, 0, 0, {}};
  }

  const Vertex source = vertex_count;
  const Vertex sink = vertex_count + 1U;
  std::vector<CapacityEdge> reduction_edges;

  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    const std::int64_t weight = vertex_weights[vertex];
    if (weight > 0) {
      reduction_edges.push_back(CapacityEdge{source, vertex, weight});
    } else if (weight < 0) {
      const std::uint64_t magnitude =
          weight == std::numeric_limits<std::int64_t>::min()
              ? (std::uint64_t{1} << 63U)
              : static_cast<std::uint64_t>(-weight);
      const std::uint64_t positive_limit =
          static_cast<std::uint64_t>(positive_sum);
      const Capacity penalty = static_cast<Capacity>(
          magnitude < positive_limit ? magnitude : positive_limit);
      reduction_edges.push_back(CapacityEdge{vertex, sink, penalty});
    }
  }

  std::set<std::pair<Vertex, Vertex>> implications;
  for (Vertex from = 0; from < vertex_count; ++from) {
    for (const auto& edge : implication_graph.neighbors(from)) {
      if (from != edge.to) {
        implications.emplace(from, edge.to);
      }
    }
  }
  reduction_edges.reserve(reduction_edges.size() + implications.size());
  for (const auto& [from, to] : implications) {
    reduction_edges.push_back(CapacityEdge{from, to, positive_sum});
  }

  const MaxFlowResult flow =
      dinic_max_flow(vertex_count + 2U, reduction_edges, source, sink);
  if (flow.value > positive_sum) {
    throw std::logic_error("maximum-weight closure reduction exceeded positive sum");
  }

  if (flow.value == positive_sum) {
    return MaximumWeightClosureResult{0, flow.value, positive_sum, {}};
  }

  std::vector<Vertex> selected;
  selected.reserve(vertex_count);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (flow.source_side_min_cut[vertex]) {
      selected.push_back(vertex);
    }
  }

  std::vector<bool> chosen(vertex_count, false);
  for (const Vertex vertex : selected) {
    chosen[vertex] = true;
  }
  for (const auto& [from, to] : implications) {
    if (chosen[from] && !chosen[to]) {
      throw std::logic_error("maximum-weight closure returned a non-closed witness");
    }
  }

  return MaximumWeightClosureResult{positive_sum - flow.value, flow.value,
                                    positive_sum, std::move(selected)};
}

}  // namespace algorithms::graphs
