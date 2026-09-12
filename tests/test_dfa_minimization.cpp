#include "algorithms/automata/dfa_minimization.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::automata::Dfa;
using algorithms::automata::MinimizedDfa;

bool run_word(const Dfa& dfa, std::size_t start,
              const std::vector<std::size_t>& word) {
  std::size_t state = start;
  for (const std::size_t symbol : word) {
    state = dfa.transitions[state][symbol];
  }
  return dfa.accepting[state];
}

bool run_word(const MinimizedDfa& dfa, std::size_t start,
              const std::vector<std::size_t>& word) {
  std::size_t state = start;
  for (const std::size_t symbol : word) {
    state = dfa.transitions[state][symbol];
  }
  return dfa.accepting[state];
}

std::vector<unsigned char> reachable(const Dfa& dfa) {
  std::vector<unsigned char> seen(dfa.transitions.size(), 0U);
  std::vector<std::size_t> queue{dfa.start_state};
  seen[dfa.start_state] = 1U;
  for (std::size_t head = 0U; head < queue.size(); ++head) {
    for (const std::size_t next : dfa.transitions[queue[head]]) {
      if (seen[next] == 0U) {
        seen[next] = 1U;
        queue.push_back(next);
      }
    }
  }
  return seen;
}

std::vector<std::vector<unsigned char>> table_filling_distinguishable(
    const Dfa& dfa, const std::vector<unsigned char>& seen) {
  const std::size_t state_count = dfa.transitions.size();
  std::vector<std::vector<unsigned char>> distinguishable(
      state_count, std::vector<unsigned char>(state_count, 0U));

  for (std::size_t first = 0U; first < state_count; ++first) {
    for (std::size_t second = first + 1U; second < state_count; ++second) {
      if (seen[first] != 0U && seen[second] != 0U &&
          dfa.accepting[first] != dfa.accepting[second]) {
        distinguishable[first][second] = 1U;
        distinguishable[second][first] = 1U;
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t first = 0U; first < state_count; ++first) {
      for (std::size_t second = first + 1U; second < state_count; ++second) {
        if (seen[first] == 0U || seen[second] == 0U ||
            distinguishable[first][second] != 0U) {
          continue;
        }
        for (std::size_t symbol = 0U;
             symbol < dfa.transitions[first].size(); ++symbol) {
          const std::size_t next_first = dfa.transitions[first][symbol];
          const std::size_t next_second = dfa.transitions[second][symbol];
          if (distinguishable[next_first][next_second] != 0U) {
            distinguishable[first][second] = 1U;
            distinguishable[second][first] = 1U;
            changed = true;
            break;
          }
        }
      }
    }
  }
  return distinguishable;
}

void verify_minimization(const Dfa& dfa, const MinimizedDfa& minimized) {
  const auto seen = reachable(dfa);
  const auto distinguishable = table_filling_distinguishable(dfa, seen);

  REQUIRE_EQ(minimized.start_state, 0U);
  REQUIRE_EQ(minimized.original_to_minimized.size(), dfa.transitions.size());
  REQUIRE_EQ(minimized.minimized_to_original.size(),
             minimized.transitions.size());

  for (std::size_t state = 0U; state < dfa.transitions.size(); ++state) {
    REQUIRE_EQ(minimized.original_to_minimized[state].has_value(),
               seen[state] != 0U);
  }

  for (std::size_t first = 0U; first < dfa.transitions.size(); ++first) {
    for (std::size_t second = 0U; second < dfa.transitions.size(); ++second) {
      if (seen[first] == 0U || seen[second] == 0U) {
        continue;
      }
      REQUIRE_EQ(
          minimized.original_to_minimized[first] ==
              minimized.original_to_minimized[second],
          distinguishable[first][second] == 0U);
    }
  }

  for (std::size_t quotient = 0U; quotient < minimized.transitions.size();
       ++quotient) {
    REQUIRE_EQ(minimized.transitions[quotient].size(),
               dfa.transitions.front().size());
    for (const std::size_t target : minimized.transitions[quotient]) {
      REQUIRE(target < minimized.transitions.size());
    }
    for (const std::size_t original :
         minimized.minimized_to_original[quotient]) {
      REQUIRE_EQ(minimized.original_to_minimized[original],
                 std::optional<std::size_t>(quotient));
      REQUIRE_EQ(minimized.accepting[quotient], dfa.accepting[original]);
      for (std::size_t symbol = 0U;
           symbol < dfa.transitions[original].size(); ++symbol) {
        REQUIRE_EQ(
            minimized.transitions[quotient][symbol],
            minimized.original_to_minimized[dfa.transitions[original][symbol]]
                .value());
      }
    }
  }
}
}  // namespace

