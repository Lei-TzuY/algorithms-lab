#pragma once

namespace algorithms::automata {
namespace earley_detail {

struct Item {
  std::size_t rule = 0;
  std::size_t dot = 0;
  std::size_t origin = 0;

  friend bool operator<(const Item& a, const Item& b) {
    return std::tie(a.rule, a.dot, a.origin) <
           std::tie(b.rule, b.dot, b.origin);
  }
};

struct Column {
  std::vector<Item> items;
  std::set<Item> seen;

  bool add(Item item) {
    if (!seen.insert(item).second) return false;
    items.push_back(item);
    return true;
  }
};

inline bool symbol_less(const GeneralCfgSymbol& a, const GeneralCfgSymbol& b) {
  if (a.kind != b.kind) {
    return static_cast<std::uint8_t>(a.kind) <
           static_cast<std::uint8_t>(b.kind);
  }
  return a.value < b.value;
}

inline bool rule_less(const GeneralCfgRule& a, const GeneralCfgRule& b) {
  if (a.lhs != b.lhs) return a.lhs < b.lhs;
  return std::lexicographical_compare(a.rhs.begin(), a.rhs.end(), b.rhs.begin(),
                                      b.rhs.end(), symbol_less);
}

inline std::vector<GeneralCfgRule> normalize(const GeneralCfgGrammar& grammar) {
  if (grammar.nonterminal_count == 0U) {
    throw std::invalid_argument("Earley grammar must contain a nonterminal");
  }
  if (grammar.start >= grammar.nonterminal_count) {
    throw std::out_of_range("Earley start nonterminal is out of range");
  }

  std::vector<GeneralCfgRule> rules = grammar.rules;
  for (const GeneralCfgRule& rule : rules) {
    if (rule.lhs >= grammar.nonterminal_count) {
      throw std::out_of_range("Earley rule lhs is out of range");
    }
    for (const GeneralCfgSymbol& symbol : rule.rhs) {
      if (symbol.kind == GeneralCfgSymbolKind::Nonterminal) {
        if (symbol.value >= grammar.nonterminal_count) {
          throw std::out_of_range("Earley rule references invalid nonterminal");
        }
      } else if (symbol.kind == GeneralCfgSymbolKind::Terminal) {
        if (symbol.value > 255U) {
          throw std::out_of_range("Earley terminal is outside the byte alphabet");
        }
      } else {
        throw std::invalid_argument("Earley symbol kind is invalid");
      }
    }
  }

  std::sort(rules.begin(), rules.end(), rule_less);
  rules.erase(std::unique(rules.begin(), rules.end()), rules.end());
  return rules;
}

inline bool is_complete(const Item& item,
                        const std::vector<GeneralCfgRule>& rules) {
  return item.dot == rules[item.rule].rhs.size();
}

inline bool expects_nonterminal(const Item& item,
                                const std::vector<GeneralCfgRule>& rules,
                                GeneralCfgNonterminal nonterminal) {
  const GeneralCfgRule& rule = rules[item.rule];
  if (item.dot >= rule.rhs.size()) return false;
  const GeneralCfgSymbol& symbol = rule.rhs[item.dot];
  return symbol.kind == GeneralCfgSymbolKind::Nonterminal &&
         symbol.value == nonterminal;
}

}  // namespace earley_detail

}  // namespace algorithms::automata
