#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/max_flow.hpp"

namespace algorithms::graphs {

struct DensestSubgraphResult {
  std::vector<Vertex> vertices;
  std::size_t internal_edge_count{};

  friend bool operator==(const DensestSubgraphResult&,
                         const DensestSubgraphResult&) = default;
};

// Exact maximum-density non-empty induced subgraph for an undirected loopless
// multigraph. Density is logical internal edge copies divided by selected
// vertices. Parallel edges count with multiplicity; weights are ignored.
// Empty input returns an empty witness, and a non-empty edgeless graph returns
// {0}. Directed graphs and self-loops are rejected.
//
// The implementation uses an exact Goldberg-style s-t min-cut reduction,
// reusing repository Dinic. std::overflow_error is thrown if the scaled exact
// integer reduction is not representable by signed 64-bit Capacity.
namespace densest_subgraph_detail {

struct LogicalEdge {
  Vertex left;
  Vertex right;
};

[[nodiscard]] inline Capacity checked_size_to_capacity(
    std::size_t value, const char* message) {
  const auto limit = static_cast<std::uint64_t>(
      std::numeric_limits<Capacity>::max());
  if (value > limit) {
    throw std::overflow_error(message);
  }
  return static_cast<Capacity>(value);
}

[[nodiscard]] inline Capacity checked_add_nonnegative(
    Capacity left, Capacity right, const char* message) {
  if (left < 0 || right < 0 ||
      left > std::numeric_limits<Capacity>::max() - right) {
    throw std::overflow_error(message);
  }
  return left + right;
}

[[nodiscard]] inline Capacity checked_mul_nonnegative(
    Capacity left, Capacity right, const char* message) {
  if (left < 0 || right < 0) {
    throw std::overflow_error(message);
  }
  if (left != 0 &&
      right > std::numeric_limits<Capacity>::max() / left) {
    throw std::overflow_error(message);
  }
  return left * right;
}

struct ReductionContext {
  std::size_t vertex_count{};
  std::vector<LogicalEdge> edges;
  std::vector<std::size_t> degree;
  Capacity scale{};
  Capacity edge_scale{};
  Capacity baseline{};
};

[[nodiscard]] inline ReductionContext build_context(const Graph& graph) {
  if (graph.directed()) {
    throw std::invalid_argument("densest subgraph requires an undirected graph");
  }

  ReductionContext context;
  context.vertex_count = graph.vertex_count();
  context.degree.resize(context.vertex_count, 0U);

  for (Vertex from = 0; from < context.vertex_count; ++from) {
    context.degree[from] = graph.neighbors(from).size();
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to == from) {
        throw std::invalid_argument("densest subgraph does not support self-loops");
      }
      if (from < edge.to) {
        context.edges.push_back(LogicalEdge{from, edge.to});
      }
    }
  }

  if (context.vertex_count == 0 || context.edges.empty()) {
    return context;
  }
  if (context.vertex_count > std::numeric_limits<std::size_t>::max() - 2U) {
    throw std::length_error("densest subgraph reduction vertex count is too large");
  }

  const Capacity vertex_count = checked_size_to_capacity(
      context.vertex_count, "densest subgraph vertex count is not representable");
  const Capacity edge_count = checked_size_to_capacity(
      context.edges.size(), "densest subgraph edge count is not representable");
  const Capacity squared = checked_mul_nonnegative(
      vertex_count, vertex_count,
      "densest subgraph rational-isolation scale is not representable");
  context.scale = checked_add_nonnegative(
      squared, 1,
      "densest subgraph rational-isolation scale is not representable");
  context.edge_scale = checked_mul_nonnegative(
      edge_count, context.scale,
      "densest subgraph scaled edge count is not representable");
  context.baseline = checked_mul_nonnegative(
      context.edge_scale, vertex_count,
      "densest subgraph max-flow baseline is not representable");
  return context;
}

struct DecisionResult {
  bool feasible{};
  std::vector<bool> selected;
};

