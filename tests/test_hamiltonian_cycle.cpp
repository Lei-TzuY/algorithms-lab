#include "algorithms/graphs/hamiltonian_cycle.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::HamiltonianCycleResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;
using algorithms::graphs::minimum_directed_hamiltonian_cycle;

std::optional<Weight> cheapest_arc(const Graph& graph, Vertex from, Vertex to) {
  std::optional<Weight> best;
  for (const auto& edge : graph.neighbors(from)) {
    if (edge.to == to && (!best.has_value() || edge.weight < *best)) {
      best = edge.weight;
    }
  }
  return best;
}

std::optional<Weight> brute_force_cost(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n <= 1U) {
    return Weight{0};
  }
  std::vector<Vertex> middle;
  for (Vertex vertex = 1U; vertex < n; ++vertex) {
    middle.push_back(vertex);
  }
  std::optional<Weight> best;
  do {
    Vertex from = 0U;
    Weight total = 0;
    bool valid = true;
    for (Vertex to : middle) {
      const auto edge = cheapest_arc(graph, from, to);
      if (!edge.has_value()) {
        valid = false;
        break;
      }
      total += *edge;
      from = to;
    }
    if (valid) {
      const auto closing = cheapest_arc(graph, from, 0U);
      if (!closing.has_value()) {
        valid = false;
      } else {
        total += *closing;
      }
    }
    if (valid && (!best.has_value() || total < *best)) {
      best = total;
    }
  } while (std::next_permutation(middle.begin(), middle.end()));
  return best;
}

void replay_result(const Graph& graph, const HamiltonianCycleResult& result) {
  const std::size_t n = graph.vertex_count();
  if (n == 0U) {
    REQUIRE(result.cycle_vertices.empty());
    REQUIRE(result.edges.empty());
    REQUIRE_EQ(result.total_weight, 0);
    return;
  }
  if (n == 1U) {
    REQUIRE(result.cycle_vertices == std::vector<Vertex>{0U});
    REQUIRE(result.edges.empty());
    REQUIRE_EQ(result.total_weight, 0);
    return;
  }

  REQUIRE_EQ(result.cycle_vertices.size(), n + 1U);
  REQUIRE_EQ(result.edges.size(), n);
  REQUIRE_EQ(result.cycle_vertices.front(), 0U);
  REQUIRE_EQ(result.cycle_vertices.back(), 0U);
  std::vector<bool> seen(n, false);
  for (std::size_t index = 0; index < n; ++index) {
    const Vertex vertex = result.cycle_vertices[index];
    REQUIRE(vertex < n);
    REQUIRE(!seen[vertex]);
    seen[vertex] = true;
  }

  Weight sum = 0;
  for (std::size_t index = 0; index < result.edges.size(); ++index) {
    const auto& witness = result.edges[index];
    REQUIRE_EQ(witness.from, result.cycle_vertices[index]);
    REQUIRE_EQ(witness.to, result.cycle_vertices[index + 1U]);
    const auto& neighbors = graph.neighbors(witness.from);
    REQUIRE(witness.adjacency_index < neighbors.size());
    REQUIRE_EQ(neighbors[witness.adjacency_index].to, witness.to);
    REQUIRE_EQ(neighbors[witness.adjacency_index].weight, witness.weight);
    sum += witness.weight;
  }
  REQUIRE_EQ(sum, result.total_weight);
}

TEST_CASE(hamiltonian_cycle_trivial_validation_and_state_bound) {
  Graph empty(0, true);
  const auto empty_result = minimum_directed_hamiltonian_cycle(empty);
  REQUIRE(empty_result.has_value());
  replay_result(empty, *empty_result);

  Graph singleton(1, true);
  singleton.add_edge(0, 0, -99);
  const auto one = minimum_directed_hamiltonian_cycle(singleton);
  REQUIRE(one.has_value());
  replay_result(singleton, *one);

  Graph undirected(3, false);
  REQUIRE_THROWS_AS(minimum_directed_hamiltonian_cycle(undirected),
                    std::invalid_argument);

  Graph too_large(17, true);
  REQUIRE_THROWS_AS(minimum_directed_hamiltonian_cycle(too_large),
                    std::length_error);
}

