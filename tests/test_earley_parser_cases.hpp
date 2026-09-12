#pragma once

#include "algorithms/automata/earley_parser.hpp"
#include "algorithms/automata/cyk_parser.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using algorithms::automata::CnfBinaryRule;
using algorithms::automata::CnfGrammar;
using algorithms::automata::CnfTerminalRule;
using algorithms::automata::EarleyParseResult;
using algorithms::automata::cyk_parse;
using algorithms::automata::GeneralCfgGrammar;
using algorithms::automata::GeneralCfgRule;
using algorithms::automata::GeneralCfgSymbol;
using algorithms::automata::GeneralCfgSymbolKind;
using algorithms::automata::earley_recognize;

namespace {
GeneralCfgSymbol nt(std::size_t id) { return GeneralCfgSymbol::nonterminal(id); }
GeneralCfgSymbol ch(std::uint8_t byte) { return GeneralCfgSymbol::terminal(byte); }

bool fixed_point_oracle(const GeneralCfgGrammar& grammar, std::string_view input) {
  const std::size_t n = input.size();
  const std::size_t stride = n + 1U;
  std::vector<std::uint8_t> derives(grammar.nonterminal_count * stride * stride, 0U);
  const auto index = [stride](std::size_t nonterminal, std::size_t begin,
                              std::size_t end) {
    return (nonterminal * stride + begin) * stride + end;
  };
  bool changed = true;
  while (changed) {
    changed = false;
    for (const GeneralCfgRule& rule : grammar.rules) {
      for (std::size_t begin = 0; begin <= n; ++begin) {
        std::vector<std::uint8_t> positions(stride, 0U);
        positions[begin] = 1U;
        for (const GeneralCfgSymbol& symbol : rule.rhs) {
          std::vector<std::uint8_t> next(stride, 0U);
          for (std::size_t position = 0; position <= n; ++position) {
            if (positions[position] == 0U) continue;
            if (symbol.kind == GeneralCfgSymbolKind::Terminal) {
              if (position < n && symbol.value == static_cast<std::size_t>(
                  static_cast<unsigned char>(input[position]))) {
                next[position + 1U] = 1U;
              }
              continue;
            }
            for (std::size_t end = position; end <= n; ++end) {
              if (derives[index(symbol.value, position, end)] != 0U) next[end] = 1U;
            }
          }
          positions.swap(next);
        }
        for (std::size_t end = begin; end <= n; ++end) {
          if (positions[end] == 0U) continue;
          auto& cell = derives[index(rule.lhs, begin, end)];
          if (cell == 0U) { cell = 1U; changed = true; }
        }
      }
    }
  }
  return derives[index(grammar.start, 0U, n)] != 0U;
}
}  // namespace

TEST_CASE(earley_general_cfg_deterministic_semantics) {
  GeneralCfgGrammar sequence{2U, 0U, {{0U, {ch('a'), nt(1U)}}, {1U, {ch('b')}}}};
  REQUIRE(earley_recognize(sequence, "ab").accepted);
  REQUIRE(!earley_recognize(sequence, "a").accepted);

  GeneralCfgGrammar nullable{2U, 0U, {{0U, {nt(1U)}}, {1U, {}}}};
  REQUIRE(earley_recognize(nullable, "").accepted);

  GeneralCfgGrammar left_recursive{1U, 0U, {{0U, {nt(0U), ch('a')}}, {0U, {}}}};
  REQUIRE(earley_recognize(left_recursive, "aaaa").accepted);
  REQUIRE(!earley_recognize(left_recursive, "b").accepted);

  GeneralCfgGrammar unit_cycle{2U, 0U,
      {{0U, {nt(1U)}}, {1U, {nt(0U)}}, {1U, {ch('x')}}}};
  REQUIRE(earley_recognize(unit_cycle, "x").accepted);

  // B -> epsilon completes before C -> .B is introduced. A recognizer that
  // only advances waiters present at completion time misses this empty parse.
  GeneralCfgGrammar late_nullable{4U, 0U,
      {{0U, {nt(1U)}}, {1U, {nt(2U), nt(3U)}}, {2U, {}}, {3U, {nt(2U)}}}};
  REQUIRE(earley_recognize(late_nullable, "").accepted);
}

TEST_CASE(earley_arbitrary_bytes_duplicates_and_replay) {
  const std::string bytes{"\0\xFF", 2};
  GeneralCfgGrammar grammar{1U, 0U,
      {{0U, {ch(0U), ch(255U)}}, {0U, {ch(0U), ch(255U)}}}};
  const EarleyParseResult first = earley_recognize(grammar, bytes);
  const EarleyParseResult second = earley_recognize(grammar, bytes);
  REQUIRE(first.accepted);
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first.chart_item_counts.size(), bytes.size() + 1U);
}

TEST_CASE(earley_validation) {
  REQUIRE_THROWS_AS(earley_recognize(GeneralCfgGrammar{}, ""), std::invalid_argument);
  REQUIRE_THROWS_AS(earley_recognize(GeneralCfgGrammar{1U, 1U, {}}, ""), std::out_of_range);
  REQUIRE_THROWS_AS(earley_recognize(GeneralCfgGrammar{1U, 0U, {{1U, {}}}}, ""), std::out_of_range);
  GeneralCfgGrammar bad_nt{1U, 0U, {{0U, {GeneralCfgSymbol::nonterminal(1U)}}}};
  REQUIRE_THROWS_AS(earley_recognize(bad_nt, ""), std::out_of_range);
  GeneralCfgGrammar bad_terminal{1U, 0U,
      {{0U, {GeneralCfgSymbol{GeneralCfgSymbolKind::Terminal, 256U}}}}};
  REQUIRE_THROWS_AS(earley_recognize(bad_terminal, ""), std::out_of_range);
  GeneralCfgGrammar bad_kind{1U, 0U,
      {{0U, {GeneralCfgSymbol{static_cast<GeneralCfgSymbolKind>(99U), 0U}}}}};
  REQUIRE_THROWS_AS(earley_recognize(bad_kind, ""), std::invalid_argument);
}
