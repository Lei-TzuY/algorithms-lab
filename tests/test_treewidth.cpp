#include "algorithms/graphs/treewidth.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::ExactTreewidthResult;
using algorithms::graphs::Graph;
using algorithms::graphs::TreewidthEliminationStep;
using algorithms::graphs::Vertex;

std::vector<std::vector<bool>> simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<bool>> adjacent(n, std::vector<bool>(n, false));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from != edge.to) {
        adjacent[from][edge.to] = true;
        adjacent[edge.to][from] = true;
      }
    }
  }
  return adjacent;
}

struct ReplayResult {
  std::size_t width{};
  std::vector<TreewidthEliminationStep> steps;
};

ReplayResult replay_elimination(const Graph& graph,
                                const std::vector<Vertex>& order) {
  const std::size_t n = graph.vertex_count();
  REQUIRE_EQ(order.size(), n);
  auto adjacent = simple_adjacency(graph);
  std::vector<bool> present(n, true);
  std::vector<bool> seen(n, false);
  ReplayResult replay;

  for (const Vertex vertex : order) {
    REQUIRE(vertex < n);
    REQUIRE(present[vertex]);
    REQUIRE(!seen[vertex]);
    seen[vertex] = true;

    std::vector<Vertex> neighbors;
    for (Vertex other = 0; other < n; ++other) {
      if (present[other] && other != vertex && adjacent[vertex][other]) {
        neighbors.push_back(other);
      }
    }
    replay.width = std::max(replay.width, neighbors.size());

    TreewidthEliminationStep step;
    step.vertex = vertex;
    step.bag.push_back(vertex);
    step.bag.insert(step.bag.end(), neighbors.begin(), neighbors.end());
    replay.steps.push_back(std::move(step));

    for (std::size_t first = 0; first < neighbors.size(); ++first) {
      for (std::size_t second = first + 1U; second < neighbors.size(); ++second) {
        const Vertex a = neighbors[first];
        const Vertex b = neighbors[second];
        adjacent[a][b] = true;
        adjacent[b][a] = true;
      }
    }
    present[vertex] = false;
    for (Vertex other = 0; other < n; ++other) {
      adjacent[vertex][other] = false;
      adjacent[other][vertex] = false;
    }
  }
  return replay;
}

ExactTreewidthResult exhaustive_treewidth(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  ExactTreewidthResult best;
  best.width = n + 1U;
  std::vector<Vertex> order(n);
  std::iota(order.begin(), order.end(), Vertex{0});

  if (n == 0U) {
    best.width = 0U;
    return best;
  }

  do {
    const ReplayResult replay = replay_elimination(graph, order);
    if (replay.width < best.width) {
      best.width = replay.width;
      best.elimination_order = order;
      best.steps = replay.steps;
    }
  } while (std::next_permutation(order.begin(), order.end()));
  return best;
}

void require_witness_replays(const Graph& graph,
                             const ExactTreewidthResult& actual) {
  const ReplayResult replay = replay_elimination(graph, actual.elimination_order);
  REQUIRE_EQ(replay.width, actual.width);
  REQUIRE_EQ(replay.steps, actual.steps);
  for (const auto& step : actual.steps) {
    REQUIRE(!step.bag.empty());
    REQUIRE_EQ(step.bag.front(), step.vertex);
    REQUIRE(step.bag.size() - 1U <= actual.width);
    REQUIRE(std::is_sorted(step.bag.begin() + 1, step.bag.end()));
  }
}

Graph path_graph(const std::size_t n) {
  Graph graph(n, false);
  for (Vertex vertex = 1; vertex < n; ++vertex) {
    graph.add_edge(vertex - 1U, vertex);
  }
  return graph;
}

Graph clique_graph(const std::size_t n) {
  Graph graph(n, false);
  for (Vertex first = 0; first < n; ++first) {
    for (Vertex second = first + 1U; second < n; ++second) {
      graph.add_edge(first, second);
    }
  }
  return graph;
}

}  // namespace