TEST_CASE(dfa_minimization_validation_and_zero_alphabet) {
  REQUIRE_THROWS_AS(algorithms::automata::minimize_dfa(Dfa{}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::automata::minimize_dfa(Dfa{1U, {{0U}}, {false}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::automata::minimize_dfa(Dfa{0U, {{0U}, {}}, {false, false}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::automata::minimize_dfa(Dfa{0U, {{1U}}, {false}}),
      std::invalid_argument);

  const Dfa zero{2U, {{}, {}, {}}, {false, true, true}};
  const auto minimized = algorithms::automata::minimize_dfa(zero);
  REQUIRE_EQ(minimized.transitions.size(), 1U);
  REQUIRE(minimized.accepting[0U]);
  REQUIRE(!minimized.original_to_minimized[0U].has_value());
  REQUIRE(!minimized.original_to_minimized[1U].has_value());
  REQUIRE_EQ(minimized.original_to_minimized[2U],
             std::optional<std::size_t>(0U));
}

TEST_CASE(dfa_minimization_merges_equivalent_and_prunes_unreachable) {
  const Dfa dfa{0U,
                {{1U, 2U}, {1U, 3U}, {1U, 2U}, {1U, 3U}, {4U, 4U}},
                {false, true, false, true, false}};
  const auto minimized = algorithms::automata::minimize_dfa(dfa);
  verify_minimization(dfa, minimized);
  REQUIRE_EQ(minimized.transitions.size(), 2U);
  REQUIRE(!minimized.original_to_minimized[4U].has_value());
  REQUIRE_EQ(minimized.original_to_minimized[0U],
             minimized.original_to_minimized[2U]);
  REQUIRE_EQ(minimized.original_to_minimized[1U],
             minimized.original_to_minimized[3U]);
}

TEST_CASE(dfa_minimization_canonical_numbering_is_repeatable) {
  const Dfa dfa{3U,
                {{0U, 0U}, {1U, 1U}, {0U, 1U}, {1U, 2U}},
                {false, true, false, false}};
  const auto first = algorithms::automata::minimize_dfa(dfa);
  const auto second = algorithms::automata::minimize_dfa(dfa);
  REQUIRE_EQ(first.transitions, second.transitions);
  REQUIRE_EQ(first.accepting, second.accepting);
  REQUIRE_EQ(first.original_to_minimized, second.original_to_minimized);
  REQUIRE_EQ(first.minimized_to_original, second.minimized_to_original);
  verify_minimization(dfa, first);
}

TEST_CASE(dfa_minimization_randomized_table_filling_differential) {
  std::mt19937_64 rng(0xDFA5EEDULL);
  for (std::size_t trial = 0U; trial < 800U; ++trial) {
    const std::size_t state_count =
        1U + static_cast<std::size_t>(rng() % 8U);
    const std::size_t alphabet_size = static_cast<std::size_t>(rng() % 4U);

    Dfa dfa;
    dfa.start_state = static_cast<std::size_t>(rng() % state_count);
    dfa.transitions.assign(
        state_count, std::vector<std::size_t>(alphabet_size, 0U));
    dfa.accepting.assign(state_count, false);
    for (std::size_t state = 0U; state < state_count; ++state) {
      dfa.accepting[state] = (rng() & 1U) != 0U;
      for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
        dfa.transitions[state][symbol] =
            static_cast<std::size_t>(rng() % state_count);
      }
    }

    const auto minimized = algorithms::automata::minimize_dfa(dfa);
    verify_minimization(dfa, minimized);

    for (std::size_t sample = 0U; sample < 80U; ++sample) {
      std::vector<std::size_t> word;
      const std::size_t length =
          alphabet_size == 0U ? 0U : static_cast<std::size_t>(rng() % 10U);
      word.reserve(length);
      for (std::size_t index = 0U; index < length; ++index) {
        word.push_back(static_cast<std::size_t>(rng() % alphabet_size));
      }
      REQUIRE_EQ(run_word(dfa, dfa.start_state, word),
                 run_word(minimized, minimized.start_state, word));
    }
  }
}

TEST_CASE(dfa_minimization_adversarial_refinement_chain_and_full_collapse) {
  Dfa chain;
  chain.start_state = 0U;
  chain.transitions.assign(64U, std::vector<std::size_t>(1U, 63U));
  chain.accepting.assign(64U, false);
  for (std::size_t state = 0U; state + 1U < 64U; ++state) {
    chain.transitions[state][0U] = state + 1U;
  }
  chain.accepting[63U] = true;
  const auto chain_minimized = algorithms::automata::minimize_dfa(chain);
  verify_minimization(chain, chain_minimized);
  REQUIRE_EQ(chain_minimized.transitions.size(), 64U);

  Dfa collapse;
  collapse.start_state = 0U;
  collapse.transitions.assign(96U, std::vector<std::size_t>(3U, 0U));
  collapse.accepting.assign(96U, false);
  for (std::size_t state = 0U; state < 96U; ++state) {
    collapse.transitions[state][0U] = (state + 1U) % 96U;
    collapse.transitions[state][1U] = (state + 17U) % 96U;
    collapse.transitions[state][2U] = (state * 5U + 3U) % 96U;
  }
  const auto collapsed = algorithms::automata::minimize_dfa(collapse);
  verify_minimization(collapse, collapsed);
  REQUIRE_EQ(collapsed.transitions.size(), 1U);
}

#include "algorithms/automata/nfa_determinization.hpp"

namespace {
using algorithms::automata::DeterminizedNfa;
using algorithms::automata::EpsilonNfa;

std::vector<std::size_t> nfa_epsilon_closure_oracle(
    const EpsilonNfa& nfa, const std::vector<std::size_t>& seeds) {
  std::vector<unsigned char> seen(nfa.accepting.size(), 0U);
  std::vector<std::size_t> queue;
  for (const std::size_t seed : seeds) {
    if (seen[seed] == 0U) {
      seen[seed] = 1U;
      queue.push_back(seed);
    }
  }
  for (std::size_t head = 0U; head < queue.size(); ++head) {
    for (const std::size_t next : nfa.epsilon_transitions[queue[head]]) {
      if (seen[next] == 0U) {
        seen[next] = 1U;
        queue.push_back(next);
      }
    }
  }
  std::vector<std::size_t> result;
  for (std::size_t state = 0U; state < seen.size(); ++state) {
    if (seen[state] != 0U) {
      result.push_back(state);
    }
  }
  return result;
}

std::vector<std::size_t> nfa_step_oracle(
    const EpsilonNfa& nfa, const std::vector<std::size_t>& subset,
    std::size_t symbol) {
  std::vector<unsigned char> selected(nfa.accepting.size(), 0U);
  std::vector<std::size_t> seeds;
  for (const std::size_t state : subset) {
    for (const std::size_t next : nfa.transitions[state][symbol]) {
      if (selected[next] == 0U) {
        selected[next] = 1U;
        seeds.push_back(next);
      }
    }
  }
  return nfa_epsilon_closure_oracle(nfa, seeds);
}

bool nfa_accepts_oracle(const EpsilonNfa& nfa,
                        const std::vector<std::size_t>& word) {
  auto active = nfa_epsilon_closure_oracle(nfa, {nfa.start_state});
  for (const std::size_t symbol : word) {
    active = nfa_step_oracle(nfa, active, symbol);
  }
  for (const std::size_t state : active) {
    if (nfa.accepting[state]) {
      return true;
    }
  }
  return false;
}

bool determinized_accepts(const DeterminizedNfa& result,
                          const std::vector<std::size_t>& word) {
  std::size_t state = result.dfa.start_state;
  for (const std::size_t symbol : word) {
    state = result.dfa.transitions[state][symbol];
  }
  return result.dfa.accepting[state];
}

void enumerate_nfa_words(std::size_t alphabet_size, std::size_t max_length,
                         std::vector<std::vector<std::size_t>>& output) {
  output.assign(1U, {});
  if (alphabet_size == 0U) {
    return;
  }
  std::vector<std::vector<std::size_t>> frontier(1U);
  for (std::size_t length = 1U; length <= max_length; ++length) {
    std::vector<std::vector<std::size_t>> next;
    for (const auto& prefix : frontier) {
      for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
        auto word = prefix;
        word.push_back(symbol);
        output.push_back(word);
        next.push_back(std::move(word));
      }
    }
    frontier = std::move(next);
  }
}

void verify_determinized_nfa(const EpsilonNfa& nfa,
                             const DeterminizedNfa& result) {
  REQUIRE_EQ(result.dfa.start_state, 0U);
  REQUIRE_EQ(result.dfa.transitions.size(), result.dfa_state_subsets.size());
  REQUIRE_EQ(result.dfa.accepting.size(), result.dfa_state_subsets.size());
  REQUIRE_EQ(result.dfa_state_subsets.front(),
             nfa_epsilon_closure_oracle(nfa, {nfa.start_state}));

  for (std::size_t state = 0U; state < result.dfa_state_subsets.size(); ++state) {
    const auto& subset = result.dfa_state_subsets[state];
    for (std::size_t index = 1U; index < subset.size(); ++index) {
      REQUIRE(subset[index - 1U] < subset[index]);
    }
    REQUIRE_EQ(result.dfa.transitions[state].size(),
               nfa.transitions.front().size());
    bool accepting = false;
    for (const std::size_t member : subset) {
      accepting = accepting || nfa.accepting[member];
    }
    REQUIRE_EQ(result.dfa.accepting[state], accepting);
    for (std::size_t symbol = 0U; symbol < nfa.transitions.front().size();
         ++symbol) {
      const std::size_t target = result.dfa.transitions[state][symbol];
      REQUIRE(target < result.dfa_state_subsets.size());
      REQUIRE_EQ(result.dfa_state_subsets[target],
                 nfa_step_oracle(nfa, subset, symbol));
    }
  }
}
}  // namespace

TEST_CASE(nfa_determinization_validation_budget_and_zero_alphabet) {
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(EpsilonNfa{}, 8U),
      std::invalid_argument);
  const EpsilonNfa bad_start{1U, {false}, {{}}, {{}}};
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(bad_start, 8U),
      std::invalid_argument);
  const EpsilonNfa bad_rows{0U, {false, false}, {{}, {}}, {{{}}, {}}};
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(bad_rows, 8U),
      std::invalid_argument);
  const EpsilonNfa bad_epsilon{0U, {false}, {{1U}}, {{{}}}};
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(bad_epsilon, 8U),
      std::invalid_argument);

  const EpsilonNfa budget{0U, {false, true}, {{}, {}}, {{{1U}}, {{1U}}}};
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(budget, 0U),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::automata::determinize_epsilon_nfa(budget, 1U),
      std::length_error);

  const EpsilonNfa zero{0U, {false, true}, {{1U}, {}}, {{}, {}}};
  const auto result = algorithms::automata::determinize_epsilon_nfa(zero, 4U);
  REQUIRE_EQ(result.dfa_state_subsets.size(), 1U);
  REQUIRE(result.dfa.transitions[0U].empty());
  REQUIRE(result.dfa.accepting[0U]);
  verify_determinized_nfa(zero, result);
}

