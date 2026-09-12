#include "algorithms/graphs/transitive_reduction.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::DirectedArc;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

std::vector<DirectedArc> unique_arcs(const Graph& graph) {
  std::vector<DirectedArc> arcs;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      arcs.push_back({from, edge.to});
    }
  }
  std::sort(arcs.begin(), arcs.end(), [](const DirectedArc& a, const DirectedArc& b) {
    return a.from < b.from || (a.from == b.from && a.to < b.to);
  });
  arcs.erase(std::unique(arcs.begin(), arcs.end()), arcs.end());
  return arcs;
}

std::vector<std::vector<unsigned char>> closure(
    std::size_t n, const std::vector<DirectedArc>& arcs) {
  std::vector<std::vector<Vertex>> adjacency(n);
  for (const auto& arc : arcs) {
    adjacency[arc.from].push_back(arc.to);
  }
  std::vector<std::vector<unsigned char>> result(
      n, std::vector<unsigned char>(n, 0));
  for (Vertex source = 0; source < n; ++source) {
    std::deque<Vertex> queue;
    for (Vertex to : adjacency[source]) {
      if (result[source][to] == 0U) {
        result[source][to] = 1U;
        queue.push_back(to);
      }
    }
    while (!queue.empty()) {
      const Vertex from = queue.front();
      queue.pop_front();
      for (Vertex to : adjacency[from]) {
        if (result[source][to] == 0U) {
          result[source][to] = 1U;
          queue.push_back(to);
        }
      }
    }
  }
  return result;
}

std::vector<DirectedArc> exhaustive_reduction(const Graph& graph) {
  const auto arcs = unique_arcs(graph);
  const auto target = closure(graph.vertex_count(), arcs);
  if (arcs.size() >= std::numeric_limits<std::uint64_t>::digits) {
    throw std::runtime_error("oracle instance too large");
  }

  std::vector<DirectedArc> best;
  std::size_t best_size = arcs.size() + 1U;
  const std::uint64_t limit = std::uint64_t{1} << arcs.size();
  const auto arc_less = [](const DirectedArc& a, const DirectedArc& b) {
    return a.from < b.from || (a.from == b.from && a.to < b.to);
  };

  for (std::uint64_t mask = 0; mask < limit; ++mask) {
    const std::size_t count = static_cast<std::size_t>(std::popcount(mask));
    if (count > best_size) {
      continue;
    }
    std::vector<DirectedArc> candidate;
    candidate.reserve(count);
    for (std::size_t index = 0; index < arcs.size(); ++index) {
      if ((mask & (std::uint64_t{1} << index)) != 0U) {
        candidate.push_back(arcs[index]);
      }
    }
    if (closure(graph.vertex_count(), candidate) != target) {
      continue;
    }
    const bool lexicographically_smaller = std::lexicographical_compare(
        candidate.begin(), candidate.end(), best.begin(), best.end(), arc_less);
    if (count < best_size ||
        (count == best_size && lexicographically_smaller)) {
      best_size = count;
      best = std::move(candidate);
    }
  }
  return best;
}

bool valid_topological_order(const Graph& graph,
                             const std::vector<Vertex>& order) {
  if (order.size() != graph.vertex_count()) {
    return false;
  }
  std::vector<std::size_t> position(order.size(), 0);
  std::vector<unsigned char> seen(order.size(), 0);
  for (std::size_t index = 0; index < order.size(); ++index) {
    if (order[index] >= order.size() || seen[order[index]] != 0U) {
      return false;
    }
    seen[order[index]] = 1U;
    position[order[index]] = index;
  }
  for (const auto& arc : unique_arcs(graph)) {
    if (position[arc.from] >= position[arc.to]) {
      return false;
    }
  }
  return true;
}

TEST_CASE(transitive_reduction_deterministic_shapes) {
  Graph graph(6, true);
  graph.add_edge(0, 1, 9);
  graph.add_edge(0, 2, -4);
  graph.add_edge(1, 3, 1);
  graph.add_edge(2, 3, 99);
  graph.add_edge(0, 3, 7);
  graph.add_edge(3, 4, 2);
  graph.add_edge(0, 4, 3);
  graph.add_edge(4, 5, 5);
  graph.add_edge(0, 1, -100);

  const auto result = algorithms::graphs::transitive_reduction(graph);
  const std::vector<DirectedArc> expected{
      {0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 5}};
  REQUIRE_EQ(result.arcs, expected);
  REQUIRE(valid_topological_order(graph, result.topological_order));
  REQUIRE_EQ(closure(graph.vertex_count(), result.arcs),
             closure(graph.vertex_count(), unique_arcs(graph)));
}

TEST_CASE(transitive_reduction_empty_singleton_disconnected) {
  Graph empty(0, true);
  REQUIRE(algorithms::graphs::transitive_reduction(empty).arcs.empty());

  Graph singleton(1, true);
  const auto one = algorithms::graphs::transitive_reduction(singleton);
  REQUIRE(one.arcs.empty());
  REQUIRE_EQ(one.topological_order, std::vector<Vertex>({0}));

  Graph disconnected(5, true);
  disconnected.add_edge(0, 1);
  disconnected.add_edge(0, 2);
  disconnected.add_edge(1, 2);
  disconnected.add_edge(3, 4);
  const auto result = algorithms::graphs::transitive_reduction(disconnected);
  const std::vector<DirectedArc> expected{{0, 1}, {1, 2}, {3, 4}};
  REQUIRE_EQ(result.arcs, expected);
}

TEST_CASE(transitive_reduction_rejects_non_dags) {
  Graph undirected(2, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(algorithms::graphs::transitive_reduction(undirected),
                    std::invalid_argument);

  Graph self_loop(1, true);
  self_loop.add_edge(0, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::transitive_reduction(self_loop),
                    std::invalid_argument);

  Graph cycle(3, true);
  cycle.add_edge(0, 1);
  cycle.add_edge(1, 2);
  cycle.add_edge(2, 0);
  REQUIRE_THROWS_AS(algorithms::graphs::transitive_reduction(cycle),
                    std::invalid_argument);
}

TEST_CASE(transitive_reduction_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0x7A4E51D1ULL);
  std::uniform_int_distribution<int> n_dist(0, 6);
  std::uniform_int_distribution<int> coin(0, 99);
  std::uniform_int_distribution<int> weight(-50, 50);

  for (int trial = 0; trial < 350; ++trial) {
    const std::size_t n = static_cast<std::size_t>(n_dist(rng));
    std::vector<Vertex> order(n);
    std::iota(order.begin(), order.end(), Vertex{0});
    std::shuffle(order.begin(), order.end(), rng);
    Graph graph(n, true);
    std::size_t unique_count = 0;
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) {
        if (coin(rng) < 42 && unique_count < 11U) {
          graph.add_edge(order[i], order[j], weight(rng));
          ++unique_count;
          if (coin(rng) < 20) {
            graph.add_edge(order[i], order[j], weight(rng));
          }
        }
      }
    }

    const auto actual = algorithms::graphs::transitive_reduction(graph);
    const auto expected = exhaustive_reduction(graph);
    REQUIRE_EQ(actual.arcs, expected);
    REQUIRE(valid_topological_order(graph, actual.topological_order));
    REQUIRE_EQ(closure(n, actual.arcs), closure(n, unique_arcs(graph)));

    for (std::size_t index = 0; index < actual.arcs.size(); ++index) {
      auto missing = actual.arcs;
      missing.erase(missing.begin() + static_cast<std::ptrdiff_t>(index));
      REQUIRE(closure(n, missing) != closure(n, actual.arcs));
    }
  }
}

}  // namespace
