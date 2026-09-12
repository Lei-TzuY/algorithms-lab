#include "algorithms/combinatorial/stable_matching.hpp"
#include "algorithms/combinatorial/stable_roommates.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::combinatorial::StableMatchingResult;
using algorithms::combinatorial::gale_shapley_stable_matching;
using Preferences = std::vector<std::vector<std::size_t>>;

std::vector<std::vector<std::size_t>> build_ranks(const Preferences& prefs) {
  const std::size_t n = prefs.size();
  std::vector<std::vector<std::size_t>> rank(n, std::vector<std::size_t>(n));
  for (std::size_t person = 0; person < n; ++person) {
    for (std::size_t pos = 0; pos < n; ++pos) {
      rank[person][prefs[person][pos]] = pos;
    }
  }
  return rank;
}

bool is_stable(const std::vector<std::size_t>& proposer_to_receiver,
               const Preferences& proposer_preferences,
               const Preferences& receiver_preferences) {
  const std::size_t n = proposer_preferences.size();
  const auto proposer_rank = build_ranks(proposer_preferences);
  const auto receiver_rank = build_ranks(receiver_preferences);
  std::vector<std::size_t> receiver_to_proposer(n, n);
  for (std::size_t p = 0; p < n; ++p) {
    if (proposer_to_receiver[p] >= n ||
        receiver_to_proposer[proposer_to_receiver[p]] != n) {
      return false;
    }
    receiver_to_proposer[proposer_to_receiver[p]] = p;
  }
  for (std::size_t p = 0; p < n; ++p) {
    for (std::size_t r = 0; r < n; ++r) {
      if (proposer_rank[p][r] < proposer_rank[p][proposer_to_receiver[p]] &&
          receiver_rank[r][p] < receiver_rank[r][receiver_to_proposer[r]]) {
        return false;
      }
    }
  }
  return true;
}

std::vector<std::vector<std::size_t>> enumerate_stable_matchings(
    const Preferences& proposer_preferences,
    const Preferences& receiver_preferences) {
  const std::size_t n = proposer_preferences.size();
  std::vector<std::size_t> matching(n);
  std::iota(matching.begin(), matching.end(), 0U);
  std::vector<std::vector<std::size_t>> stable;
  do {
    if (is_stable(matching, proposer_preferences, receiver_preferences)) {
      stable.push_back(matching);
    }
  } while (std::next_permutation(matching.begin(), matching.end()));
  return stable;
}

void verify_result(const StableMatchingResult& result,
                   const Preferences& proposer,
                   const Preferences& receiver) {
  const std::size_t n = proposer.size();
  REQUIRE_EQ(result.proposer_to_receiver.size(), n);
  REQUIRE_EQ(result.receiver_to_proposer.size(), n);
  REQUIRE(result.proposal_count <= n * n);
  for (std::size_t p = 0; p < n; ++p) {
    const std::size_t r = result.proposer_to_receiver[p];
    REQUIRE(r < n);
    REQUIRE_EQ(result.receiver_to_proposer[r], p);
  }
  REQUIRE(is_stable(result.proposer_to_receiver, proposer, receiver));
}

Preferences random_preferences(const std::size_t n, std::mt19937_64& rng) {
  Preferences prefs(n, std::vector<std::size_t>(n));
  for (auto& row : prefs) {
    std::iota(row.begin(), row.end(), 0U);
    std::shuffle(row.begin(), row.end(), rng);
  }
  return prefs;
}
}  // namespace

