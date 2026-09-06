#include "test_framework.hpp"

#include "algorithms/graphs/max_flow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::graphs::Capacity;
using algorithms::graphs::CapacityEdge;
using algorithms::graphs::FlowEdge;
using algorithms::graphs::Vertex;
using algorithms::graphs::dinic_max_flow;

Capacity edmonds_karp(std::size_t n, const std::vector<CapacityEdge>& edges,
                      Vertex source, Vertex sink) {
  std::vector<std::vector<Capacity>> residual(n, std::vector<Capacity>(n, 0));
  for (const auto& edge : edges) {
    if (edge.from != edge.to) {
      residual[edge.from][edge.to] += edge.capacity;
    }
  }
  Capacity total = 0;
  while (true) {
    std::vector<Vertex> parent(n, n);
    std::queue<Vertex> queue;
    parent[source] = source;
    queue.push(source);
    while (!queue.empty() && parent[sink] == n) {
      const Vertex u = queue.front();
      queue.pop();
      for (Vertex v = 0; v < n; ++v) {
        if (parent[v] == n && residual[u][v] > 0) {
          parent[v] = u;
          queue.push(v);
        }
      }
    }
    if (parent[sink] == n) {
      return total;
    }
    Capacity pushed = std::numeric_limits<Capacity>::max();
    for (Vertex v = sink; v != source; v = parent[v]) {
      pushed = std::min(pushed, residual[parent[v]][v]);
    }
    for (Vertex v = sink; v != source; v = parent[v]) {
      residual[parent[v]][v] -= pushed;
      residual[v][parent[v]] += pushed;
    }
    total += pushed;
  }
}

Capacity brute_force_min_cut(std::size_t n,
                             const std::vector<CapacityEdge>& edges,
                             Vertex source, Vertex sink) {
  std::vector<Vertex> middle;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (vertex != source && vertex != sink) {
      middle.push_back(vertex);
    }
  }
  Capacity best = std::numeric_limits<Capacity>::max();
  const std::uint64_t choices = std::uint64_t{1} << middle.size();
  for (std::uint64_t mask = 0; mask < choices; ++mask) {
    std::vector<bool> in_source(n, false);
    in_source[source] = true;
    for (std::size_t bit = 0; bit < middle.size(); ++bit) {
      if ((mask & (std::uint64_t{1} << bit)) != 0) {
        in_source[middle[bit]] = true;
      }
    }
    Capacity cut = 0;
    for (const auto& edge : edges) {
      if (in_source[edge.from] && !in_source[edge.to]) {
        cut += edge.capacity;
      }
    }
    best = std::min(best, cut);
  }
  return best;
}

void verify_certificate(std::size_t n, const std::vector<CapacityEdge>& input,
                        Vertex source, Vertex sink) {
  const auto result = dinic_max_flow(n, input, source, sink);
  REQUIRE_EQ(result.edges.size(), input.size());
  REQUIRE_EQ(result.source_side_min_cut.size(), n);
  REQUIRE(result.source_side_min_cut[source]);
  REQUIRE(!result.source_side_min_cut[sink]);
  REQUIRE_EQ(result.value, result.cut_capacity);

  std::vector<Capacity> balance(n, 0);
  for (std::size_t i = 0; i < input.size(); ++i) {
    const FlowEdge& edge = result.edges[i];
    REQUIRE_EQ(edge.from, input[i].from);
    REQUIRE_EQ(edge.to, input[i].to);
    REQUIRE_EQ(edge.capacity, input[i].capacity);
    REQUIRE(edge.flow >= 0);
    REQUIRE(edge.flow <= edge.capacity);
    balance[edge.from] -= edge.flow;
    balance[edge.to] += edge.flow;
    if (result.source_side_min_cut[edge.from] &&
        !result.source_side_min_cut[edge.to]) {
      REQUIRE_EQ(edge.flow, edge.capacity);
    }
  }
  REQUIRE_EQ(-balance[source], result.value);
  REQUIRE_EQ(balance[sink], result.value);
  for (Vertex v = 0; v < n; ++v) {
    if (v != source && v != sink) {
      REQUIRE_EQ(balance[v], Capacity{0});
    }
  }
}

