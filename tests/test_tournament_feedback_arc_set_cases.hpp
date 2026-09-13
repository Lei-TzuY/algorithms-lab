#pragma once

#include "algorithms/graphs/tournament_feedback_arc_set.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tournament_feedback_arc_cases {

using algorithms::graphs::Graph;
using algorithms::graphs::TournamentFeedbackArcSetResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::minimum_feedback_arc_set_tournament;

inline Graph make_tournament(const std::vector<std::vector<bool>>& arc,
                             bool weighted = false) {
  Graph graph(arc.size(), true);
  for (std::size_t u = 0; u < arc.size(); ++u) {
    for (std::size_t v = 0; v < arc.size(); ++v) {
      if (arc[u][v]) {
        const auto weight = weighted
                                ? static_cast<std::int64_t>((u + 1) * 17) -
                                      static_cast<std::int64_t>((v + 3) * 11)
                                : 1;
        graph.add_edge(u, v, weight);
      }
    }
  }
  return graph;
}

inline std::size_t ordering_cost(
    const std::vector<std::vector<bool>>& arc,
    const std::vector<Vertex>& order) {
  std::vector<std::size_t> position(order.size());
  for (std::size_t index = 0; index < order.size(); ++index) {
    position[order[index]] = index;
  }
  std::size_t cost = 0;
  for (std::size_t u = 0; u < order.size(); ++u) {
    for (std::size_t v = 0; v < order.size(); ++v) {
      if (arc[u][v] && position[u] > position[v]) {
        ++cost;
      }
    }
  }
  return cost;
}

inline std::pair<std::size_t, std::vector<Vertex>> exhaustive(
    const std::vector<std::vector<bool>>& arc) {
  std::vector<Vertex> permutation(arc.size());
  std::iota(permutation.begin(), permutation.end(), Vertex{0});
  std::size_t best = std::numeric_limits<std::size_t>::max();
  std::vector<Vertex> best_ordering;
  do {
    const std::size_t cost = ordering_cost(arc, permutation);
    if (cost < best) {
      best = cost;
      best_ordering = permutation;
    }
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return {best, best_ordering};
}

inline void verify(const std::vector<std::vector<bool>>& arc,
                   const TournamentFeedbackArcSetResult& result) {
  REQUIRE_EQ(result.ordering.size(), arc.size());
  auto sorted = result.ordering;
  std::sort(sorted.begin(), sorted.end());
  for (std::size_t index = 0; index < sorted.size(); ++index) {
    REQUIRE_EQ(sorted[index], index);
  }
  REQUIRE_EQ(ordering_cost(arc, result.ordering), result.feedback_arc_count);

  std::vector<std::size_t> position(arc.size());
  for (std::size_t index = 0; index < result.ordering.size(); ++index) {
    position[result.ordering[index]] = index;
  }
  for (const auto& [from, to] : result.feedback_arcs) {
    REQUIRE(arc[from][to]);
    REQUIRE(position[from] > position[to]);
  }
  REQUIRE_EQ(result.feedback_arcs.size(), result.feedback_arc_count);
}

TEST_CASE(tournament_fas_deterministic_examples_and_weights) {
  {
    Graph graph(0, true);
    const auto result = minimum_feedback_arc_set_tournament(graph);
    REQUIRE_EQ(result.feedback_arc_count, 0U);
    REQUIRE(result.ordering.empty());
  }
  {
    std::vector<std::vector<bool>> arc(4, std::vector<bool>(4, false));
    for (std::size_t u = 0; u < 4; ++u) {
      for (std::size_t v = u + 1; v < 4; ++v) {
        arc[u][v] = true;
      }
    }
    const auto result =
        minimum_feedback_arc_set_tournament(make_tournament(arc, true));
    verify(arc, result);
    REQUIRE_EQ(result.feedback_arc_count, 0U);
    REQUIRE_EQ(result.ordering, (std::vector<Vertex>{0, 1, 2, 3}));
  }
  {
    std::vector<std::vector<bool>> arc(3, std::vector<bool>(3, false));
    arc[0][1] = true;
    arc[1][2] = true;
    arc[2][0] = true;
    const auto result = minimum_feedback_arc_set_tournament(make_tournament(arc));
    verify(arc, result);
    REQUIRE_EQ(result.feedback_arc_count, 1U);
    REQUIRE_EQ(result.ordering, (std::vector<Vertex>{0, 1, 2}));
  }
}

TEST_CASE(tournament_fas_rejects_non_tournaments) {
  Graph undirected(3, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(undirected),
                    std::invalid_argument);

  Graph loop(2, true);
  loop.add_edge(0, 0);
  loop.add_edge(0, 1);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(loop),
                    std::invalid_argument);

  Graph missing(3, true);
  missing.add_edge(0, 1);
  missing.add_edge(1, 2);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(missing),
                    std::invalid_argument);

  Graph both(2, true);
  both.add_edge(0, 1);
  both.add_edge(1, 0);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(both),
                    std::invalid_argument);

  Graph parallel(2, true);
  parallel.add_edge(0, 1);
  parallel.add_edge(0, 1);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(parallel),
                    std::invalid_argument);

  Graph too_large(21, true);
  REQUIRE_THROWS_AS(minimum_feedback_arc_set_tournament(too_large),
                    std::length_error);
}

TEST_CASE(tournament_fas_exhaustive_random_differential) {
  std::mt19937_64 rng(0xFA57A2CULL);
  for (std::size_t trial = 0; trial < 280; ++trial) {
    const std::size_t n = 1 + static_cast<std::size_t>(rng() % 8U);
    std::vector<std::vector<bool>> arc(n, std::vector<bool>(n, false));
    for (std::size_t u = 0; u < n; ++u) {
      for (std::size_t v = u + 1; v < n; ++v) {
        if ((rng() & 1U) != 0U) {
          arc[u][v] = true;
        } else {
          arc[v][u] = true;
        }
      }
    }
    const auto oracle = exhaustive(arc);
    const auto actual =
        minimum_feedback_arc_set_tournament(make_tournament(arc));
    verify(arc, actual);
    REQUIRE_EQ(actual.feedback_arc_count, oracle.first);
    REQUIRE_EQ(actual.ordering, oracle.second);
  }
}

TEST_CASE(tournament_fas_replay_determinism) {
  constexpr std::size_t n = 9;
  std::mt19937_64 rng(1234567);
  std::vector<std::vector<bool>> arc(n, std::vector<bool>(n, false));
  for (std::size_t u = 0; u < n; ++u) {
    for (std::size_t v = u + 1; v < n; ++v) {
      if ((rng() & 1U) != 0U) {
        arc[u][v] = true;
      } else {
        arc[v][u] = true;
      }
    }
  }
  const Graph graph = make_tournament(arc);
  const auto first = minimum_feedback_arc_set_tournament(graph);
  const auto second = minimum_feedback_arc_set_tournament(graph);
  REQUIRE_EQ(first.feedback_arc_count, second.feedback_arc_count);
  REQUIRE_EQ(first.ordering, second.ordering);
  REQUIRE_EQ(first.feedback_arcs, second.feedback_arcs);
}

}  // namespace tournament_feedback_arc_cases