TEST_CASE(hamiltonian_cycle_known_parallel_negative_and_deterministic) {
  Graph graph(4, true);
  constexpr Weight matrix[4][4] = {{0, 10, 15, 20},
                                    {10, 0, 35, 25},
                                    {15, 35, 0, 30},
                                    {20, 25, 30, 0}};
  for (Vertex from = 0; from < 4U; ++from) {
    for (Vertex to = 0; to < 4U; ++to) {
      if (from != to) {
        graph.add_edge(from, to, matrix[from][to]);
      }
    }
  }
  const auto result = minimum_directed_hamiltonian_cycle(graph);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->total_weight, 80);
  replay_result(graph, *result);

  Graph parallel(3, true);
  parallel.add_edge(0, 0, -1000);
  parallel.add_edge(0, 1, 9);
  parallel.add_edge(0, 1, -5);
  parallel.add_edge(0, 1, -5);
  parallel.add_edge(1, 2, -7);
  parallel.add_edge(2, 0, 4);
  parallel.add_edge(0, 2, 100);
  parallel.add_edge(2, 1, 100);
  parallel.add_edge(1, 0, 100);

  const auto first = minimum_directed_hamiltonian_cycle(parallel);
  REQUIRE(first.has_value());
  REQUIRE_EQ(first->total_weight, -8);
  replay_result(parallel, *first);
  REQUIRE_EQ(first->edges.front().adjacency_index, 2U);

  const auto second = minimum_directed_hamiltonian_cycle(parallel);
  REQUIRE(second.has_value());
  REQUIRE(second->cycle_vertices == first->cycle_vertices);
  REQUIRE(second->edges == first->edges);
}

TEST_CASE(hamiltonian_cycle_no_tour_and_exact_wide_arithmetic) {
  Graph missing(3, true);
  missing.add_edge(0, 1, 1);
  missing.add_edge(1, 2, 1);
  REQUIRE(!minimum_directed_hamiltonian_cycle(missing).has_value());

  Graph exact_min(2, true);
  exact_min.add_edge(0, 1, std::numeric_limits<Weight>::min());
  exact_min.add_edge(1, 0, 0);
  const auto minimum = minimum_directed_hamiltonian_cycle(exact_min);
  REQUIRE(minimum.has_value());
  REQUIRE_EQ(minimum->total_weight, std::numeric_limits<Weight>::min());

  Graph overflow(2, true);
  overflow.add_edge(0, 1, std::numeric_limits<Weight>::max());
  overflow.add_edge(1, 0, 1);
  REQUIRE_THROWS_AS(minimum_directed_hamiltonian_cycle(overflow),
                    std::overflow_error);

  Graph alternative(3, true);
  alternative.add_edge(0, 1, std::numeric_limits<Weight>::max());
  alternative.add_edge(1, 2, std::numeric_limits<Weight>::max());
  alternative.add_edge(2, 0, std::numeric_limits<Weight>::max());
  alternative.add_edge(0, 2, 1);
  alternative.add_edge(2, 1, 1);
  alternative.add_edge(1, 0, 1);
  const auto representable = minimum_directed_hamiltonian_cycle(alternative);
  REQUIRE(representable.has_value());
  REQUIRE_EQ(representable->total_weight, 3);
  replay_result(alternative, *representable);
}

TEST_CASE(hamiltonian_cycle_randomized_exhaustive_permutation_differential) {
  std::mt19937_64 rng(0x48A11D0B5ULL);
  std::uniform_int_distribution<int> vertex_count(0, 8);
  std::uniform_int_distribution<int> chance(0, 99);
  std::uniform_int_distribution<int> weight(-10, 20);

  for (int trial = 0; trial < 450; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count(rng));
    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      if (chance(rng) < 20) {
        graph.add_edge(from, from, static_cast<Weight>(weight(rng)));
      }
      for (Vertex to = 0; to < n; ++to) {
        if (from == to) {
          continue;
        }
        if (chance(rng) < 58) {
          graph.add_edge(from, to, static_cast<Weight>(weight(rng)));
          if (chance(rng) < 18) {
            graph.add_edge(from, to, static_cast<Weight>(weight(rng)));
          }
        }
      }
    }

    const auto expected = brute_force_cost(graph);
    const auto actual = minimum_directed_hamiltonian_cycle(graph);
    REQUIRE_EQ(actual.has_value(), expected.has_value());
    if (actual.has_value()) {
      REQUIRE_EQ(actual->total_weight, *expected);
      replay_result(graph, *actual);
    }
  }
}

}  // namespace