[[nodiscard]] inline DecisionResult has_density_above(
    const ReductionContext& context, Capacity threshold_numerator,
    bool capture_witness) {
  const std::size_t source = context.vertex_count;
  const std::size_t sink = context.vertex_count + 1U;
  const std::size_t network_vertex_count = context.vertex_count + 2U;

  if (context.vertex_count > (std::numeric_limits<std::size_t>::max() / 2U) ||
      context.edges.size() > (std::numeric_limits<std::size_t>::max() / 2U) ||
      context.vertex_count * 2U >
          std::numeric_limits<std::size_t>::max() - context.edges.size() * 2U) {
    throw std::length_error("densest subgraph reduction edge list is too large");
  }

  std::vector<CapacityEdge> network;
  network.reserve(context.vertex_count * 2U + context.edges.size() * 2U);

  const Capacity doubled_threshold = checked_mul_nonnegative(
      threshold_numerator, 2,
      "densest subgraph scaled threshold is not representable");

  for (Vertex vertex = 0; vertex < context.vertex_count; ++vertex) {
    const Capacity degree = checked_size_to_capacity(
        context.degree[vertex], "densest subgraph degree is not representable");
    const Capacity scaled_degree = checked_mul_nonnegative(
        degree, context.scale,
        "densest subgraph scaled degree is not representable");
    if (scaled_degree > context.edge_scale) {
      throw std::logic_error("densest subgraph degree exceeds logical edge count");
    }
    const Capacity sink_capacity = checked_add_nonnegative(
        context.edge_scale - scaled_degree, doubled_threshold,
        "densest subgraph sink capacity is not representable");
    network.push_back(CapacityEdge{source, vertex, context.edge_scale});
    network.push_back(CapacityEdge{vertex, sink, sink_capacity});
  }

  for (const auto& edge : context.edges) {
    network.push_back(CapacityEdge{edge.left, edge.right, context.scale});
    network.push_back(CapacityEdge{edge.right, edge.left, context.scale});
  }

  const auto flow =
      dinic_max_flow(network_vertex_count, network, source, sink);
  const bool feasible = flow.value < context.baseline;
  if (!capture_witness || !feasible) {
    return DecisionResult{feasible, {}};
  }

  std::vector<bool> selected(context.vertex_count, false);
  for (Vertex vertex = 0; vertex < context.vertex_count; ++vertex) {
    selected[vertex] = flow.source_side_min_cut[vertex];
  }
  return DecisionResult{true, std::move(selected)};
}

[[nodiscard]] inline std::size_t count_internal_edges(
    const ReductionContext& context, const std::vector<bool>& selected) {
  std::size_t count = 0;
  for (const auto& edge : context.edges) {
    if (selected[edge.left] && selected[edge.right]) {
      ++count;
    }
  }
  return count;
}

}  // namespace densest_subgraph_detail

[[nodiscard]] inline DensestSubgraphResult densest_subgraph(const Graph& graph) {
  using namespace densest_subgraph_detail;
  const ReductionContext context = build_context(graph);
  if (context.vertex_count == 0) {
    return DensestSubgraphResult{};
  }
  if (context.edges.empty()) {
    return DensestSubgraphResult{{0U}, 0U};
  }

  // Every feasible induced density is e/v with v <= n. Distinct such rationals
  // differ by at least 1/n^2. scale = n^2 + 1 therefore isolates the optimum:
  // the last integer threshold k/scale that is strictly below the optimum has no
  // suboptimal feasible density above it.
  Capacity low = 0;
  Capacity high = context.edge_scale;
  if (has_density_above(context, high, false).feasible) {
    throw std::logic_error("densest subgraph upper threshold must be infeasible");
  }
  if (!has_density_above(context, low, false).feasible) {
    throw std::logic_error("non-empty loopless graph must have positive density");
  }
  while (low + 1 < high) {
    const Capacity middle = low + (high - low) / 2;
    if (has_density_above(context, middle, false).feasible) {
      low = middle;
    } else {
      high = middle;
    }
  }

  const DecisionResult final = has_density_above(context, low, true);
  if (!final.feasible) {
    throw std::logic_error("densest subgraph witness threshold unexpectedly failed");
  }

  std::vector<Vertex> vertices;
  for (Vertex vertex = 0; vertex < context.vertex_count; ++vertex) {
    if (final.selected[vertex]) {
      vertices.push_back(vertex);
    }
  }
  if (vertices.empty()) {
    throw std::logic_error("densest subgraph reduction returned an empty witness");
  }

  const std::size_t internal_edges = count_internal_edges(context, final.selected);
  return DensestSubgraphResult{std::move(vertices), internal_edges};
}

}  // namespace algorithms::graphs
