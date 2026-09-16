#pragma once

#include "algorithms/automata/cyk_parser.hpp"
#include "algorithms/automata/earley_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace earley_test_support {

using algorithms::automata::CnfBinaryRule;
using algorithms::automata::CnfGrammar;
using algorithms::automata::CnfTerminalRule;
using algorithms::automata::EarleyParseResult;
using algorithms::automata::GeneralCfgGrammar;
using algorithms::automata::GeneralCfgRule;
using algorithms::automata::GeneralCfgSymbol;
using algorithms::automata::GeneralCfgSymbolKind;
using algorithms::automata::cyk_parse;
using algorithms::automata::earley_recognize;

inline GeneralCfgSymbol nt(std::size_t id) {
  return GeneralCfgSymbol::nonterminal(id);
}

inline GeneralCfgSymbol ch(std::uint8_t byte) {
  return GeneralCfgSymbol::terminal(byte);
}

inline bool fixed_point_oracle(const GeneralCfgGrammar& grammar,
                               std::string_view input) {
  const std::size_t n = input.size();
  const std::size_t stride = n + 1U;
  std::vector<std::uint8_t> derives(grammar.nonterminal_count * stride * stride,
                                    0U);
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
            if (positions[position] == 0U) {
              continue;
            }
            if (symbol.kind == GeneralCfgSymbolKind::Terminal) {
              if (position < n &&
                  symbol.value == static_cast<std::size_t>(
                                      static_cast<unsigned char>(input[position]))) {
                next[position + 1U] = 1U;
              }
              continue;
            }
            for (std::size_t end = position; end <= n; ++end) {
              if (derives[index(symbol.value, position, end)] != 0U) {
                next[end] = 1U;
              }
            }
          }
          positions.swap(next);
        }
        for (std::size_t end = begin; end <= n; ++end) {
          if (positions[end] == 0U) {
            continue;
          }
          auto& cell = derives[index(rule.lhs, begin, end)];
          if (cell == 0U) {
            cell = 1U;
            changed = true;
          }
        }
      }
    }
  }

  return derives[index(grammar.start, 0U, n)] != 0U;
}

}  // namespace earley_test_support
