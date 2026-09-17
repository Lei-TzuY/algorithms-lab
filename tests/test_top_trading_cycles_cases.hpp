#pragma once

#include "algorithms/combinatorial/top_trading_cycles.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::combinatorial::TopTradingCyclesResult;
using algorithms::combinatorial::top_trading_cycles;
using PreferenceProfile = std::vector<std::vector<std::size_t>>;

std::vector<std::vector<std::size_t>> all_permutations(std::size_t n) {
  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(), 0U);
  std::vector<std::vector<std::size_t>> result;
  do {
    result.push_back(permutation);
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return result;
}

std::vector<std::vector<std::size_t>> rank_table(
    const PreferenceProfile& preferences) {
  const std::size_t n = preferences.size();
  std::vector<std::vector<std::size_t>> rank(
      n, std::vector<std::size_t>(n, n));
  for (std::size_t agent = 0; agent < n; ++agent) {
    for (std::size_t position = 0; position < n; ++position) {
      rank[agent][preferences[agent][position]] = position;
    }
  }
  return rank;
}

bool is_permutation_allocation(const std::vector<std::size_t>& allocation) {
  const std::size_t n = allocation.size();
  std::vector<std::uint8_t> seen(n, 0U);
  for (const std::size_t house : allocation) {
    if (house >= n || seen[house] != 0U) {
      return false;
    }
    seen[house] = 1U;
  }
  return true;
}

// Independent core criterion. Given an allocation, add a weak arc i->j when
// agent i weakly prefers house j (initially owned by j) to allocation[i]. With
// strict preferences, weak means either strictly better or exactly the same
// house. A coalition blocks iff there is a directed cycle of weak arcs that
// contains at least one strict arc. Equivalently, some strict arc u->v has a
// weak path from v back to u.
bool allocation_is_core(const PreferenceProfile& preferences,
                        const std::vector<std::size_t>& allocation) {
  if (!is_permutation_allocation(allocation)) {
    return false;
  }
  const std::size_t n = preferences.size();
  const auto rank = rank_table(preferences);
  std::vector<std::vector<std::size_t>> weak(n);
  std::vector<std::pair<std::size_t, std::size_t>> strict_edges;

  for (std::size_t agent = 0; agent < n; ++agent) {
    const std::size_t assigned_rank = rank[agent][allocation[agent]];
    for (std::size_t owner = 0; owner < n; ++owner) {
      if (rank[agent][owner] <= assigned_rank) {
        weak[agent].push_back(owner);
        if (rank[agent][owner] < assigned_rank) {
          strict_edges.emplace_back(agent, owner);
        }
      }
    }
  }

  for (const auto& [source, target] : strict_edges) {
    std::vector<std::uint8_t> seen(n, 0U);
    std::vector<std::size_t> stack{target};
    seen[target] = 1U;
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      if (current == source) {
        return false;
      }
      for (const std::size_t next : weak[current]) {
        if (seen[next] == 0U) {
          seen[next] = 1U;
          stack.push_back(next);
        }
      }
    }
  }
  return true;
}

std::vector<std::vector<std::size_t>> enumerate_core_allocations(
    const PreferenceProfile& preferences) {
  const std::size_t n = preferences.size();
  std::vector<std::size_t> allocation(n);
  std::iota(allocation.begin(), allocation.end(), 0U);
  std::vector<std::vector<std::size_t>> cores;
  do {
    if (allocation_is_core(preferences, allocation)) {
      cores.push_back(allocation);
    }
  } while (std::next_permutation(allocation.begin(), allocation.end()));
  return cores;
}

void replay_round_witness(const PreferenceProfile& preferences,
                          const TopTradingCyclesResult& result) {
  const std::size_t n = preferences.size();
  REQUIRE_EQ(result.allocation.size(), n);
  REQUIRE(is_permutation_allocation(result.allocation));

  std::vector<std::uint8_t> active(n, 1U);
  std::vector<std::uint8_t> assigned_agent(n, 0U);
  std::size_t remaining = n;

  for (const auto& round : result.rounds) {
    REQUIRE(!round.cycles.empty());
    std::vector<std::size_t> top(n, n);
    for (std::size_t agent = 0; agent < n; ++agent) {
      if (active[agent] == 0U) {
        continue;
      }
      for (const std::size_t house : preferences[agent]) {
        if (active[house] != 0U) {
          top[agent] = house;
          break;
        }
      }
      REQUIRE(top[agent] < n);
    }

    std::vector<std::uint8_t> removed(n, 0U);
    std::size_t prior_first = 0U;
    bool have_prior = false;
    for (const auto& cycle : round.cycles) {
      REQUIRE(!cycle.empty());
      REQUIRE_EQ(cycle.front(),
                 *std::min_element(cycle.begin(), cycle.end()));
      if (have_prior) {
        REQUIRE(prior_first < cycle.front());
      }
      prior_first = cycle.front();
      have_prior = true;

      for (std::size_t index = 0; index < cycle.size(); ++index) {
        const std::size_t agent = cycle[index];
        const std::size_t next_agent = cycle[(index + 1U) % cycle.size()];
        REQUIRE(agent < n);
        REQUIRE(active[agent] != 0U);
        REQUIRE(removed[agent] == 0U);
        REQUIRE_EQ(top[agent], next_agent);
        REQUIRE_EQ(result.allocation[agent], top[agent]);
        removed[agent] = 1U;
      }
    }

    std::size_t removed_count = 0U;
    for (std::size_t agent = 0; agent < n; ++agent) {
      if (removed[agent] != 0U) {
        REQUIRE(assigned_agent[agent] == 0U);
        assigned_agent[agent] = 1U;
        active[agent] = 0U;
        ++removed_count;
      }
    }
    REQUIRE(removed_count > 0U);
    REQUIRE(removed_count <= remaining);
    remaining -= removed_count;
  }

  REQUIRE_EQ(remaining, 0U);
  for (const auto assigned : assigned_agent) {
    REQUIRE(assigned != 0U);
  }
}

