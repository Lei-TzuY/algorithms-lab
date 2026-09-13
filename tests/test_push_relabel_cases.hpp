#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/graphs/push_relabel_max_flow.hpp"
#include "test_framework.hpp"

namespace {

using algorithms::graphs::Capacity;
using algorithms::graphs::CapacityEdge;
using algorithms::graphs::MaxFlowResult;
using algorithms::graphs::Vertex;

Capacity exhaustive_min_cut(std::size_t vertex_count,
                            const std::vector<CapacityEdge>& edges,
                            Vertex source, Vertex sink) {
  std::vector<Vertex> internal;
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != source && vertex != sink) {
      internal.push_back(vertex);
    }
  }
  Capacity best = std::numeric_limits<Capacity>::max();
  const std::size_t subsets = std::size_t{1} << internal.size();
  for (std::size_t mask = 0; mask < subsets; ++mask) {
    std::vector<bool> side(vertex_count, false);
    side[source] = true;
    for (std::size_t bit = 0; bit < internal.size(); ++bit) {
      if ((mask & (std::size_t{1} << bit)) != 0) {
        side[internal[bit]] = true;
      }
    }
    Capacity cut = 0;
    for (const auto& edge : edges) {
      if (side[edge.from] && !side[edge.to]) {
        cut += edge.capacity;
      }
    }
    best = std::min(best, cut);
  }
  return best;
}

void verify_small_flow(const MaxFlowResult& result, std::size_t vertex_count,
                       const std::vector<CapacityEdge>& input, Vertex source,
                       Vertex sink) {
  REQUIRE_EQ(result.edges.size(), input.size());
  REQUIRE_EQ(result.source_side_min_cut.size(), vertex_count);
  REQUIRE(result.source_side_min_cut[source]);
  REQUIRE(!result.source_side_min_cut[sink]);

  std::vector<Capacity> balance(vertex_count, 0);
  Capacity cut = 0;
  for (std::size_t index = 0; index < input.size(); ++index) {
    const auto& expected = input[index];
    const auto& actual = result.edges[index];
    REQUIRE_EQ(actual.from, expected.from);
    REQUIRE_EQ(actual.to, expected.to);
    REQUIRE_EQ(actual.capacity, expected.capacity);
    REQUIRE(actual.flow >= 0);
    REQUIRE(actual.flow <= actual.capacity);
    if (actual.from == actual.to) {
      REQUIRE_EQ(actual.flow, Capacity{0});
    } else {
      balance[actual.from] -= actual.flow;
      balance[actual.to] += actual.flow;
    }
    if (result.source_side_min_cut[actual.from] &&
        !result.source_side_min_cut[actual.to]) {
      cut += actual.capacity;
    }
  }
  REQUIRE_EQ(balance[source], -result.value);
  REQUIRE_EQ(balance[sink], result.value);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (vertex != source && vertex != sink) {
      REQUIRE_EQ(balance[vertex], Capacity{0});
    }
  }
  REQUIRE_EQ(cut, result.cut_capacity);
  REQUIRE_EQ(result.cut_capacity, result.value);
}

TEST_CASE(push_relabel_validation_and_classic_network) {
  const std::vector<CapacityEdge> classic = {
      {0, 1, 16}, {0, 2, 13}, {1, 2, 10}, {2, 1, 4}, {1, 3, 12},
      {3, 2, 9},  {2, 4, 14}, {4, 3, 7},  {3, 5, 20}, {4, 5, 4}};
  const auto result =
      algorithms::graphs::push_relabel_max_flow(6, classic, 0, 5);
  REQUIRE_EQ(result.value, Capacity{23});
  REQUIRE_EQ(result.cut_capacity, Capacity{23});
  verify_small_flow(result, 6, classic, 0, 5);

  const auto repeated =
      algorithms::graphs::push_relabel_max_flow(6, classic, 0, 5);
  REQUIRE_EQ(repeated.value, result.value);
  REQUIRE(repeated.edges == result.edges);
  REQUIRE(repeated.source_side_min_cut == result.source_side_min_cut);

  REQUIRE_THROWS_AS(
      algorithms::graphs::push_relabel_max_flow(0, classic, 0, 0),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      algorithms::graphs::push_relabel_max_flow(6, classic, 0, 0),
      std::invalid_argument);
  const std::vector<CapacityEdge> bad_endpoint = {{0, 2, 1}};
  REQUIRE_THROWS_AS(
      algorithms::graphs::push_relabel_max_flow(2, bad_endpoint, 0, 1),
      std::out_of_range);
  const std::vector<CapacityEdge> negative = {{0, 1, -1}};
  REQUIRE_THROWS_AS(
      algorithms::graphs::push_relabel_max_flow(2, negative, 0, 1),
      std::invalid_argument);
}

