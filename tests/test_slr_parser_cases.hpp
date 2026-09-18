#pragma once

#include "algorithms/automata/earley_parser.hpp"
#include "algorithms/automata/slr_parser.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace slr_parser_test_detail {

using algorithms::automata::GeneralCfgGrammar;
using algorithms::automata::GeneralCfgRule;
using algorithms::automata::GeneralCfgSymbol;
using algorithms::automata::GeneralCfgSymbolKind;
using algorithms::automata::SlrActionKind;
using algorithms::automata::SlrParseResult;
using algorithms::automata::SlrParseStepKind;
using algorithms::automata::SlrParseTable;
using algorithms::automata::build_slr_parse_table;
using algorithms::automata::earley_recognize;
using algorithms::automata::slr_parse;

[[nodiscard]] inline GeneralCfgSymbol nt(std::size_t id) {
  return GeneralCfgSymbol::nonterminal(id);
}

[[nodiscard]] inline GeneralCfgSymbol ch(std::uint8_t byte) {
  return GeneralCfgSymbol::terminal(byte);
}

inline void replay_accepted_trace(const SlrParseTable& table,
                                  std::string_view input,
                                  const SlrParseResult& result) {
  REQUIRE(result.accepted);
  REQUIRE_EQ(result.consumed_bytes, input.size());

  std::vector<GeneralCfgSymbol> symbols;
  std::size_t input_position = 0;
  for (const auto& step : result.trace) {
    if (step.kind == SlrParseStepKind::Shift) {
      REQUIRE(input_position < input.size());
      const auto byte = static_cast<std::uint8_t>(
          static_cast<unsigned char>(input[input_position]));
      REQUIRE_EQ(step.value, static_cast<std::size_t>(byte));
      symbols.push_back(ch(byte));
      ++input_position;
      continue;
    }

    REQUIRE(step.kind == SlrParseStepKind::Reduce);
    REQUIRE(step.value < table.rules.size());
    const GeneralCfgRule& rule = table.rules[step.value];
    REQUIRE(rule.rhs.size() <= symbols.size());
    const std::size_t begin = symbols.size() - rule.rhs.size();
    for (std::size_t index = 0; index < rule.rhs.size(); ++index) {
      REQUIRE(symbols[begin + index] == rule.rhs[index]);
    }
    symbols.resize(begin);
    symbols.push_back(nt(rule.lhs));
  }

  REQUIRE_EQ(input_position, input.size());
  REQUIRE_EQ(symbols.size(), 1U);
  REQUIRE(symbols.front() == nt(table.start));
}

struct TinyDfa {
  std::size_t state_count = 0;
  std::vector<std::array<std::optional<std::size_t>, 3U>> transitions;
  std::vector<bool> accepting;
};

[[nodiscard]] inline GeneralCfgGrammar grammar_from_dfa(const TinyDfa& dfa) {
  GeneralCfgGrammar grammar;
  grammar.nonterminal_count = dfa.state_count;
  grammar.start = 0U;
  for (std::size_t state = 0; state < dfa.state_count; ++state) {
    if (dfa.accepting[state]) {
      grammar.rules.push_back(GeneralCfgRule{state, {}});
    }
    for (std::size_t byte = 0; byte < 3U; ++byte) {
      if (dfa.transitions[state][byte].has_value()) {
        grammar.rules.push_back(
            GeneralCfgRule{state,
                           {ch(static_cast<std::uint8_t>(byte)),
                            nt(*dfa.transitions[state][byte])}});
      }
    }
  }
  return grammar;
}

[[nodiscard]] inline bool dfa_accepts(const TinyDfa& dfa,
                                      std::string_view input) {
  std::size_t state = 0U;
  for (const char raw : input) {
    const std::size_t byte = static_cast<std::size_t>(
        static_cast<unsigned char>(raw));
    if (byte >= 3U || !dfa.transitions[state][byte].has_value()) {
      return false;
    }
    state = *dfa.transitions[state][byte];
  }
  return dfa.accepting[state];
}

}  // namespace slr_parser_test_detail

TEST_CASE(slr_parser_expression_grammar_and_replayable_trace) {
  using namespace slr_parser_test_detail;
  GeneralCfgGrammar grammar;
  grammar.nonterminal_count = 3U;  // E, T, F
  grammar.start = 0U;
  grammar.rules = {
      {0U, {nt(0U), ch('+'), nt(1U)}},
      {0U, {nt(1U)}},
      {1U, {nt(1U), ch('*'), nt(2U)}},
      {1U, {nt(2U)}},
      {2U, {ch('('), nt(0U), ch(')')}},
      {2U, {ch('n')}},
  };

  const SlrParseTable table = build_slr_parse_table(grammar);
  REQUIRE(table.conflict_free());
  REQUIRE(table.state_count() > 0U);
  REQUIRE(table == build_slr_parse_table(grammar));

  for (const std::string_view input : {std::string_view("n+n*n"),
                                       std::string_view("(n+n)*n"),
                                       std::string_view("n")}) {
    const SlrParseResult result = slr_parse(table, input);
    REQUIRE(result.accepted);
    replay_accepted_trace(table, input, result);
    REQUIRE(earley_recognize(grammar, input).accepted);
  }

  for (const std::string_view input : {std::string_view("n+*n"),
                                       std::string_view("(n"),
                                       std::string_view("+n")}) {
    REQUIRE(!slr_parse(table, input).accepted);
    REQUIRE(!earley_recognize(grammar, input).accepted);
  }
}

