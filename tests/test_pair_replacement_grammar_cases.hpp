#pragma once

#include "algorithms/coding/pair_replacement_grammar.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pair_replacement_grammar_test_detail {

using algorithms::coding::PairReplacementGrammar;
using algorithms::coding::PairReplacementRule;
using algorithms::coding::PairReplacementSymbol;
using algorithms::coding::kPairReplacementAlphabetSize;
using algorithms::coding::pair_replacement_decode_bytes;
using algorithms::coding::pair_replacement_encode_bytes;
using algorithms::coding::valid_pair_replacement_grammar;

inline char byte_char(const unsigned value) {
  return static_cast<char>(static_cast<unsigned char>(value));
}

inline std::size_t brute_count(
    const std::vector<PairReplacementSymbol>& sequence,
    const std::pair<PairReplacementSymbol, PairReplacementSymbol>& pair) {
  std::size_t count = 0U;
  for (std::size_t index = 0U; index + 1U < sequence.size();) {
    if (sequence[index] == pair.first &&
        sequence[index + 1U] == pair.second) {
      ++count;
      index += 2U;
    } else {
      ++index;
    }
  }
  return count;
}

// Independent slow selector: candidate pairs are enumerated directly from every
// position, duplicate candidates are intentionally reconsidered, and the best
// pair is selected by brute-force rescans rather than the production set walk.
inline PairReplacementGrammar brute_force_pair_replacement(
    const std::string_view input) {
  PairReplacementGrammar result;
  std::vector<PairReplacementSymbol> sequence;
  for (const char value : input) {
    sequence.push_back(static_cast<PairReplacementSymbol>(
        static_cast<unsigned char>(value)));
  }

  while (sequence.size() >= 2U) {
    std::pair<PairReplacementSymbol, PairReplacementSymbol> best{};
    std::size_t best_count = 0U;
    bool have_best = false;

    for (std::size_t position = 0U; position + 1U < sequence.size();
         ++position) {
      const auto candidate =
          std::make_pair(sequence[position], sequence[position + 1U]);
      const std::size_t count = brute_count(sequence, candidate);
      if (!have_best || count > best_count ||
          (count == best_count && candidate < best)) {
        best = candidate;
        best_count = count;
        have_best = true;
      }
    }

    if (!have_best || best_count < 2U) {
      break;
    }

    const PairReplacementSymbol nonterminal =
        static_cast<PairReplacementSymbol>(
            kPairReplacementAlphabetSize + result.rules.size());
    result.rules.push_back(PairReplacementRule{best.first, best.second});

    std::vector<PairReplacementSymbol> next;
    for (std::size_t index = 0U; index < sequence.size();) {
      if (index + 1U < sequence.size() &&
          sequence[index] == best.first &&
          sequence[index + 1U] == best.second) {
        next.push_back(nonterminal);
        index += 2U;
      } else {
        next.push_back(sequence[index]);
        ++index;
      }
    }
    sequence = std::move(next);
  }

  result.start = std::move(sequence);
  return result;
}

}  // namespace pair_replacement_grammar_test_detail

TEST_CASE(pair_replacement_grammar_empty_literal_and_known_repetition) {
  using namespace pair_replacement_grammar_test_detail;

  const auto empty = pair_replacement_encode_bytes("");
  REQUIRE(empty.rules.empty());
  REQUIRE(empty.start.empty());
  REQUIRE(valid_pair_replacement_grammar(empty));
  REQUIRE(pair_replacement_decode_bytes(empty).empty());

  const auto literal = pair_replacement_encode_bytes("abc");
  REQUIRE(literal.rules.empty());
  REQUIRE(literal.start ==
          std::vector<PairReplacementSymbol>({97U, 98U, 99U}));
  REQUIRE(pair_replacement_decode_bytes(literal) == "abc");

  const auto repeated = pair_replacement_encode_bytes("abababab");
  const PairReplacementGrammar expected{
      {{97U, 98U}, {256U, 256U}},
      {257U, 257U}};
  REQUIRE(repeated == expected);
  REQUIRE(valid_pair_replacement_grammar(repeated));
  REQUIRE(pair_replacement_decode_bytes(repeated) == "abababab");
}

