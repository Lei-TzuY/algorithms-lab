#pragma once

#include "algorithms/graphs/online_bridge_connectivity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace online_bridge_connectivity_test_detail {

using algorithms::graphs::OnlineBridgeConnectivity;
using algorithms::graphs::Vertex;

struct OracleEdge {
  Vertex first;
  Vertex second;
};

struct AdjacentEdge {
  Vertex to;
  std::size_t edge_id;
};

struct OracleState {
  std::size_t bridge_count{};
  std::size_t component_count{};
  std::vector<std::size_t> connected_component;
  std::vector<std::size_t> two_edge_component;
};

inline void bridge_dfs(
    const Vertex vertex, const std::size_t parent_edge,
    const std::vector<std::vector<AdjacentEdge>>& adjacency,
    std::size_t& timer, std::vector<std::size_t>& discovery,
    std::vector<std::size_t>& low, std::vector<unsigned char>& is_bridge) {
  discovery[vertex] = timer;
  low[vertex] = timer;
  ++timer;

  for (const AdjacentEdge edge : adjacency[vertex]) {
    if (edge.edge_id == parent_edge) {
      continue;
    }
    if (discovery[edge.to] != std::numeric_limits<std::size_t>::max()) {
      low[vertex] = std::min(low[vertex], discovery[edge.to]);
      continue;
    }

    bridge_dfs(edge.to, edge.edge_id, adjacency, timer, discovery, low,
               is_bridge);
    low[vertex] = std::min(low[vertex], low[edge.to]);
    if (low[edge.to] > discovery[vertex]) {
      is_bridge[edge.edge_id] = 1U;
    }
  }
}

inline std::vector<std::size_t> component_labels(
    const std::vector<std::vector<AdjacentEdge>>& adjacency,
    const std::vector<unsigned char>& blocked_edges,
    std::size_t& component_count) {
  const std::size_t vertex_count = adjacency.size();
  const std::size_t no_component = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> label(vertex_count, no_component);
  std::queue<Vertex> pending;
  component_count = 0U;

  for (Vertex start = 0U; start < vertex_count; ++start) {
    if (label[start] != no_component) {
      continue;
    }
    label[start] = component_count;
    pending.push(start);

    while (!pending.empty()) {
      const Vertex from = pending.front();
      pending.pop();
      for (const AdjacentEdge edge : adjacency[from]) {
        if (blocked_edges[edge.edge_id] != 0U ||
            label[edge.to] != no_component) {
          continue;
        }
        label[edge.to] = component_count;
        pending.push(edge.to);
      }
    }
    ++component_count;
  }

  return label;
}

inline OracleState recompute_oracle(
    const std::size_t vertex_count, const std::vector<OracleEdge>& edges) {
  std::vector<std::vector<AdjacentEdge>> adjacency(vertex_count);
  for (std::size_t edge_id = 0U; edge_id < edges.size(); ++edge_id) {
    const OracleEdge edge = edges[edge_id];
    adjacency[edge.first].push_back({edge.second, edge_id});
    adjacency[edge.second].push_back({edge.first, edge_id});
  }

  const std::size_t unseen = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> discovery(vertex_count, unseen);
  std::vector<std::size_t> low(vertex_count, unseen);
  std::vector<unsigned char> is_bridge(edges.size(), 0U);
  std::size_t timer = 0U;

  for (Vertex vertex = 0U; vertex < vertex_count; ++vertex) {
    if (discovery[vertex] == unseen) {
      bridge_dfs(vertex, edges.size(), adjacency, timer, discovery, low,
                 is_bridge);
    }
  }

  OracleState state;
  for (const unsigned char flag : is_bridge) {
    state.bridge_count += flag != 0U ? 1U : 0U;
  }

  std::vector<unsigned char> no_blocked_edges(edges.size(), 0U);
  state.connected_component =
      component_labels(adjacency, no_blocked_edges, state.component_count);

  std::size_t two_edge_component_count = 0U;
  state.two_edge_component =
      component_labels(adjacency, is_bridge, two_edge_component_count);
  return state;
}

inline void require_matches(
    const OnlineBridgeConnectivity& index,
    const std::vector<OracleEdge>& edges) {
  const OracleState oracle = recompute_oracle(index.vertex_count(), edges);

  REQUIRE(index.valid_invariants());
  REQUIRE_EQ(index.edge_count(), edges.size());
  REQUIRE_EQ(index.bridge_count(), oracle.bridge_count);
  REQUIRE_EQ(index.component_count(), oracle.component_count);

  for (Vertex first = 0U; first < index.vertex_count(); ++first) {
    for (Vertex second = 0U; second < index.vertex_count(); ++second) {
      REQUIRE_EQ(index.connected(first, second),
                 oracle.connected_component[first] ==
                     oracle.connected_component[second]);
      REQUIRE_EQ(index.same_two_edge_component(first, second),
                 oracle.two_edge_component[first] ==
                     oracle.two_edge_component[second]);
    }
  }
}

}  // namespace online_bridge_connectivity_test_detail

