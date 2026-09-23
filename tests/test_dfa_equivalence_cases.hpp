#pragma once

#include "algorithms/automata/dfa_equivalence.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::automata::Dfa;
using algorithms::automata::DfaEquivalenceResult;
using algorithms::automata::compare_dfa_languages;

namespace dfa_equivalence_test_detail {

inline bool accepts_word(
    const Dfa& dfa, const std::vector<std::size_t>& word) {
  std::size_t state = dfa.start_state;
  for (const std::size_t symbol : word) {
    state = dfa.transitions[state][symbol];
  }
  return dfa.accepting[state];
}

// Independent tiny-instance oracle.
//
// It deliberately does not maintain product-state visited information.
// Instead it enumerates every word in increasing length and lexicographic
// symbol order. If two complete DFAs with n and m states are inequivalent,
// a shortest distinguishing word has length at most n*m-1 because a shortest
// path to an acceptance-disagreeing product state is simple.
inline std::optional<std::vector<std::size_t>>
brute_shortest_distinguishing_word(
    const Dfa& first, const Dfa& second) {
  const std::size_t alphabet_size =
      first.transitions.front().size();
  const std::size_t max_length =
      first.transitions.size() * second.transitions.size() - 1U;

  std::vector<std::vector<std::size_t>> frontier(1U);
  for (std::size_t length = 0U;
       length <= max_length; ++length) {
    for (const auto& word : frontier) {
      if (accepts_word(first, word) !=
          accepts_word(second, word)) {
        return word;
      }
    }

    if (length == max_length || alphabet_size == 0U) {
      break;
    }

    std::vector<std::vector<std::size_t>> next;
    next.reserve(frontier.size() * alphabet_size);
    for (const auto& word : frontier) {
      for (std::size_t symbol = 0U;
           symbol < alphabet_size; ++symbol) {
        auto extended = word;
        extended.push_back(symbol);
        next.push_back(std::move(extended));
      }
    }
    frontier = std::move(next);
  }
  return std::nullopt;
}

inline Dfa random_dfa(
    std::mt19937_64& random,
    const std::size_t state_count,
    const std::size_t alphabet_size) {
  Dfa dfa;
  dfa.start_state =
      static_cast<std::size_t>(random() % state_count);
  dfa.transitions.assign(
      state_count,
      std::vector<std::size_t>(alphabet_size, 0U));
  dfa.accepting.assign(state_count, false);

  for (std::size_t state = 0U;
       state < state_count; ++state) {
    dfa.accepting[state] = (random() & 1ULL) != 0ULL;
    for (std::size_t symbol = 0U;
         symbol < alphabet_size; ++symbol) {
      dfa.transitions[state][symbol] =
          static_cast<std::size_t>(random() % state_count);
    }
  }
  return dfa;
}

}  // namespace dfa_equivalence_test_detail

TEST_CASE(dfa_equivalence_identical_and_empty_alphabet) {
  const Dfa identical{
      0U,
      {
          {1U, 0U},
          {1U, 0U},
      },
      {false, true}};

  const DfaEquivalenceResult same =
      compare_dfa_languages(identical, identical, 4U);
  REQUIRE(same.equivalent);
  REQUIRE(same.distinguishing_word.empty());
  REQUIRE(same.explored_product_states >= 1U);
  REQUIRE(same.explored_product_states <= 4U);

  const Dfa empty_alphabet_reject{
      0U,
      {{}},
      {false}};
  const Dfa empty_alphabet_accept{
      0U,
      {{}},
      {true}};

  const auto equal_empty =
      compare_dfa_languages(
          empty_alphabet_reject,
          empty_alphabet_reject, 1U);
  REQUIRE(equal_empty.equivalent);
  REQUIRE_EQ(equal_empty.explored_product_states, 1U);

  const auto epsilon_witness =
      compare_dfa_languages(
          empty_alphabet_reject,
          empty_alphabet_accept, 1U);
  REQUIRE(!epsilon_witness.equivalent);
  REQUIRE(epsilon_witness.distinguishing_word.empty());
  REQUIRE_EQ(epsilon_witness.explored_product_states, 1U);
}

TEST_CASE(dfa_equivalence_returns_shortest_lexicographic_witness) {
  // Both one-symbol words distinguish, so symbol 0 must win.
  const Dfa first{
      0U,
      {
          {1U, 2U},
          {1U, 1U},
          {2U, 2U},
      },
      {false, true, true}};
  const Dfa second{
      0U,
      {
          {0U, 0U},
      },
      {false}};

  const auto result =
      compare_dfa_languages(first, second, 3U);
  REQUIRE(!result.equivalent);
  REQUIRE(result.distinguishing_word ==
          std::vector<std::size_t>{0U});
  REQUIRE(
      dfa_equivalence_test_detail::accepts_word(
          first, result.distinguishing_word) !=
      dfa_equivalence_test_detail::accepts_word(
          second, result.distinguishing_word));
}

