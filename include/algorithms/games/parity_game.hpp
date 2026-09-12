#pragma once

#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::games {

enum class ParityPlayer : std::uint8_t { even = 0, odd = 1 };

struct ParityGameResult {
  std::vector<ParityPlayer> winner;
  std::size_t recursive_calls = 0;
  std::size_t attractor_vertex_insertions = 0;
};

namespace detail {

struct ParitySubsolution {
  std::vector<unsigned char> even;
  std::vector<unsigned char> odd;
};

[[nodiscard]] inline ParityPlayer opposite(ParityPlayer player) noexcept {
  return player == ParityPlayer::even ? ParityPlayer::odd : ParityPlayer::even;
}

[[nodiscard]] inline bool owns(ParityPlayer owner, ParityPlayer player) noexcept {
  return owner == player;
}

[[nodiscard]] inline std::vector<unsigned char> parity_attractor(
    const algorithms::graphs::Graph& graph,
    std::span<const ParityPlayer> owner,
    const std::vector<std::vector<algorithms::graphs::Vertex>>& reverse,
    const std::vector<unsigned char>& active,
    const std::vector<unsigned char>& seed,
    ParityPlayer player,
    std::size_t& insertion_counter) {
  const std::size_t n = graph.vertex_count();
  std::vector<unsigned char> in_attractor(n, 0);
  std::vector<std::size_t> remaining(n, 0);
  std::deque<algorithms::graphs::Vertex> queue;

  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (active[vertex] == 0U) {
      continue;
    }
    for (const auto& edge : graph.neighbors(vertex)) {
      if (active[edge.to] != 0U) {
        ++remaining[vertex];
      }
    }
    if (remaining[vertex] == 0U) {
      throw std::logic_error("parity subgame unexpectedly has a dead end");
    }
    if (seed[vertex] != 0U) {
      in_attractor[vertex] = 1U;
      queue.push_back(vertex);
    }
  }

  while (!queue.empty()) {
    const auto target = queue.front();
    queue.pop_front();
    for (const auto predecessor : reverse[target]) {
      if (active[predecessor] == 0U || in_attractor[predecessor] != 0U) {
        continue;
      }
      if (owns(owner[predecessor], player)) {
        in_attractor[predecessor] = 1U;
        ++insertion_counter;
        queue.push_back(predecessor);
        continue;
      }
      if (remaining[predecessor] == 0U) {
        throw std::logic_error("parity attractor edge accounting underflow");
      }
      --remaining[predecessor];
      if (remaining[predecessor] == 0U) {
        in_attractor[predecessor] = 1U;
        ++insertion_counter;
        queue.push_back(predecessor);
      }
    }
  }

  return in_attractor;
}

[[nodiscard]] inline bool any_marked(const std::vector<unsigned char>& mask) {
  return std::any_of(mask.begin(), mask.end(), [](unsigned char value) {
    return value != 0U;
  });
}

[[nodiscard]] inline ParitySubsolution solve_parity_subgame(
    const algorithms::graphs::Graph& graph,
    std::span<const ParityPlayer> owner,
    std::span<const std::uint32_t> priority,
    const std::vector<std::vector<algorithms::graphs::Vertex>>& reverse,
    const std::vector<unsigned char>& active,
    std::size_t& recursive_calls,
    std::size_t& insertion_counter) {
  ++recursive_calls;
  const std::size_t n = graph.vertex_count();
  ParitySubsolution empty{std::vector<unsigned char>(n, 0),
                          std::vector<unsigned char>(n, 0)};

  bool found = false;
  std::uint32_t highest_priority = 0;
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (active[vertex] == 0U) {
      continue;
    }
    if (!found || priority[vertex] > highest_priority) {
      found = true;
      highest_priority = priority[vertex];
    }
  }
  if (!found) {
    return empty;
  }

  const ParityPlayer player =
      (highest_priority % 2U == 0U) ? ParityPlayer::even : ParityPlayer::odd;
  const ParityPlayer opponent = opposite(player);

  std::vector<unsigned char> seed(n, 0);
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (active[vertex] != 0U && priority[vertex] == highest_priority) {
      seed[vertex] = 1U;
    }
  }

  const auto attractor = parity_attractor(graph, owner, reverse, active, seed,
                                           player, insertion_counter);
  auto without_attractor = active;
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (attractor[vertex] != 0U) {
      without_attractor[vertex] = 0U;
    }
  }

  auto first = solve_parity_subgame(graph, owner, priority, reverse,
                                    without_attractor, recursive_calls,
                                    insertion_counter);
  const auto& opponent_region =
      (opponent == ParityPlayer::even) ? first.even : first.odd;

  if (!any_marked(opponent_region)) {
    for (std::size_t vertex = 0; vertex < n; ++vertex) {
      if (attractor[vertex] == 0U) {
        continue;
      }
      if (player == ParityPlayer::even) {
        first.even[vertex] = 1U;
      } else {
        first.odd[vertex] = 1U;
      }
    }
    return first;
  }

  const auto opponent_attractor = parity_attractor(
      graph, owner, reverse, active, opponent_region, opponent,
      insertion_counter);
  auto without_opponent_attractor = active;
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (opponent_attractor[vertex] != 0U) {
      without_opponent_attractor[vertex] = 0U;
    }
  }

  auto second = solve_parity_subgame(graph, owner, priority, reverse,
                                     without_opponent_attractor,
                                     recursive_calls, insertion_counter);
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (opponent_attractor[vertex] == 0U) {
      continue;
    }
    if (opponent == ParityPlayer::even) {
      second.even[vertex] = 1U;
    } else {
      second.odd[vertex] = 1U;
    }
  }
  return second;
}

}  // namespace detail

[[nodiscard]] inline ParityGameResult solve_parity_game(
    const algorithms::graphs::Graph& graph,
    std::span<const ParityPlayer> owner,
    std::span<const std::uint32_t> priority) {
  if (!graph.directed()) {
    throw std::invalid_argument("parity game requires a directed graph");
  }
  const std::size_t n = graph.vertex_count();
  if (owner.size() != n || priority.size() != n) {
    throw std::invalid_argument("parity-game owner/priority size mismatch");
  }

  std::vector<std::vector<algorithms::graphs::Vertex>> reverse(n);
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    if (owner[vertex] != ParityPlayer::even &&
        owner[vertex] != ParityPlayer::odd) {
      throw std::invalid_argument("parity-game owner value is invalid");
    }
    if (graph.neighbors(vertex).empty()) {
      throw std::invalid_argument("parity game requires at least one move per vertex");
    }
    for (const auto& edge : graph.neighbors(vertex)) {
      reverse[edge.to].push_back(vertex);
    }
  }

  std::vector<unsigned char> active(n, 1U);
  std::size_t recursive_calls = 0;
  std::size_t insertion_counter = 0;
  const auto solved = detail::solve_parity_subgame(
      graph, owner, priority, reverse, active, recursive_calls,
      insertion_counter);

  ParityGameResult result;
  result.winner.resize(n, ParityPlayer::even);
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    const bool even = solved.even[vertex] != 0U;
    const bool odd = solved.odd[vertex] != 0U;
    if (even == odd) {
      throw std::logic_error("parity-game solution is not a partition");
    }
    result.winner[vertex] = even ? ParityPlayer::even : ParityPlayer::odd;
  }
  result.recursive_calls = recursive_calls;
  result.attractor_vertex_insertions = insertion_counter;
  return result;
}

}  // namespace algorithms::games
