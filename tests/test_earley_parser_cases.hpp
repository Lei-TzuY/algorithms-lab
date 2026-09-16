#pragma once

#include "earley_parser_test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using earley_test_support::EarleyParseResult;
using earley_test_support::GeneralCfgGrammar;
using earley_test_support::GeneralCfgSymbol;
using earley_test_support::GeneralCfgSymbolKind;
using earley_test_support::ch;
using earley_test_support::earley_recognize;
using earley_test_support::nt;

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