TEST_CASE(pair_replacement_grammar_uses_nonoverlap_count_and_lexicographic_ties) {
  using namespace pair_replacement_grammar_test_detail;

  const auto same = pair_replacement_encode_bytes("aaaa");
  REQUIRE(same.rules ==
          std::vector<PairReplacementRule>({{97U, 97U}}));
  REQUIRE(same.start ==
          std::vector<PairReplacementSymbol>({256U, 256U}));

  const auto tied = pair_replacement_encode_bytes("ababcdcd");
  REQUIRE(!tied.rules.empty());
  REQUIRE((tied.rules.front() == PairReplacementRule{97U, 98U}));
  REQUIRE(pair_replacement_decode_bytes(tied) == "ababcdcd");
}

TEST_CASE(pair_replacement_grammar_round_trips_arbitrary_bytes) {
  using namespace pair_replacement_grammar_test_detail;

  std::string input;
  for (std::size_t repeat = 0U; repeat < 64U; ++repeat) {
    input.push_back(byte_char(0x00U));
    input.push_back(byte_char(0xffU));
    input.push_back(byte_char(0x80U));
    input.push_back(byte_char(0x00U));
  }

  const auto grammar = pair_replacement_encode_bytes(input);
  REQUIRE(valid_pair_replacement_grammar(grammar));
  REQUIRE(pair_replacement_decode_bytes(grammar) == input);
  REQUIRE(grammar == brute_force_pair_replacement(input));
}

TEST_CASE(pair_replacement_grammar_rejects_forward_unknown_and_overflowing_forms) {
  using namespace pair_replacement_grammar_test_detail;

  PairReplacementGrammar self_reference{{{256U, 65U}}, {256U}};
  REQUIRE(!valid_pair_replacement_grammar(self_reference));
  REQUIRE_THROWS_AS(pair_replacement_decode_bytes(self_reference),
                    std::invalid_argument);

  PairReplacementGrammar forward_reference{
      {{65U, 66U}, {258U, 67U}}, {257U}};
  REQUIRE(!valid_pair_replacement_grammar(forward_reference));
  REQUIRE_THROWS_AS(pair_replacement_decode_bytes(forward_reference),
                    std::invalid_argument);

  PairReplacementGrammar unknown_start{{{65U, 66U}}, {999999U}};
  REQUIRE(!valid_pair_replacement_grammar(unknown_start));
  REQUIRE_THROWS_AS(pair_replacement_decode_bytes(unknown_start),
                    std::invalid_argument);

  PairReplacementGrammar overflowing;
  overflowing.rules.push_back(PairReplacementRule{0U, 0U});
  for (std::size_t index = 1U;
       index < std::numeric_limits<std::size_t>::digits; ++index) {
    const PairReplacementSymbol previous =
        static_cast<PairReplacementSymbol>(
            kPairReplacementAlphabetSize + index - 1U);
    overflowing.rules.push_back(
        PairReplacementRule{previous, previous});
  }
  overflowing.start.push_back(static_cast<PairReplacementSymbol>(
      kPairReplacementAlphabetSize + overflowing.rules.size() - 1U));
  REQUIRE(!valid_pair_replacement_grammar(overflowing));
  REQUIRE_THROWS_AS(pair_replacement_decode_bytes(overflowing),
                    std::length_error);
}

TEST_CASE(pair_replacement_grammar_randomized_matches_bruteforce_selector) {
  using namespace pair_replacement_grammar_test_detail;

  std::mt19937_64 random(0xA17E5EEDULL);
  for (std::size_t trial = 0U; trial < 1800U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(random() % 73U);
    const unsigned alphabet = (trial & 1U) == 0U ? 5U : 256U;

    std::string input;
    input.reserve(length);
    for (std::size_t index = 0U; index < length; ++index) {
      input.push_back(byte_char(
          static_cast<unsigned>(random() % alphabet)));
    }

    const auto production = pair_replacement_encode_bytes(input);
    const auto oracle = brute_force_pair_replacement(input);
    REQUIRE(production == oracle);
    REQUIRE(valid_pair_replacement_grammar(production));
    REQUIRE(pair_replacement_decode_bytes(production) == input);
    REQUIRE(pair_replacement_encode_bytes(input) == production);
  }
}