TEST_CASE(treewidth_known_families_and_edge_semantics) {
  {
    const Graph graph(0, false);
    const auto result = algorithms::graphs::exact_treewidth(graph);
    REQUIRE_EQ(result.width, 0U);
    REQUIRE(result.elimination_order.empty());
    REQUIRE(result.steps.empty());
  }
  {
    const Graph graph(1, false);
    const auto result = algorithms::graphs::exact_treewidth(graph);
    REQUIRE_EQ(result.width, 0U);
    REQUIRE_EQ(result.elimination_order, std::vector<Vertex>({0U}));
    require_witness_replays(graph, result);
  }
  {
    const Graph graph = path_graph(7U);
    const auto result = algorithms::graphs::exact_treewidth(graph);
    REQUIRE_EQ(result.width, 1U);
    require_witness_replays(graph, result);
  }
  {
    const Graph graph = clique_graph(5U);
    const auto result = algorithms::graphs::exact_treewidth(graph);
    REQUIRE_EQ(result.width, 4U);
    REQUIRE_EQ(result.elimination_order,
               std::vector<Vertex>({0U, 1U, 2U, 3U, 4U}));
    require_witness_replays(graph, result);
  }
  {
    Graph cycle(5U, false);
    for (Vertex vertex = 0; vertex < 5U; ++vertex) {
      cycle.add_edge(vertex, (vertex + 1U) % 5U);
    }
    const auto result = algorithms::graphs::exact_treewidth(cycle);
    REQUIRE_EQ(result.width, 2U);
    require_witness_replays(cycle, result);
  }
  {
    Graph multi(4U, false);
    multi.add_edge(0U, 1U, 9);
    multi.add_edge(0U, 1U, -7);
    multi.add_edge(1U, 2U, 3);
    multi.add_edge(2U, 3U, 5);
    multi.add_edge(2U, 2U, -999);
    const auto result = algorithms::graphs::exact_treewidth(multi);
    REQUIRE_EQ(result.width, 1U);
    require_witness_replays(multi, result);
  }
}

TEST_CASE(treewidth_rejects_directed_and_over_limit_inputs) {
  Graph directed(3U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_treewidth(directed),
                    std::invalid_argument);

  Graph too_large(algorithms::graphs::kExactTreewidthMaxVertices + 1U, false);
  REQUIRE_THROWS_AS(algorithms::graphs::exact_treewidth(too_large),
                    std::length_error);
}

TEST_CASE(treewidth_fill_in_witness_is_replayable) {
  Graph graph(6U, false);
  graph.add_edge(0U, 1U);
  graph.add_edge(1U, 2U);
  graph.add_edge(2U, 3U);
  graph.add_edge(3U, 0U);
  graph.add_edge(1U, 4U);
  graph.add_edge(3U, 5U);
  graph.add_edge(4U, 5U);

  const auto result = algorithms::graphs::exact_treewidth(graph);
  const auto oracle = exhaustive_treewidth(graph);
  REQUIRE_EQ(result.width, oracle.width);
  REQUIRE_EQ(result.elimination_order, oracle.elimination_order);
  require_witness_replays(graph, result);
}

TEST_CASE(treewidth_randomized_differential_against_exhaustive_orders) {
  std::mt19937_64 rng(0x7A33D71DULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 7);
  std::bernoulli_distribution edge_present(0.38);
  std::bernoulli_distribution duplicate_edge(0.12);
  std::bernoulli_distribution self_loop(0.10);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);

  for (std::size_t trial = 0; trial < 160U; ++trial) {
    const std::size_t n =
        static_cast<std::size_t>(vertex_count_distribution(rng));
    Graph graph(n, false);
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      if (self_loop(rng)) {
        graph.add_edge(vertex, vertex, weight_distribution(rng));
      }
      for (Vertex other = vertex + 1U; other < n; ++other) {
        if (!edge_present(rng)) {
          continue;
        }
        graph.add_edge(vertex, other, weight_distribution(rng));
        if (duplicate_edge(rng)) {
          graph.add_edge(vertex, other, weight_distribution(rng));
        }
      }
    }

    const ExactTreewidthResult expected = exhaustive_treewidth(graph);
    const ExactTreewidthResult actual = algorithms::graphs::exact_treewidth(graph);
    REQUIRE_EQ(actual.width, expected.width);
    REQUIRE_EQ(actual.elimination_order, expected.elimination_order);
    require_witness_replays(graph, actual);
  }
}
