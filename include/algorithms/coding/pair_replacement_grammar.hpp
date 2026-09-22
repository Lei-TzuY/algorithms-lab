#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::coding {

using PairReplacementSymbol = std::uint32_t;
inline constexpr PairReplacementSymbol kPairReplacementAlphabetSize = 256U;

struct PairReplacementRule {
  PairReplacementSymbol left{};
  PairReplacementSymbol right{};

  friend bool operator==(const PairReplacementRule&,
                         const PairReplacementRule&) = default;
};

struct PairReplacementGrammar {
  std::vector<PairReplacementRule> rules;
  std::vector<PairReplacementSymbol> start;

  friend bool operator==(const PairReplacementGrammar&,
                         const PairReplacementGrammar&) = default;
};

namespace pair_replacement_detail {

[[nodiscard]] inline std::uint8_t to_byte(const char value) noexcept {
  return static_cast<std::uint8_t>(static_cast<unsigned char>(value));
}

[[nodiscard]] inline char to_char(const std::uint8_t value) noexcept {
  return static_cast<char>(value);
}

[[nodiscard]] inline bool is_terminal(
    const PairReplacementSymbol symbol) noexcept {
  return symbol < kPairReplacementAlphabetSize;
}

[[nodiscard]] inline bool valid_rule_reference(
    const PairReplacementSymbol symbol, const std::size_t rule_index) noexcept {
  if (is_terminal(symbol)) {
    return true;
  }
  const std::uint64_t referenced =
      static_cast<std::uint64_t>(symbol) - kPairReplacementAlphabetSize;
  return referenced < rule_index;
}

[[nodiscard]] inline bool valid_start_reference(
    const PairReplacementSymbol symbol, const std::size_t rule_count) noexcept {
  if (is_terminal(symbol)) {
    return true;
  }
  const std::uint64_t referenced =
      static_cast<std::uint64_t>(symbol) - kPairReplacementAlphabetSize;
  return referenced < rule_count;
}

[[nodiscard]] inline std::size_t symbol_length(
    const PairReplacementSymbol symbol,
    const std::vector<std::size_t>& rule_lengths,
    const std::size_t available_rules) {
  if (is_terminal(symbol)) {
    return 1U;
  }
  const std::uint64_t referenced =
      static_cast<std::uint64_t>(symbol) - kPairReplacementAlphabetSize;
  if (referenced >= available_rules) {
    throw std::invalid_argument(
        "pair-replacement grammar has a forward or unknown rule reference");
  }
  return rule_lengths[static_cast<std::size_t>(referenced)];
}

struct LengthAnalysis {
  std::vector<std::size_t> rule_lengths;
  std::size_t output_size{};
};

[[nodiscard]] inline LengthAnalysis analyze_lengths(
    const PairReplacementGrammar& grammar) {
  LengthAnalysis analysis;
  analysis.rule_lengths.resize(grammar.rules.size(), 0U);

  for (std::size_t index = 0U; index < grammar.rules.size(); ++index) {
    const PairReplacementRule& rule = grammar.rules[index];
    if (!valid_rule_reference(rule.left, index) ||
        !valid_rule_reference(rule.right, index)) {
      throw std::invalid_argument(
          "pair-replacement rule must reference only terminals or earlier rules");
    }

    const std::size_t left =
        symbol_length(rule.left, analysis.rule_lengths, index);
    const std::size_t right =
        symbol_length(rule.right, analysis.rule_lengths, index);
    if (left > std::numeric_limits<std::size_t>::max() - right) {
      throw std::length_error(
          "pair-replacement expanded rule length overflows size_t");
    }
    analysis.rule_lengths[index] = left + right;
  }

  for (const PairReplacementSymbol symbol : grammar.start) {
    if (!valid_start_reference(symbol, grammar.rules.size())) {
      throw std::invalid_argument(
          "pair-replacement start sequence references an unknown rule");
    }
    const std::size_t length =
        symbol_length(symbol, analysis.rule_lengths, grammar.rules.size());
    if (analysis.output_size >
        std::numeric_limits<std::size_t>::max() - length) {
      throw std::length_error(
          "pair-replacement decoded output length overflows size_t");
    }
    analysis.output_size += length;
  }

  return analysis;
}

[[nodiscard]] inline std::size_t count_non_overlapping(
    const std::vector<PairReplacementSymbol>& sequence,
    const std::pair<PairReplacementSymbol, PairReplacementSymbol>& pair) {
  std::size_t count = 0U;
  std::size_t index = 0U;
  while (index + 1U < sequence.size()) {
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

}  // namespace pair_replacement_detail

// Deterministic pair-replacement grammar over arbitrary bytes.
//
// At each round every adjacent symbol pair currently present is considered.
// The pair with the greatest left-to-right non-overlapping occurrence count is
// selected; ties choose the lexicographically smaller pair. A rule is created
// only when that count is at least two. All selected occurrences are then
// replaced left-to-right by the new nonterminal.
//
// New rule i receives symbol 256+i and can therefore reference only terminals
// or older nonterminals. The returned rule graph is acyclic by construction.
//
// This is a deterministic educational pair-replacement grammar, not a claim of
// optimal grammar size, canonical Re-Pair compatibility, or bit-packed output.
[[nodiscard]] inline PairReplacementGrammar pair_replacement_encode_bytes(
    const std::string_view input) {
  PairReplacementGrammar grammar;
  std::vector<PairReplacementSymbol> sequence;
  sequence.reserve(input.size());
  for (const char value : input) {
    sequence.push_back(
        static_cast<PairReplacementSymbol>(
            pair_replacement_detail::to_byte(value)));
  }

  while (sequence.size() >= 2U) {
    std::set<std::pair<PairReplacementSymbol, PairReplacementSymbol>>
        candidates;
    for (std::size_t index = 0U; index + 1U < sequence.size(); ++index) {
      candidates.emplace(sequence[index], sequence[index + 1U]);
    }

    std::pair<PairReplacementSymbol, PairReplacementSymbol> best{};
    std::size_t best_count = 0U;
    bool have_best = false;
    for (const auto& candidate : candidates) {
      const std::size_t count =
          pair_replacement_detail::count_non_overlapping(sequence, candidate);
      if (!have_best || count > best_count) {
        best = candidate;
        best_count = count;
        have_best = true;
      }
    }

    if (!have_best || best_count < 2U) {
      break;
    }

    constexpr std::uint64_t max_symbol =
        std::numeric_limits<PairReplacementSymbol>::max();
    if (grammar.rules.size() >
        static_cast<std::size_t>(max_symbol -
                                 kPairReplacementAlphabetSize)) {
      throw std::length_error(
          "pair-replacement nonterminal symbol space exhausted");
    }
    const PairReplacementSymbol nonterminal =
        static_cast<PairReplacementSymbol>(
            kPairReplacementAlphabetSize + grammar.rules.size());
    grammar.rules.push_back(PairReplacementRule{best.first, best.second});

    std::vector<PairReplacementSymbol> replaced;
    replaced.reserve(sequence.size() - best_count);
    std::size_t index = 0U;
    while (index < sequence.size()) {
      if (index + 1U < sequence.size() &&
          sequence[index] == best.first &&
          sequence[index + 1U] == best.second) {
        replaced.push_back(nonterminal);
        index += 2U;
      } else {
        replaced.push_back(sequence[index]);
        ++index;
      }
    }
    sequence = std::move(replaced);
  }

  grammar.start = std::move(sequence);
  return grammar;
}

[[nodiscard]] inline std::string pair_replacement_decode_bytes(
    const PairReplacementGrammar& grammar) {
  const auto analysis = pair_replacement_detail::analyze_lengths(grammar);

  std::string output;
  if (analysis.output_size > output.max_size()) {
    throw std::length_error(
        "pair-replacement decoded output exceeds string max_size");
  }
  output.reserve(analysis.output_size);

  std::vector<PairReplacementSymbol> stack;
  stack.reserve(grammar.start.size());
  for (auto it = grammar.start.rbegin(); it != grammar.start.rend(); ++it) {
    stack.push_back(*it);
  }

  while (!stack.empty()) {
    const PairReplacementSymbol symbol = stack.back();
    stack.pop_back();

    if (pair_replacement_detail::is_terminal(symbol)) {
      output.push_back(pair_replacement_detail::to_char(
          static_cast<std::uint8_t>(symbol)));
      continue;
    }

    const std::size_t rule_index = static_cast<std::size_t>(
        static_cast<std::uint64_t>(symbol) -
        kPairReplacementAlphabetSize);
    if (rule_index >= grammar.rules.size()) {
      throw std::invalid_argument(
          "pair-replacement expansion references an unknown rule");
    }
    const PairReplacementRule& rule = grammar.rules[rule_index];
    stack.push_back(rule.right);
    stack.push_back(rule.left);
  }

  return output;
}

[[nodiscard]] inline bool valid_pair_replacement_grammar(
    const PairReplacementGrammar& grammar) noexcept {
  try {
    static_cast<void>(pair_replacement_detail::analyze_lengths(grammar));
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace algorithms::coding