TEST_CASE(max_flow_classic_network_certificate) {
  const std::vector<CapacityEdge> edges{{0, 1, 16}, {0, 2, 13}, {1, 2, 10},
                                        {2, 1, 4},  {1, 3, 12}, {3, 2, 9},
                                        {2, 4, 14}, {4, 3, 7},  {3, 5, 20},
                                        {4, 5, 4}};
  const auto result = dinic_max_flow(6, edges, 0, 5);
  REQUIRE_EQ(result.value, Capacity{23});
  REQUIRE_EQ(result.cut_capacity, Capacity{23});
  verify_certificate(6, edges, 0, 5);
}

TEST_CASE(max_flow_parallel_self_loop_zero_and_disconnected) {
  const std::vector<CapacityEdge> edges{{0, 0, 99}, {0, 1, 0}, {0, 1, 3},
                                        {0, 1, 4},  {1, 2, 5}, {1, 2, 6},
                                        {3, 4, 7}};
  const auto result = dinic_max_flow(5, edges, 0, 2);
  REQUIRE_EQ(result.value, Capacity{7});
  REQUIRE_EQ(result.edges[0].flow, Capacity{0});
  REQUIRE_EQ(result.edges[1].flow, Capacity{0});
  verify_certificate(5, edges, 0, 2);

  const auto disconnected = dinic_max_flow(5, edges, 0, 4);
  REQUIRE_EQ(disconnected.value, Capacity{0});
  REQUIRE_EQ(disconnected.cut_capacity, Capacity{0});
}

TEST_CASE(max_flow_rejects_invalid_inputs_and_overflow) {
  const std::vector<CapacityEdge> bad_vertex{{0, 3, 1}};
  REQUIRE_THROWS_AS(dinic_max_flow(3, bad_vertex, 0, 2), std::out_of_range);
  const std::vector<CapacityEdge> negative{{0, 1, -1}};
  REQUIRE_THROWS_AS(dinic_max_flow(2, negative, 0, 1), std::invalid_argument);
  const std::vector<CapacityEdge> empty;
  REQUIRE_THROWS_AS(dinic_max_flow(2, empty, 0, 0), std::invalid_argument);
  REQUIRE_THROWS_AS(dinic_max_flow(2, empty, 0, 2), std::out_of_range);

  const Capacity big = std::numeric_limits<Capacity>::max();
  const std::vector<CapacityEdge> overflow{{0, 1, big}, {1, 3, big},
                                           {0, 2, big}, {2, 3, big}};
  REQUIRE_THROWS_AS(dinic_max_flow(4, overflow, 0, 3), std::overflow_error);
}

TEST_CASE(max_flow_randomized_differential_and_min_cut) {
  std::mt19937_64 rng(0xF10A6EEDULL);
  for (std::size_t trial = 0; trial < 300; ++trial) {
    const std::size_t n = 2 + static_cast<std::size_t>(rng() % 7U);
    const Vertex source = 0;
    const Vertex sink = n - 1;
    std::vector<CapacityEdge> edges;
    const std::size_t edge_count = static_cast<std::size_t>(rng() % 28U);
    edges.reserve(edge_count);
    for (std::size_t e = 0; e < edge_count; ++e) {
      const Vertex from = static_cast<Vertex>(rng() % n);
      const Vertex to = static_cast<Vertex>(rng() % n);
      const Capacity capacity = static_cast<Capacity>(rng() % 26U);
      edges.push_back(CapacityEdge{from, to, capacity});
    }
    const auto result = dinic_max_flow(n, edges, source, sink);
    REQUIRE_EQ(result.value, edmonds_karp(n, edges, source, sink));
    REQUIRE_EQ(result.value, brute_force_min_cut(n, edges, source, sink));
    verify_certificate(n, edges, source, sink);
  }
}

}  // namespace
