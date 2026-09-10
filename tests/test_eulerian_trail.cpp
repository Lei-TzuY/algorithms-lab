#include "algorithms/graphs/eulerian_trail.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>

#include "test_framework.hpp"

namespace {

using algorithms::graphs::EulerianTrailResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::eulerian_trail;

struct OracleEdge {
  Vertex from;
  Vertex to;
};

bool exhaustive_eulerian_exists(std::size_t vertex_count, bool directed,
                                const std::vector<OracleEdge>& edges) {
  if (edges.empty()) {
    return true;
  }
  if (vertex_count == 0 || edges.size() >= 63) {
    return false;
  }

  const std::uint64_t all_mask =
      (std::uint64_t{1} << static_cast<unsigned int>(edges.size())) - 1;
  std::vector<std::vector<signed char>> memo(
      vertex_count,
      std::vector<signed char>(static_cast<std::size_t>(all_mask + 1), -1));

  std::function<bool(Vertex, std::uint64_t)> search =
      [&](Vertex current, std::uint64_t used_mask) -> bool {
    if (used_mask == all_mask) {
      return true;
    }
    signed char& cached =
        memo[current][static_cast<std::size_t>(used_mask)];
    if (cached >= 0) {
      return cached != 0;
    }

    for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
      const std::uint64_t bit =
          std::uint64_t{1} << static_cast<unsigned int>(edge_id);
      if ((used_mask & bit) != 0) {
        continue;
      }
      const OracleEdge edge = edges[edge_id];
      std::optional<Vertex> next;
      if (directed) {
        if (edge.from == current) {
          next = edge.to;
        }
      } else if (edge.from == current) {
        next = edge.to;
      } else if (edge.to == current) {
        next = edge.from;
      }
      if (next.has_value() && search(*next, used_mask | bit)) {
        cached = 1;
        return true;
      }
    }

    cached = 0;
    return false;
  };

  for (Vertex start = 0; start < vertex_count; ++start) {
    if (search(start, 0)) {
      return true;
    }
  }
  return false;
}

bool edge_matches(const OracleEdge& edge, bool directed, Vertex from,
                  Vertex to) {
  if (directed) {
    return edge.from == from && edge.to == to;
  }
  return (edge.from == from && edge.to == to) ||
         (edge.from == to && edge.to == from);
}

void require_replayable(const EulerianTrailResult& result,
                        std::size_t vertex_count, bool directed,
                        const std::vector<OracleEdge>& edges) {
  REQUIRE_EQ(result.edge_count, edges.size());
  if (edges.empty()) {
    REQUIRE_EQ(result.vertices.size(), vertex_count == 0 ? 0U : 1U);
    REQUIRE(result.is_circuit);
    return;
  }

  REQUIRE_EQ(result.vertices.size(), edges.size() + 1);
  std::vector<bool> used(edges.size(), false);
  for (const Vertex vertex : result.vertices) {
    REQUIRE(vertex < vertex_count);
  }
  for (std::size_t step = 0; step < edges.size(); ++step) {
    const Vertex from = result.vertices[step];
    const Vertex to = result.vertices[step + 1];
    bool matched = false;
    for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
      if (!used[edge_id] && edge_matches(edges[edge_id], directed, from, to)) {
        used[edge_id] = true;
        matched = true;
        break;
      }
    }
    REQUIRE(matched);
  }
  REQUIRE(std::all_of(used.begin(), used.end(), [](bool value) { return value; }));
  REQUIRE_EQ(result.is_circuit,
             result.vertices.front() == result.vertices.back());
}

TEST_CASE(eulerian_trivial_and_undirected_deterministic_cases) {
  Graph empty(0, false);
  const auto empty_result = eulerian_trail(empty);
  REQUIRE(empty_result.has_value());
  REQUIRE(empty_result->vertices.empty());
  REQUIRE_EQ(empty_result->edge_count, 0U);
  REQUIRE(empty_result->is_circuit);

  Graph edgeless(4, false);
  const auto edgeless_result = eulerian_trail(edgeless);
  REQUIRE(edgeless_result.has_value());
  REQUIRE_EQ(edgeless_result->vertices, std::vector<Vertex>({0}));
  REQUIRE(edgeless_result->is_circuit);

  Graph path(3, false);
  path.add_edge(0, 1, 7);
  path.add_edge(1, 2, -11);
  const auto path_result = eulerian_trail(path);
  REQUIRE(path_result.has_value());
  REQUIRE_EQ(path_result->vertices, std::vector<Vertex>({0, 1, 2}));
  REQUIRE(!path_result->is_circuit);

  Graph triangle(3, false);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  const auto triangle_result = eulerian_trail(triangle);
  REQUIRE(triangle_result.has_value());
  REQUIRE_EQ(triangle_result->vertices,
             std::vector<Vertex>({0, 1, 2, 0}));
  REQUIRE(triangle_result->is_circuit);
}

