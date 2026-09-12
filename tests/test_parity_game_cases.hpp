#pragma once

#include "algorithms/games/parity_game.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace parity_game_test_detail {

using algorithms::games::ParityPlayer;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

using Strategy = std::vector<Vertex>;

inline std::vector<std::vector<Vertex>> structural_choices(const Graph& graph) {
  std::vector<std::vector<Vertex>> choices(graph.vertex_count());
  for (std::size_t vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    for (const auto& edge : graph.neighbors(vertex)) {
      choices[vertex].push_back(edge.to);
    }
    std::sort(choices[vertex].begin(), choices[vertex].end());
    choices[vertex].erase(
        std::unique(choices[vertex].begin(), choices[vertex].end()),
        choices[vertex].end());
  }
  return choices;
}

inline void enumerate_strategy_rec(
    const std::vector<std::vector<Vertex>>& choices,
    std::span<const ParityPlayer> owner,
    ParityPlayer player,
    std::size_t vertex,
    Strategy& current,
    std::vector<Strategy>& output) {
  if (vertex == choices.size()) {
    output.push_back(current);
    return;
  }
  if (owner[vertex] != player) {
    enumerate_strategy_rec(choices, owner, player, vertex + 1, current, output);
    return;
  }
  for (const auto target : choices[vertex]) {
    current[vertex] = target;
    enumerate_strategy_rec(choices, owner, player, vertex + 1, current, output);
  }
}

inline std::vector<Strategy> enumerate_strategies(
    const std::vector<std::vector<Vertex>>& choices,
    std::span<const ParityPlayer> owner,
    ParityPlayer player) {
  Strategy current(choices.size(), choices.size());
  std::vector<Strategy> output;
  enumerate_strategy_rec(choices, owner, player, 0, current, output);
  return output;
}

inline ParityPlayer fixed_strategy_winner(
    Vertex start,
    std::span<const ParityPlayer> owner,
    std::span<const std::uint32_t> priority,
    const Strategy& even_strategy,
    const Strategy& odd_strategy) {
  const std::size_t n = owner.size();
  std::vector<std::size_t> first_seen(n, n);
  std::vector<Vertex> path;
  Vertex vertex = start;
  while (first_seen[vertex] == n) {
    first_seen[vertex] = path.size();
    path.push_back(vertex);
    const auto next = (owner[vertex] == ParityPlayer::even)
                          ? even_strategy[vertex]
                          : odd_strategy[vertex];
    if (next >= n) {
      throw std::logic_error("oracle strategy is incomplete");
    }
    vertex = next;
  }

  const std::size_t cycle_begin = first_seen[vertex];
  std::uint32_t highest = priority[path[cycle_begin]];
  for (std::size_t index = cycle_begin + 1; index < path.size(); ++index) {
    highest = std::max(highest, priority[path[index]]);
  }
  return (highest % 2U == 0U) ? ParityPlayer::even : ParityPlayer::odd;
}

inline std::vector<ParityPlayer> exhaustive_positional_oracle(
    const Graph& graph,
    std::span<const ParityPlayer> owner,
    std::span<const std::uint32_t> priority) {
  const auto choices = structural_choices(graph);
  const auto even_strategies =
      enumerate_strategies(choices, owner, ParityPlayer::even);
  const auto odd_strategies =
      enumerate_strategies(choices, owner, ParityPlayer::odd);

  std::vector<ParityPlayer> result(graph.vertex_count(), ParityPlayer::even);
  for (Vertex start = 0; start < graph.vertex_count(); ++start) {
    bool even_can_force = false;
    for (const auto& even_strategy : even_strategies) {
      bool wins_against_all = true;
      for (const auto& odd_strategy : odd_strategies) {
        if (fixed_strategy_winner(start, owner, priority, even_strategy,
                                  odd_strategy) != ParityPlayer::even) {
          wins_against_all = false;
          break;
        }
      }
      if (wins_against_all) {
        even_can_force = true;
        break;
      }
    }

    bool odd_can_force = false;
    for (const auto& odd_strategy : odd_strategies) {
      bool wins_against_all = true;
      for (const auto& even_strategy : even_strategies) {
        if (fixed_strategy_winner(start, owner, priority, even_strategy,
                                  odd_strategy) != ParityPlayer::odd) {
          wins_against_all = false;
          break;
        }
      }
      if (wins_against_all) {
        odd_can_force = true;
        break;
      }
    }

    REQUIRE(even_can_force != odd_can_force);
    result[start] = even_can_force ? ParityPlayer::even : ParityPlayer::odd;
  }
  return result;
}

}  // namespace parity_game_test_detail

TEST_CASE(parity_game_validation_and_empty_semantics) {
  using namespace parity_game_test_detail;
  const Graph empty(0, true);
  const std::vector<ParityPlayer> no_owner;
  const std::vector<std::uint32_t> no_priority;
  const auto empty_result = algorithms::games::solve_parity_game(
      empty, no_owner, no_priority);
  REQUIRE(empty_result.winner.empty());

  Graph undirected(1, false);
  undirected.add_edge(0, 0);
  const std::vector<ParityPlayer> owner{ParityPlayer::even};
  const std::vector<std::uint32_t> priority{0};
  REQUIRE_THROWS_AS(algorithms::games::solve_parity_game(
                        undirected, owner, priority),
                    std::invalid_argument);

  Graph sink(1, true);
  REQUIRE_THROWS_AS(algorithms::games::solve_parity_game(sink, owner, priority),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::games::solve_parity_game(
                        empty, owner, no_priority),
                    std::invalid_argument);

  Graph invalid_owner_graph(1, true);
  invalid_owner_graph.add_edge(0, 0);
  const std::vector<ParityPlayer> invalid_owner{
      static_cast<ParityPlayer>(99)};
  REQUIRE_THROWS_AS(algorithms::games::solve_parity_game(
                        invalid_owner_graph, invalid_owner, priority),
                    std::invalid_argument);
}