TEST_CASE(online_bridge_connectivity_chain_cycle_parallel_and_self_loop) {
  using namespace online_bridge_connectivity_test_detail;

  OnlineBridgeConnectivity index(5U);
  std::vector<OracleEdge> edges;
  require_matches(index, edges);

  REQUIRE_EQ(index.add_edge(0U, 1U), 0U);
  edges.push_back({0U, 1U});
  REQUIRE_EQ(index.bridge_count(), 1U);
  require_matches(index, edges);

  REQUIRE_EQ(index.add_edge(1U, 2U), 1U);
  edges.push_back({1U, 2U});
  REQUIRE_EQ(index.bridge_count(), 2U);
  require_matches(index, edges);

  REQUIRE_EQ(index.add_edge(0U, 2U), 2U);
  edges.push_back({0U, 2U});
  REQUIRE_EQ(index.bridge_count(), 0U);
  REQUIRE(index.same_two_edge_component(0U, 2U));
  require_matches(index, edges);

  REQUIRE_EQ(index.add_edge(2U, 3U), 3U);
  edges.push_back({2U, 3U});
  REQUIRE_EQ(index.bridge_count(), 1U);
  require_matches(index, edges);

  // A parallel edge creates a length-two multigraph cycle and removes the
  // bridge status of the original 2--3 edge.
  REQUIRE_EQ(index.add_edge(2U, 3U), 4U);
  edges.push_back({2U, 3U});
  REQUIRE_EQ(index.bridge_count(), 0U);
  REQUIRE(index.same_two_edge_component(2U, 3U));
  require_matches(index, edges);

  REQUIRE_EQ(index.add_edge(4U, 4U), 5U);
  edges.push_back({4U, 4U});
  REQUIRE_EQ(index.bridge_count(), 0U);
  REQUIRE(!index.connected(0U, 4U));
  require_matches(index, edges);
}

TEST_CASE(online_bridge_connectivity_two_cycles_joined_by_bridge) {
  using namespace online_bridge_connectivity_test_detail;

  OnlineBridgeConnectivity index(7U);
  std::vector<OracleEdge> edges;
  const std::vector<OracleEdge> insertion_order = {
      {0U, 1U}, {1U, 2U}, {2U, 0U}, {3U, 4U},
      {4U, 5U}, {5U, 3U}, {2U, 3U}, {5U, 6U}};

  for (const OracleEdge edge : insertion_order) {
    REQUIRE_EQ(index.add_edge(edge.first, edge.second), edges.size());
    edges.push_back(edge);
    require_matches(index, edges);
  }

  REQUIRE_EQ(index.bridge_count(), 2U);
  REQUIRE(index.same_two_edge_component(0U, 2U));
  REQUIRE(index.same_two_edge_component(3U, 5U));
  REQUIRE(!index.same_two_edge_component(2U, 3U));
  REQUIRE(index.connected(0U, 6U));

  // Closing a large cycle across both blocks eliminates both bridges.
  REQUIRE_EQ(index.add_edge(6U, 0U), edges.size());
  edges.push_back({6U, 0U});
  REQUIRE_EQ(index.bridge_count(), 0U);
  for (Vertex vertex = 1U; vertex < 7U; ++vertex) {
    REQUIRE(index.same_two_edge_component(0U, vertex));
  }
  require_matches(index, edges);
}

TEST_CASE(online_bridge_connectivity_rejects_invalid_vertices_without_mutation) {
  using namespace online_bridge_connectivity_test_detail;

  OnlineBridgeConnectivity empty(0U);
  REQUIRE(empty.valid_invariants());
  REQUIRE_THROWS_AS(empty.add_edge(0U, 0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.connected(0U, 0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.same_two_edge_component(0U, 0U),
                    std::out_of_range);

  OnlineBridgeConnectivity index(3U);
  std::vector<OracleEdge> edges;
  REQUIRE_EQ(index.add_edge(0U, 1U), 0U);
  edges.push_back({0U, 1U});

  const std::size_t before_edges = index.edge_count();
  const std::size_t before_bridges = index.bridge_count();
  const std::size_t before_components = index.component_count();
  REQUIRE_THROWS_AS(index.add_edge(3U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(index.add_edge(1U, 3U), std::out_of_range);
  REQUIRE_EQ(index.edge_count(), before_edges);
  REQUIRE_EQ(index.bridge_count(), before_bridges);
  REQUIRE_EQ(index.component_count(), before_components);
  require_matches(index, edges);
}

TEST_CASE(online_bridge_connectivity_randomized_matches_tarjan_multigraph_oracle) {
  using namespace online_bridge_connectivity_test_detail;

  std::mt19937_64 random(0xB71D63ULL);
  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    const std::size_t vertex_count =
        1U + static_cast<std::size_t>(random() % 16U);
    OnlineBridgeConnectivity index(vertex_count);
    std::vector<OracleEdge> edges;

    for (std::size_t step = 0U; step < 140U; ++step) {
      const Vertex first = static_cast<Vertex>(random() % vertex_count);
      const Vertex second = static_cast<Vertex>(random() % vertex_count);

      REQUIRE_EQ(index.add_edge(first, second), edges.size());
      edges.push_back({first, second});

      // Recompute from the complete undirected multigraph after every update.
      require_matches(index, edges);
    }
  }
}