TEST_CASE(nfa_determinization_epsilon_cycle_duplicates_and_dead_subset) {
  const EpsilonNfa nfa{
      0U,
      {false, false, true},
      {{1U}, {0U}, {}},
      {{{}, {}}, {{1U, 2U, 2U}, {}}, {{2U}, {2U}}},
  };
  const auto result = algorithms::automata::determinize_epsilon_nfa(nfa, 16U);
  verify_determinized_nfa(nfa, result);
  REQUIRE(!determinized_accepts(result, {}));
  REQUIRE(determinized_accepts(result, {0U}));
  REQUIRE(!determinized_accepts(result, {1U}));
  bool found_empty = false;
  for (const auto& subset : result.dfa_state_subsets) {
    found_empty = found_empty || subset.empty();
  }
  REQUIRE(found_empty);

  const auto repeated = algorithms::automata::determinize_epsilon_nfa(nfa, 16U);
  REQUIRE_EQ(result.dfa.transitions, repeated.dfa.transitions);
  REQUIRE_EQ(result.dfa.accepting, repeated.dfa.accepting);
  REQUIRE_EQ(result.dfa_state_subsets, repeated.dfa_state_subsets);
}

TEST_CASE(nfa_determinization_randomized_direct_simulation_and_dfa_minimization) {
  std::mt19937_64 rng(0xE51A5EEDULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t state_count =
        1U + static_cast<std::size_t>(rng() % 6U);
    const std::size_t alphabet_size = static_cast<std::size_t>(rng() % 4U);
    EpsilonNfa nfa;
    nfa.start_state = static_cast<std::size_t>(rng() % state_count);
    nfa.accepting.assign(state_count, false);
    nfa.epsilon_transitions.resize(state_count);
    nfa.transitions.resize(
        state_count,
        std::vector<std::vector<std::size_t>>(alphabet_size));

    for (std::size_t state = 0U; state < state_count; ++state) {
      nfa.accepting[state] = (rng() % 4U) == 0U;
      for (std::size_t target = 0U; target < state_count; ++target) {
        if ((rng() % 7U) == 0U) {
          nfa.epsilon_transitions[state].push_back(target);
          if ((rng() % 9U) == 0U) {
            nfa.epsilon_transitions[state].push_back(target);
          }
        }
      }
      for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
        for (std::size_t target = 0U; target < state_count; ++target) {
          if ((rng() % 6U) == 0U) {
            nfa.transitions[state][symbol].push_back(target);
            if ((rng() % 11U) == 0U) {
              nfa.transitions[state][symbol].push_back(target);
            }
          }
        }
      }
    }

    const auto result =
        algorithms::automata::determinize_epsilon_nfa(nfa, 128U);
    verify_determinized_nfa(nfa, result);
    const auto minimized = algorithms::automata::minimize_dfa(result.dfa);

    std::vector<std::vector<std::size_t>> words;
    enumerate_nfa_words(alphabet_size, 5U, words);
    for (const auto& word : words) {
      const bool expected = nfa_accepts_oracle(nfa, word);
      REQUIRE_EQ(determinized_accepts(result, word), expected);
      REQUIRE_EQ(run_word(minimized, minimized.start_state, word), expected);
    }
  }
}
