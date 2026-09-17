#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct TopTradingCyclesRound {
  std::vector<std::vector<std::size_t>> cycles;

  friend bool operator==(const TopTradingCyclesRound&,
                         const TopTradingCyclesRound&) = default;
};

struct TopTradingCyclesResult {
  // allocation[agent] is the house assigned to that agent. House h is initially
  // endowed to agent h.
  std::vector<std::size_t> allocation;
  std::vector<TopTradingCyclesRound> rounds;

  friend bool operator==(const TopTradingCyclesResult&,
                         const TopTradingCyclesResult&) = default;
};

namespace top_trading_cycles_detail {

inline void validate_preferences(
    const std::vector<std::vector<std::size_t>>& preferences) {
  const std::size_t n = preferences.size();
  for (const auto& row : preferences) {
    if (row.size() != n) {
      throw std::invalid_argument(
          "top trading cycles requires one complete preference row per agent");
    }
    std::vector<std::uint8_t> seen(n, 0U);
    for (const std::size_t house : row) {
      if (house >= n || seen[house] != 0U) {
        throw std::invalid_argument(
            "top trading cycles preferences must be permutations of all houses");
      }
      seen[house] = 1U;
    }
  }
}

inline void canonicalize_cycle(std::vector<std::size_t>& cycle) {
  if (cycle.empty()) {
    throw std::logic_error("top trading cycles produced an empty cycle");
  }
  const auto minimum = std::min_element(cycle.begin(), cycle.end());
  std::rotate(cycle.begin(), minimum, cycle.end());
}

}  // namespace top_trading_cycles_detail

// Shapley-Scarf housing market with one agent and one initially-owned house per
// index. Preferences are complete and strict. Every round each active agent
// points to their most-preferred active house, each active house points to its
// owner, all directed cycles trade simultaneously, and those agents/houses
// leave the market.
//
// The classical top-trading-cycles theorem states that this returns the unique
// core allocation for strict preferences. That theorem is a mathematical proof
// obligation; the implementation below only realizes the round/cycle process.
//
// This direct implementation keeps one monotone preference cursor per agent.
// Each cursor advances at most n times total, while each round performs O(n)
// functional-graph work across at most n rounds: O(n^2) time and O(n^2) input /
// result scale (O(n) working state excluding the returned cycle witness).
inline TopTradingCyclesResult top_trading_cycles(
    const std::vector<std::vector<std::size_t>>& preferences) {
  using top_trading_cycles_detail::canonicalize_cycle;
  using top_trading_cycles_detail::validate_preferences;

  validate_preferences(preferences);
  const std::size_t n = preferences.size();

  TopTradingCyclesResult result;
  result.allocation.assign(n, n);
  if (n == 0U) {
    return result;
  }

  std::vector<std::uint8_t> active(n, 1U);
  std::vector<std::size_t> cursor(n, 0U);
  std::vector<std::size_t> top_house(n, n);
  std::size_t remaining = n;

  while (remaining != 0U) {
    for (std::size_t agent = 0; agent < n; ++agent) {
      if (active[agent] == 0U) {
        continue;
      }
      while (cursor[agent] < n &&
             active[preferences[agent][cursor[agent]]] == 0U) {
        ++cursor[agent];
      }
      // The active agent's own house is active, so some choice must remain.
      if (cursor[agent] == n) {
        throw std::logic_error(
            "top trading cycles lost every active house for an active agent");
      }
      top_house[agent] = preferences[agent][cursor[agent]];
    }

    std::vector<std::uint8_t> state(n, 0U);  // 0 unseen, 1 path, 2 complete.
    TopTradingCyclesRound round;

    for (std::size_t start = 0; start < n; ++start) {
      if (active[start] == 0U || state[start] != 0U) {
        continue;
      }

      std::vector<std::size_t> path;
      std::size_t current = start;
      while (state[current] == 0U) {
        state[current] = 1U;
        path.push_back(current);
        current = top_house[current];  // House id equals its original owner id.
      }

      if (state[current] == 1U) {
        const auto cycle_begin =
            std::find(path.begin(), path.end(), current);
        if (cycle_begin == path.end()) {
          throw std::logic_error(
              "top trading cycles functional path lost its active cycle");
        }
        std::vector<std::size_t> cycle(cycle_begin, path.end());
        canonicalize_cycle(cycle);
        round.cycles.push_back(std::move(cycle));
      }

      for (const std::size_t vertex : path) {
        state[vertex] = 2U;
      }
    }

    if (round.cycles.empty()) {
      throw std::logic_error(
          "top trading cycles round contained no directed cycle");
    }
    std::sort(round.cycles.begin(), round.cycles.end(),
              [](const auto& lhs, const auto& rhs) {
                return lhs.front() < rhs.front();
              });

    std::size_t removed_this_round = 0U;
    for (const auto& cycle : round.cycles) {
      for (const std::size_t agent : cycle) {
        if (active[agent] == 0U) {
          throw std::logic_error(
              "top trading cycles attempted to remove an inactive agent");
        }
        const std::size_t house = top_house[agent];
        if (active[house] == 0U) {
          throw std::logic_error(
              "top trading cycles attempted to assign an inactive house");
        }
        result.allocation[agent] = house;
      }
      for (const std::size_t agent : cycle) {
        active[agent] = 0U;
        ++removed_this_round;
      }
    }

    if (removed_this_round == 0U || removed_this_round > remaining) {
      throw std::logic_error("top trading cycles failed to make progress");
    }
    remaining -= removed_this_round;
    result.rounds.push_back(std::move(round));
  }

  std::vector<std::uint8_t> assigned(n, 0U);
  for (const std::size_t house : result.allocation) {
    if (house >= n || assigned[house] != 0U) {
      throw std::logic_error(
          "top trading cycles did not return a house permutation");
    }
    assigned[house] = 1U;
  }
  return result;
}

}  // namespace algorithms::combinatorial
