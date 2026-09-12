#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace algorithms::automata {

using GeneralCfgNonterminal = std::size_t;

enum class GeneralCfgSymbolKind : std::uint8_t {
  Nonterminal,
  Terminal,
};

struct GeneralCfgSymbol {
  GeneralCfgSymbolKind kind = GeneralCfgSymbolKind::Nonterminal;
  std::size_t value = 0;

  [[nodiscard]] static GeneralCfgSymbol nonterminal(GeneralCfgNonterminal id) {
    return GeneralCfgSymbol{GeneralCfgSymbolKind::Nonterminal, id};
  }

  [[nodiscard]] static GeneralCfgSymbol terminal(std::uint8_t byte) {
    return GeneralCfgSymbol{GeneralCfgSymbolKind::Terminal,
                            static_cast<std::size_t>(byte)};
  }

  friend bool operator==(const GeneralCfgSymbol&, const GeneralCfgSymbol&) = default;
};

struct GeneralCfgRule {
  GeneralCfgNonterminal lhs = 0;
  std::vector<GeneralCfgSymbol> rhs;

  friend bool operator==(const GeneralCfgRule&, const GeneralCfgRule&) = default;
};

struct GeneralCfgGrammar {
  std::size_t nonterminal_count = 0;
  GeneralCfgNonterminal start = 0;
  std::vector<GeneralCfgRule> rules;
};

struct EarleyParseResult {
  bool accepted = false;
  std::vector<std::size_t> chart_item_counts;
  std::size_t total_items = 0;

  friend bool operator==(const EarleyParseResult&, const EarleyParseResult&) = default;
};

}  // namespace algorithms::automata

#include "algorithms/automata/detail/earley_parser_grammar.hpp"
#include "algorithms/automata/detail/earley_parser_engine.hpp"