TEST_CASE(stable_matching_empty_singleton_and_validation) {
  const Preferences empty;
  const auto empty_result = gale_shapley_stable_matching(empty, empty);
  REQUIRE(empty_result.proposer_to_receiver.empty());
  REQUIRE(empty_result.receiver_to_proposer.empty());
  REQUIRE_EQ(empty_result.proposal_count, 0U);

  const Preferences singleton{{0}};
  const auto one = gale_shapley_stable_matching(singleton, singleton);
  REQUIRE_EQ(one.proposer_to_receiver, std::vector<std::size_t>({0}));
  REQUIRE_EQ(one.receiver_to_proposer, std::vector<std::size_t>({0}));
  REQUIRE_EQ(one.proposal_count, 1U);

  REQUIRE_THROWS_AS(gale_shapley_stable_matching({{0}, {1}}, {{0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      gale_shapley_stable_matching({{0, 1}, {0}}, {{0, 1}, {1, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      gale_shapley_stable_matching({{0, 0}, {0, 1}}, {{0, 1}, {1, 0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      gale_shapley_stable_matching({{0, 2}, {0, 1}}, {{0, 1}, {1, 0}}),
      std::out_of_range);
  REQUIRE_THROWS_AS(
      gale_shapley_stable_matching({{0, 1}, {1, 0}}, {{0, 0}, {0, 1}}),
      std::invalid_argument);
}

TEST_CASE(stable_matching_displacement_chain_and_determinism) {
  const Preferences proposer{{0, 1, 2}, {0, 1, 2}, {1, 0, 2}};
  const Preferences receiver{{1, 0, 2}, {0, 1, 2}, {2, 1, 0}};
  const auto first = gale_shapley_stable_matching(proposer, receiver);
  const auto second = gale_shapley_stable_matching(proposer, receiver);
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first.proposer_to_receiver,
             std::vector<std::size_t>({1, 0, 2}));
  REQUIRE_EQ(first.receiver_to_proposer,
             std::vector<std::size_t>({1, 0, 2}));
  REQUIRE_EQ(first.proposal_count, 6U);
  verify_result(first, proposer, receiver);
}

TEST_CASE(stable_matching_exhaustive_proposer_optimality_randomized) {
  std::mt19937_64 rng(0x57AB1EULL);
  for (std::size_t trial = 0; trial < 450U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 6U);
    const Preferences proposer = random_preferences(n, rng);
    const Preferences receiver = random_preferences(n, rng);
    const StableMatchingResult actual =
        gale_shapley_stable_matching(proposer, receiver);
    verify_result(actual, proposer, receiver);

    const auto stable = enumerate_stable_matchings(proposer, receiver);
    REQUIRE(!stable.empty());
    const auto proposer_rank = build_ranks(proposer);
    for (std::size_t p = 0; p < n; ++p) {
      std::size_t best_rank = n;
      for (const auto& candidate : stable) {
        best_rank = std::min(best_rank, proposer_rank[p][candidate[p]]);
      }
      REQUIRE_EQ(proposer_rank[p][actual.proposer_to_receiver[p]], best_rank);
    }
  }
}

TEST_CASE(stable_matching_proposal_bound_and_adversarial_shared_first_choice) {
  constexpr std::size_t n = 8U;
  Preferences proposer(n, std::vector<std::size_t>(n));
  Preferences receiver(n, std::vector<std::size_t>(n));
  for (std::size_t p = 0; p < n; ++p) {
    std::iota(proposer[p].begin(), proposer[p].end(), 0U);
  }
  for (std::size_t r = 0; r < n; ++r) {
    for (std::size_t rank = 0; rank < n; ++rank) {
      receiver[r][rank] = n - 1U - rank;
    }
  }
  const auto result = gale_shapley_stable_matching(proposer, receiver);
  verify_result(result, proposer, receiver);
  REQUIRE(result.proposal_count > n);
  REQUIRE(result.proposal_count <= n * n);
}

namespace {
using algorithms::combinatorial::StableRoommatesResult;
using algorithms::combinatorial::stable_roommates_irving;
using RoommatePreferences = std::vector<std::vector<std::size_t>>;

std::vector<std::vector<std::size_t>> roommate_build_ranks(
    const RoommatePreferences& preferences) {
  const std::size_t n = preferences.size();
  std::vector<std::vector<std::size_t>> rank(
      n, std::vector<std::size_t>(n, n));
  for (std::size_t person = 0; person < n; ++person) {
    for (std::size_t position = 0; position < preferences[person].size();
         ++position) {
      rank[person][preferences[person][position]] = position;
    }
  }
  return rank;
}

bool roommate_matching_is_stable(
    const std::vector<std::size_t>& partner,
    const RoommatePreferences& preferences,
    const std::vector<std::vector<std::size_t>>& rank) {
  const std::size_t n = preferences.size();
  if (partner.size() != n) {
    return false;
  }
  for (std::size_t person = 0; person < n; ++person) {
    if (partner[person] >= n || partner[person] == person ||
        partner[partner[person]] != person) {
      return false;
    }
  }
  for (std::size_t first = 0; first < n; ++first) {
    for (std::size_t second = first + 1U; second < n; ++second) {
      if (partner[first] == second) {
        continue;
      }
      if (rank[first][second] < rank[first][partner[first]] &&
          rank[second][first] < rank[second][partner[second]]) {
        return false;
      }
    }
  }
  return true;
}

bool roommate_exhaustive_search(
    std::vector<std::size_t>& partner,
    const RoommatePreferences& preferences,
    const std::vector<std::vector<std::size_t>>& rank) {
  const std::size_t n = preferences.size();
  std::size_t first = 0U;
  while (first < n && partner[first] != n) {
    ++first;
  }
  if (first == n) {
    return roommate_matching_is_stable(partner, preferences, rank);
  }
  for (std::size_t second = first + 1U; second < n; ++second) {
    if (partner[second] != n) {
      continue;
    }
    partner[first] = second;
    partner[second] = first;
    if (roommate_exhaustive_search(partner, preferences, rank)) {
      return true;
    }
    partner[first] = n;
    partner[second] = n;
  }
  return false;
}

bool roommate_has_stable_matching(const RoommatePreferences& preferences) {
  const std::size_t n = preferences.size();
  if (n == 0U) {
    return true;
  }
  if ((n & 1U) != 0U) {
    return false;
  }
  std::vector<std::size_t> partner(n, n);
  return roommate_exhaustive_search(
      partner, preferences, roommate_build_ranks(preferences));
}

RoommatePreferences random_roommate_preferences(const std::size_t n,
                                                std::mt19937_64& rng) {
  RoommatePreferences preferences(n);
  for (std::size_t person = 0; person < n; ++person) {
    for (std::size_t candidate = 0; candidate < n; ++candidate) {
      if (candidate != person) {
        preferences[person].push_back(candidate);
      }
    }
    std::shuffle(preferences[person].begin(), preferences[person].end(), rng);
  }
  return preferences;
}

void verify_roommates_result(const StableRoommatesResult& result,
                             const RoommatePreferences& preferences) {
  const std::size_t n = preferences.size();
  const auto rank = roommate_build_ranks(preferences);
  REQUIRE(roommate_matching_is_stable(result.partner, preferences, rank));
  REQUIRE(result.phase1_proposal_count <= n * (n - 1U));
}
}  // namespace

TEST_CASE(stable_roommates_validation_trivial_and_odd_cardinality) {
  const RoommatePreferences empty;
  const auto empty_result = stable_roommates_irving(empty);
  REQUIRE(empty_result.has_value());
  REQUIRE(empty_result->partner.empty());
  REQUIRE_EQ(empty_result->phase1_proposal_count, 0U);
  REQUIRE_EQ(empty_result->rotation_count, 0U);

  const RoommatePreferences pair{{1}, {0}};
  const auto pair_result = stable_roommates_irving(pair);
  REQUIRE(pair_result.has_value());
  REQUIRE_EQ(pair_result->partner, std::vector<std::size_t>({1, 0}));
  verify_roommates_result(*pair_result, pair);

  const RoommatePreferences singleton{{}};
  REQUIRE(!stable_roommates_irving(singleton).has_value());
  const RoommatePreferences odd{{1, 2}, {0, 2}, {0, 1}};
  REQUIRE(!stable_roommates_irving(odd).has_value());

  REQUIRE_THROWS_AS(stable_roommates_irving({{1}, {0, 0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(stable_roommates_irving({{0}, {0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(stable_roommates_irving({{2}, {0}}),
                    std::out_of_range);
  REQUIRE_THROWS_AS(
      stable_roommates_irving({{1, 1}, {0, 2}, {0, 1}}),
      std::invalid_argument);
}

TEST_CASE(stable_roommates_rotation_and_unsatisfiable_regressions) {
  const RoommatePreferences rotation{{4, 1, 2, 3, 5},
                                     {2, 4, 0, 3, 5},
                                     {4, 0, 5, 3, 1},
                                     {5, 1, 0, 4, 2},
                                     {0, 3, 2, 5, 1},
                                     {1, 4, 3, 2, 0}};
  const auto first = stable_roommates_irving(rotation);
  const auto second = stable_roommates_irving(rotation);
  REQUIRE(first.has_value());
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first->partner,
             std::vector<std::size_t>({4, 2, 1, 5, 0, 3}));
  REQUIRE_EQ(first->phase1_proposal_count, 7U);
  REQUIRE_EQ(first->rotation_count, 1U);
  verify_roommates_result(*first, rotation);

  const RoommatePreferences no_stable{{5, 3, 4, 1, 2},
                                      {3, 0, 4, 2, 5},
                                      {5, 4, 3, 1, 0},
                                      {2, 5, 0, 4, 1},
                                      {3, 2, 5, 1, 0},
                                      {0, 2, 4, 3, 1}};
  REQUIRE(roommate_has_stable_matching(no_stable) == false);
  REQUIRE(!stable_roommates_irving(no_stable).has_value());
}

TEST_CASE(stable_roommates_exhaustive_randomized_differential) {
  std::mt19937_64 rng(0x1A71B6ULL);
  constexpr std::size_t sizes[]{2U, 4U, 6U, 8U};
  for (std::size_t trial = 0; trial < 1200U; ++trial) {
    const std::size_t n = sizes[static_cast<std::size_t>(rng() % 4U)];
    const RoommatePreferences preferences = random_roommate_preferences(n, rng);
    const bool expected = roommate_has_stable_matching(preferences);
    const auto actual = stable_roommates_irving(preferences);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      verify_roommates_result(*actual, preferences);
      REQUIRE_EQ(actual, stable_roommates_irving(preferences));
    }
  }
}