TEST_CASE(eulerian_multigraph_self_loop_and_weight_semantics) {
  Graph first(2, false);
  first.add_edge(0, 1, 7);
  first.add_edge(0, 1, -3);
  first.add_edge(1, 1, 99);

  Graph second(2, false);
  second.add_edge(0, 1, -1000);
  second.add_edge(0, 1, 42);
  second.add_edge(1, 1, -5);

  const auto first_result = eulerian_trail(first);
  const auto second_result = eulerian_trail(second);
  REQUIRE(first_result.has_value());
  REQUIRE(second_result.has_value());
  REQUIRE_EQ(*first_result, *second_result);
  require_replayable(*first_result, 2, false,
                     {{0, 1}, {0, 1}, {1, 1}});

  Graph four_odds(4, false);
  four_odds.add_edge(0, 1);
  four_odds.add_edge(0, 2);
  four_odds.add_edge(0, 3);
  REQUIRE(!eulerian_trail(four_odds).has_value());

  Graph disconnected(4, false);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(2, 3);
  disconnected.add_edge(2, 3);
  REQUIRE(!eulerian_trail(disconnected).has_value());
}

TEST_CASE(eulerian_directed_degree_and_weak_connectivity_cases) {
  Graph path(3, true);
  path.add_edge(0, 1, 3);
  path.add_edge(1, 2, -7);
  const auto path_result = eulerian_trail(path);
  REQUIRE(path_result.has_value());
  REQUIRE_EQ(path_result->vertices, std::vector<Vertex>({0, 1, 2}));
  REQUIRE(!path_result->is_circuit);

  Graph circuit(3, true);
  circuit.add_edge(0, 1);
  circuit.add_edge(1, 2);
  circuit.add_edge(2, 0);
  const auto circuit_result = eulerian_trail(circuit);
  REQUIRE(circuit_result.has_value());
  REQUIRE_EQ(circuit_result->vertices,
             std::vector<Vertex>({0, 1, 2, 0}));
  REQUIRE(circuit_result->is_circuit);

  Graph bad_imbalance(3, true);
  bad_imbalance.add_edge(0, 1);
  bad_imbalance.add_edge(0, 2);
  REQUIRE(!eulerian_trail(bad_imbalance).has_value());

  Graph disconnected(4, true);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(1, 0);
  disconnected.add_edge(2, 3);
  disconnected.add_edge(3, 2);
  REQUIRE(!eulerian_trail(disconnected).has_value());

  Graph loops(2, true);
  loops.add_edge(0, 0);
  loops.add_edge(0, 1);
  loops.add_edge(1, 0);
  const auto loops_result = eulerian_trail(loops);
  REQUIRE(loops_result.has_value());
  require_replayable(*loops_result, 2, true,
                     {{0, 0}, {0, 1}, {1, 0}});
}

TEST_CASE(eulerian_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xE017A11ULL);
  for (std::size_t trial = 0; trial < 1200; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(rng() % 7);
    const bool directed = (rng() & 1ULL) != 0;
    Graph graph(vertex_count, directed);
    std::vector<OracleEdge> edges;

    const std::size_t requested_edges =
        vertex_count == 0 ? 0 : static_cast<std::size_t>(rng() % 10);
    for (std::size_t index = 0; index < requested_edges; ++index) {
      const Vertex from = static_cast<Vertex>(rng() % vertex_count);
      const Vertex to = static_cast<Vertex>(rng() % vertex_count);
      const auto weight = static_cast<std::int64_t>(rng() % 101) - 50;
      graph.add_edge(from, to, weight);
      edges.push_back(OracleEdge{from, to});
    }

    const bool expected =
        exhaustive_eulerian_exists(vertex_count, directed, edges);
    const auto actual = eulerian_trail(graph);
    REQUIRE_EQ(actual.has_value(), expected);
    if (!actual.has_value()) {
      continue;
    }

    require_replayable(*actual, vertex_count, directed, edges);
    REQUIRE_EQ(eulerian_trail(graph), actual);
  }
}

}  // namespace