TEST_CASE(push_relabel_multigraph_self_loop_and_disconnected_semantics) {
  const std::vector<CapacityEdge> edges = {
      {0, 0, 99}, {0, 1, 3}, {0, 1, 5}, {1, 0, 7},
      {1, 2, 4},  {1, 2, 2}, {2, 1, 8}, {2, 3, 6},
      {1, 3, 1},  {3, 3, 50}, {0, 2, 0}};
  const auto result =
      algorithms::graphs::push_relabel_max_flow(4, edges, 0, 3);
  const Capacity optimum = exhaustive_min_cut(4, edges, 0, 3);
  REQUIRE_EQ(result.value, optimum);
  verify_small_flow(result, 4, edges, 0, 3);

  const std::vector<CapacityEdge> dead_end = {{0, 1, 8}, {1, 2, 8}};
  const auto zero =
      algorithms::graphs::push_relabel_max_flow(4, dead_end, 0, 3);
  REQUIRE_EQ(zero.value, Capacity{0});
  verify_small_flow(zero, 4, dead_end, 0, 3);
}

TEST_CASE(push_relabel_representability_boundaries) {
  const Capacity maximum = std::numeric_limits<Capacity>::max();
  const std::vector<CapacityEdge> exact = {{0, 1, maximum}};
  const auto result =
      algorithms::graphs::push_relabel_max_flow(2, exact, 0, 1);
  REQUIRE_EQ(result.value, maximum);
  REQUIRE_EQ(result.cut_capacity, maximum);
  REQUIRE_EQ(result.edges[0].flow, maximum);

  const std::vector<CapacityEdge> wide_temporary_excess = {
      {0, 1, maximum}, {0, 1, maximum}, {0, 1, maximum}, {1, 2, 1}};
  const auto narrow_optimum = algorithms::graphs::push_relabel_max_flow(
      3, wide_temporary_excess, 0, 2);
  REQUIRE_EQ(narrow_optimum.value, Capacity{1});
  REQUIRE_EQ(narrow_optimum.cut_capacity, Capacity{1});
  verify_small_flow(narrow_optimum, 3, wide_temporary_excess, 0, 2);

  const std::vector<CapacityEdge> overflow = {
      {0, 1, maximum}, {0, 1, maximum}};
  REQUIRE_THROWS_AS(
      algorithms::graphs::push_relabel_max_flow(2, overflow, 0, 1),
      std::overflow_error);
}

TEST_CASE(push_relabel_randomized_exhaustive_cut_differential) {
  std::mt19937_64 random(0x5055534852454C41ULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t vertex_count = 2 + static_cast<std::size_t>(random() % 7);
    const Vertex source = 0;
    const Vertex sink = vertex_count - 1;
    std::vector<CapacityEdge> edges;
    const std::size_t edge_count = static_cast<std::size_t>(random() % 40);
    edges.reserve(edge_count);
    for (std::size_t index = 0; index < edge_count; ++index) {
      const Vertex from = static_cast<Vertex>(random() % vertex_count);
      const Vertex to = static_cast<Vertex>(random() % vertex_count);
      const Capacity capacity = static_cast<Capacity>(random() % 9);
      edges.push_back(CapacityEdge{from, to, capacity});
    }

    const auto result = algorithms::graphs::push_relabel_max_flow(
        vertex_count, edges, source, sink);
    const Capacity optimum =
        exhaustive_min_cut(vertex_count, edges, source, sink);
    const auto dinic = algorithms::graphs::dinic_max_flow(
        vertex_count, edges, source, sink);
    REQUIRE_EQ(result.value, optimum);
    REQUIRE_EQ(result.value, dinic.value);
    verify_small_flow(result, vertex_count, edges, source, sink);
  }
}

}  // namespace