TEST_CASE(slr_parser_epsilon_and_arbitrary_byte_semantics) {
  using namespace slr_parser_test_detail;

  const GeneralCfgGrammar star_a{
      2U,
      0U,
      {{0U, {nt(1U)}}, {1U, {ch('a'), nt(1U)}}, {1U, {}}},
  };
  const SlrParseTable star_table = build_slr_parse_table(star_a);
  REQUIRE(star_table.conflict_free());
  for (const std::string_view input : {std::string_view(""),
                                       std::string_view("a"),
                                       std::string_view("aaaa")}) {
    const SlrParseResult result = slr_parse(star_table, input);
    REQUIRE(result.accepted);
    replay_accepted_trace(star_table, input, result);
  }
  REQUIRE(!slr_parse(star_table, "b").accepted);

  const GeneralCfgGrammar bytes{
      1U,
      0U,
      {{0U, {ch(0x00U), ch(0xffU)}}},
  };
  const SlrParseTable byte_table = build_slr_parse_table(bytes);
  REQUIRE(byte_table.conflict_free());
  const std::string payload{"\x00\xff", 2U};
  const SlrParseResult result = slr_parse(byte_table, payload);
  REQUIRE(result.accepted);
  replay_accepted_trace(byte_table, payload, result);
  REQUIRE(!slr_parse(byte_table, std::string("\x00", 1U)).accepted);
}

TEST_CASE(slr_parser_conflicts_are_explicit_and_never_silently_resolved) {
  using namespace slr_parser_test_detail;

  const GeneralCfgGrammar shift_reduce{
      1U,
      0U,
      {{0U, {nt(0U), nt(0U)}}, {0U, {ch('a')}}},
  };
  const SlrParseTable shift_reduce_table = build_slr_parse_table(shift_reduce);
  REQUIRE(!shift_reduce_table.conflict_free());
  bool found_shift_reduce = false;
  for (const auto& conflict : shift_reduce_table.conflicts) {
    if ((conflict.existing.kind == SlrActionKind::Shift &&
         conflict.incoming.kind == SlrActionKind::Reduce) ||
        (conflict.existing.kind == SlrActionKind::Reduce &&
         conflict.incoming.kind == SlrActionKind::Shift)) {
      found_shift_reduce = true;
    }
  }
  REQUIRE(found_shift_reduce);
  REQUIRE_THROWS_AS(slr_parse(shift_reduce_table, "aa"), std::logic_error);

  const GeneralCfgGrammar reduce_reduce{
      3U,
      0U,
      {{0U, {nt(1U)}}, {0U, {nt(2U)}}, {1U, {ch('a')}}, {2U, {ch('a')}}},
  };
  const SlrParseTable reduce_reduce_table = build_slr_parse_table(reduce_reduce);
  REQUIRE(!reduce_reduce_table.conflict_free());
  bool found_reduce_reduce = false;
  for (const auto& conflict : reduce_reduce_table.conflicts) {
    if (conflict.existing.kind == SlrActionKind::Reduce &&
        conflict.incoming.kind == SlrActionKind::Reduce) {
      found_reduce_reduce = true;
    }
  }
  REQUIRE(found_reduce_reduce);
  REQUIRE_THROWS_AS(slr_parse(reduce_reduce_table, "a"), std::logic_error);
}

TEST_CASE(slr_parser_validation_and_empty_language) {
  using namespace slr_parser_test_detail;

  REQUIRE_THROWS_AS(build_slr_parse_table(GeneralCfgGrammar{}),
                    std::invalid_argument);

  GeneralCfgGrammar invalid_symbol;
  invalid_symbol.nonterminal_count = 1U;
  invalid_symbol.start = 0U;
  invalid_symbol.rules = {
      {0U, {GeneralCfgSymbol{GeneralCfgSymbolKind::Terminal, 256U}}}};
  REQUIRE_THROWS_AS(build_slr_parse_table(invalid_symbol), std::out_of_range);

  const GeneralCfgGrammar empty_language{1U, 0U, {}};
  const SlrParseTable table = build_slr_parse_table(empty_language);
  REQUIRE(table.conflict_free());
  REQUIRE(!slr_parse(table, "").accepted);
  REQUIRE(!slr_parse(table, "x").accepted);
}

TEST_CASE(slr_parser_random_right_linear_grammars_vs_dfa_and_earley) {
  using namespace slr_parser_test_detail;

  std::mt19937_64 rng(0x51A7BEEF1234ULL);
  for (std::size_t trial = 0; trial < 300U; ++trial) {
    TinyDfa dfa;
    dfa.state_count = 1U + static_cast<std::size_t>(rng() % 4U);
    dfa.transitions.resize(dfa.state_count);
    dfa.accepting.resize(dfa.state_count);

    for (std::size_t state = 0; state < dfa.state_count; ++state) {
      dfa.accepting[state] = (rng() % 3U) == 0U;
      for (std::size_t byte = 0; byte < 3U; ++byte) {
        if ((rng() % 4U) != 0U) {
          dfa.transitions[state][byte] =
              static_cast<std::size_t>(rng() % dfa.state_count);
        }
      }
    }

    const GeneralCfgGrammar grammar = grammar_from_dfa(dfa);
    const SlrParseTable table = build_slr_parse_table(grammar);
    REQUIRE(table.conflict_free());
    REQUIRE(table == build_slr_parse_table(grammar));

    for (std::size_t sample = 0; sample < 25U; ++sample) {
      const std::size_t length = static_cast<std::size_t>(rng() % 8U);
      std::string input(length, '\0');
      for (char& byte : input) {
        byte = static_cast<char>(rng() % 4U);
      }

      const bool expected = dfa_accepts(dfa, input);
      const SlrParseResult result = slr_parse(table, input);
      REQUIRE_EQ(result.accepted, expected);
      REQUIRE_EQ(earley_recognize(grammar, input).accepted, expected);
      if (result.accepted) {
        replay_accepted_trace(table, input, result);
      }
    }
  }
}