TEST_CASE(dfa_equivalence_reconstructs_deeper_shortest_witness) {
  const Dfa first{
      0U,
      {
          {1U, 0U},
          {1U, 2U},
          {2U, 2U},
      },
      {false, false, true}};
  const Dfa second{
      0U,
      {
          {1U, 0U},
          {1U, 1U},
      },
      {false, false}};

  const auto result =
      compare_dfa_languages(first, second, 6U);
  REQUIRE(!result.equivalent);
  REQUIRE(result.distinguishing_word ==
          std::vector<std::size_t>{0U, 1U});
  REQUIRE(
      dfa_equivalence_test_detail::accepts_word(
          first, result.distinguishing_word) !=
      dfa_equivalence_test_detail::accepts_word(
          second, result.distinguishing_word));
}

TEST_CASE(dfa_equivalence_ignores_unreachable_state_differences) {
  const Dfa first{
      0U,
      {
          {0U},
          {1U},
      },
      {false, true}};
  const Dfa second{
      0U,
      {
          {0U},
      },
      {false}};

  const auto result =
      compare_dfa_languages(first, second, 2U);
  REQUIRE(result.equivalent);
  REQUIRE(result.distinguishing_word.empty());
  REQUIRE_EQ(result.explored_product_states, 1U);
}

TEST_CASE(dfa_equivalence_enforces_product_state_budget) {
  const Dfa two_cycle{
      0U,
      {
          {1U},
          {0U},
      },
      {false, false}};
  const Dfa one_loop{
      0U,
      {
          {0U},
      },
      {false}};

  REQUIRE_THROWS_AS(
      compare_dfa_languages(two_cycle, one_loop, 1U),
      std::length_error);

  const auto enough =
      compare_dfa_languages(two_cycle, one_loop, 2U);
  REQUIRE(enough.equivalent);
  REQUIRE_EQ(enough.explored_product_states, 2U);
}

TEST_CASE(dfa_equivalence_rejects_malformed_or_mismatched_dfas) {
  const Dfa valid{
      0U,
      {
          {0U},
      },
      {false}};

  const Dfa empty;
  REQUIRE_THROWS_AS(
      compare_dfa_languages(empty, valid, 1U),
      std::invalid_argument);

  const Dfa accepting_mismatch{
      0U,
      {
          {0U},
      },
      {}};
  REQUIRE_THROWS_AS(
      compare_dfa_languages(
          accepting_mismatch, valid, 1U),
      std::invalid_argument);

  const Dfa bad_start{
      1U,
      {
          {0U},
      },
      {false}};
  REQUIRE_THROWS_AS(
      compare_dfa_languages(bad_start, valid, 1U),
      std::invalid_argument);

  const Dfa ragged{
      0U,
      {
          {0U},
          {0U, 1U},
      },
      {false, false}};
  REQUIRE_THROWS_AS(
      compare_dfa_languages(ragged, valid, 2U),
      std::invalid_argument);

  const Dfa bad_target{
      0U,
      {
          {1U},
      },
      {false}};
  REQUIRE_THROWS_AS(
      compare_dfa_languages(bad_target, valid, 1U),
      std::invalid_argument);

  const Dfa alphabet_two{
      0U,
      {
          {0U, 0U},
      },
      {false}};
  REQUIRE_THROWS_AS(
      compare_dfa_languages(valid, alphabet_two, 1U),
      std::invalid_argument);

  REQUIRE_THROWS_AS(
      compare_dfa_languages(valid, valid, 0U),
      std::invalid_argument);
}

TEST_CASE(dfa_equivalence_random_tiny_matches_word_enumeration) {
  using namespace dfa_equivalence_test_detail;

  std::mt19937_64 random(0xDFAE011A1EULL);
  for (std::size_t trial = 0U;
       trial < 800U; ++trial) {
    const std::size_t first_states =
        1U + static_cast<std::size_t>(random() % 3U);
    const std::size_t second_states =
        1U + static_cast<std::size_t>(random() % 3U);
    const std::size_t alphabet_size =
        static_cast<std::size_t>(random() % 3U);

    const Dfa first =
        random_dfa(random, first_states, alphabet_size);
    const Dfa second =
        random_dfa(random, second_states, alphabet_size);

    const auto expected =
        brute_shortest_distinguishing_word(first, second);
    const std::size_t full_product =
        first_states * second_states;
    const auto actual =
        compare_dfa_languages(
            first, second, full_product);

    REQUIRE_EQ(actual.equivalent, !expected.has_value());
    REQUIRE(actual.explored_product_states >= 1U);
    REQUIRE(actual.explored_product_states <= full_product);

    if (expected.has_value()) {
      REQUIRE(!actual.equivalent);
      REQUIRE(actual.distinguishing_word == *expected);
      REQUIRE(
          accepts_word(first, actual.distinguishing_word) !=
          accepts_word(second, actual.distinguishing_word));
    } else {
      REQUIRE(actual.distinguishing_word.empty());
    }
  }
}