TEST_CASE(parity_game_singleton_cycles_follow_priority_parity) {
  using namespace parity_game_test_detail;
  for (std::uint32_t priority_value = 0; priority_value < 8; ++priority_value) {
    Graph graph(1, true);
    graph.add_edge(0, 0, -17);
    const std::vector<ParityPlayer> owner{ParityPlayer::odd};
    const std::vector<std::uint32_t> priority{priority_value};
    const auto result = algorithms::games::solve_parity_game(
        graph, owner, priority);
    const auto expected = (priority_value % 2U == 0U)
                              ? ParityPlayer::even
                              : ParityPlayer::odd;
    REQUIRE_EQ(result.winner, std::vector<ParityPlayer>{expected});
  }
}

TEST_CASE(parity_game_ownership_controls_attractor_choices) {
  using namespace parity_game_test_detail;
  Graph graph(4, true);
  graph.add_edge(0, 1, 100);
  graph.add_edge(0, 2, -100);
  graph.add_edge(0, 2, 7);
  graph.add_edge(1, 1);
  graph.add_edge(2, 2);
  graph.add_edge(3, 1);
  graph.add_edge(3, 2);
  const std::vector<ParityPlayer> owner{
      ParityPlayer::even, ParityPlayer::even, ParityPlayer::odd,
      ParityPlayer::odd};
  const std::vector<std::uint32_t> priority{0, 1, 2, 0};
  const auto result = algorithms::games::solve_parity_game(graph, owner, priority);
  REQUIRE_EQ(result.winner[0], ParityPlayer::even);
  REQUIRE_EQ(result.winner[1], ParityPlayer::odd);
  REQUIRE_EQ(result.winner[2], ParityPlayer::even);
  REQUIRE_EQ(result.winner[3], ParityPlayer::odd);

  const auto repeated = algorithms::games::solve_parity_game(graph, owner, priority);
  REQUIRE_EQ(repeated.winner, result.winner);
  REQUIRE(repeated.recursive_calls == result.recursive_calls);
}

TEST_CASE(parity_game_cycle_priority_and_disconnected_components) {
  using namespace parity_game_test_detail;
  Graph graph(5, true);
  graph.add_edge(0, 1);
  graph.add_edge(1, 0);
  graph.add_edge(2, 3);
  graph.add_edge(3, 4);
  graph.add_edge(4, 2);
  const std::vector<ParityPlayer> owner{
      ParityPlayer::even, ParityPlayer::odd, ParityPlayer::even,
      ParityPlayer::odd, ParityPlayer::even};
  const std::vector<std::uint32_t> priority{2, 5, 4, 3, 2};
  const auto result = algorithms::games::solve_parity_game(graph, owner, priority);
  REQUIRE_EQ(result.winner[0], ParityPlayer::odd);
  REQUIRE_EQ(result.winner[1], ParityPlayer::odd);
  REQUIRE_EQ(result.winner[2], ParityPlayer::even);
  REQUIRE_EQ(result.winner[3], ParityPlayer::even);
  REQUIRE_EQ(result.winner[4], ParityPlayer::even);
}

TEST_CASE(parity_game_randomized_differential_against_strategy_enumeration) {
  using namespace parity_game_test_detail;
  std::mt19937_64 rng(0x504152495459ULL);
  for (std::size_t trial = 0; trial < 180; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 6U);
    Graph graph(n, true);
    std::vector<ParityPlayer> owner(n, ParityPlayer::even);
    std::vector<std::uint32_t> priority(n, 0);

    for (std::size_t vertex = 0; vertex < n; ++vertex) {
      owner[vertex] = ((rng() & 1U) == 0U) ? ParityPlayer::even
                                           : ParityPlayer::odd;
      priority[vertex] = static_cast<std::uint32_t>(rng() % 6U);

      const Vertex first = static_cast<Vertex>(rng() % n);
      graph.add_edge(vertex, first,
                     static_cast<std::int64_t>(rng() % 101U) - 50);
      if (n > 1U && (rng() % 3U) != 0U) {
        Vertex second = static_cast<Vertex>(rng() % n);
        while (second == first) {
          second = static_cast<Vertex>(rng() % n);
        }
        graph.add_edge(vertex, second,
                       static_cast<std::int64_t>(rng() % 101U) - 50);
        if ((rng() % 4U) == 0U) {
          graph.add_edge(vertex, second,
                         static_cast<std::int64_t>(rng() % 101U) - 50);
        }
      }
    }

    const auto expected = exhaustive_positional_oracle(graph, owner, priority);
    const auto actual = algorithms::games::solve_parity_game(
        graph, owner, priority);
    REQUIRE_EQ(actual.winner, expected);
    REQUIRE(actual.recursive_calls >= 1U);
  }
}