TEST_CASE(top_trading_cycles_validates_complete_strict_preferences) {
  REQUIRE_THROWS_AS(top_trading_cycles({{0U, 1U}, {1U}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(top_trading_cycles({{0U, 0U}, {1U, 0U}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(top_trading_cycles({{0U, 2U}, {1U, 0U}}),
                    std::invalid_argument);

  const auto empty = top_trading_cycles({});
  REQUIRE(empty.allocation.empty());
  REQUIRE(empty.rounds.empty());
}

TEST_CASE(top_trading_cycles_replays_cycles_and_multiple_rounds) {
  const PreferenceProfile one_cycle{{1U, 0U, 2U},
                                    {2U, 1U, 0U},
                                    {0U, 2U, 1U}};
  const auto one = top_trading_cycles(one_cycle);
  REQUIRE_EQ(one.allocation, (std::vector<std::size_t>{1U, 2U, 0U}));
  REQUIRE_EQ(one.rounds.size(), 1U);
  REQUIRE_EQ(one.rounds[0].cycles,
             (std::vector<std::vector<std::size_t>>{{0U, 1U, 2U}}));
  replay_round_witness(one_cycle, one);

  const PreferenceProfile two_rounds{{0U, 1U, 2U},
                                     {0U, 2U, 1U},
                                     {1U, 2U, 0U}};
  const auto two = top_trading_cycles(two_rounds);
  REQUIRE_EQ(two.allocation, (std::vector<std::size_t>{0U, 2U, 1U}));
  REQUIRE_EQ(two.rounds.size(), 2U);
  REQUIRE_EQ(two.rounds[0].cycles,
             (std::vector<std::vector<std::size_t>>{{0U}}));
  REQUIRE_EQ(two.rounds[1].cycles,
             (std::vector<std::vector<std::size_t>>{{1U, 2U}}));
  replay_round_witness(two_rounds, two);

  const PreferenceProfile own_first{{0U, 1U, 2U},
                                    {1U, 2U, 0U},
                                    {2U, 0U, 1U}};
  const auto self = top_trading_cycles(own_first);
  REQUIRE_EQ(self.rounds.size(), 1U);
  REQUIRE_EQ(self.rounds[0].cycles,
             (std::vector<std::vector<std::size_t>>{{0U}, {1U}, {2U}}));
  replay_round_witness(own_first, self);
}

TEST_CASE(top_trading_cycles_exhaustive_profiles_through_three_agents) {
  for (std::size_t n = 0U; n <= 3U; ++n) {
    const auto rows = all_permutations(n);
    PreferenceProfile profile(n);

    const auto enumerate_profiles = [&](auto&& self, std::size_t agent) -> void {
      if (agent == n) {
        const auto production = top_trading_cycles(profile);
        replay_round_witness(profile, production);
        const auto cores = enumerate_core_allocations(profile);
        REQUIRE_EQ(cores.size(), 1U);
        REQUIRE_EQ(production.allocation, cores.front());
        return;
      }
      for (const auto& row : rows) {
        profile[agent] = row;
        self(self, agent + 1U);
      }
    };

    enumerate_profiles(enumerate_profiles, 0U);
  }
}

TEST_CASE(top_trading_cycles_randomized_unique_core_differential) {
  std::mt19937_64 rng(0x5454435F434F5245ULL);
  for (std::size_t trial = 0U; trial < 360U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 7U);
    PreferenceProfile profile(n, std::vector<std::size_t>(n));
    for (std::size_t agent = 0; agent < n; ++agent) {
      std::iota(profile[agent].begin(), profile[agent].end(), 0U);
      std::shuffle(profile[agent].begin(), profile[agent].end(), rng);
    }

    const auto production = top_trading_cycles(profile);
    const auto repeated = top_trading_cycles(profile);
    REQUIRE_EQ(production, repeated);
    replay_round_witness(profile, production);

    const auto cores = enumerate_core_allocations(profile);
    REQUIRE_EQ(cores.size(), 1U);
    REQUIRE_EQ(production.allocation, cores.front());
  }
}

}  // namespace
