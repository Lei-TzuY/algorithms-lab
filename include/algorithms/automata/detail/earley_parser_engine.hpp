#pragma once

namespace algorithms::automata {

// Exact recognition for a byte-oriented context-free grammar. Rules may have
// arbitrary RHS length and may contain epsilon productions, unit productions,
// left recursion, and arbitrary byte terminals. Duplicate rules are accepted
// and canonicalized. The returned chart diagnostics are deterministic for a
// fixed grammar and input.
//
// Let Q be the number of dotted rule positions after canonicalization. This
// direct educational implementation stores O(n^2 Q) Earley items and uses a
// conservative O(n^3 Q^2) worst-case bound for its explicit completion / nullable
// catch-up scans (O(n^3) when the grammar is fixed). It makes no parse-forest,
// ambiguity-counting, error-recovery, incremental, or specialized
// unambiguous-grammar complexity claim.
[[nodiscard]] inline EarleyParseResult earley_recognize(
    const GeneralCfgGrammar& grammar, std::string_view input) {
  using earley_detail::Column;
  using earley_detail::Item;

  const std::vector<GeneralCfgRule> rules = earley_detail::normalize(grammar);
  const std::size_t n = input.size();
  if (n == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("Earley chart size overflows size_t");
  }

  std::vector<std::vector<std::size_t>> rules_by_lhs(grammar.nonterminal_count);
  for (std::size_t rule_index = 0; rule_index < rules.size(); ++rule_index) {
    rules_by_lhs[rules[rule_index].lhs].push_back(rule_index);
  }

  std::vector<Column> chart(n + 1U);
  for (const std::size_t rule_index : rules_by_lhs[grammar.start]) {
    chart[0].add(Item{rule_index, 0U, 0U});
  }

  for (std::size_t position = 0; position <= n; ++position) {
    std::size_t cursor = 0;
    while (cursor < chart[position].items.size()) {
      const Item item = chart[position].items[cursor++];
      const GeneralCfgRule& rule = rules[item.rule];

      if (earley_detail::is_complete(item, rules)) {
        const GeneralCfgNonterminal completed = rule.lhs;
        const std::size_t origin_count = chart[item.origin].items.size();
        for (std::size_t index = 0; index < origin_count; ++index) {
          const Item waiting = chart[item.origin].items[index];
          if (earley_detail::expects_nonterminal(waiting, rules, completed)) {
            chart[position].add(
                Item{waiting.rule, waiting.dot + 1U, waiting.origin});
          }
        }
        continue;
      }

      const GeneralCfgSymbol& next = rule.rhs[item.dot];
      if (next.kind == GeneralCfgSymbolKind::Terminal) {
        if (position < n &&
            next.value == static_cast<std::size_t>(
                              static_cast<unsigned char>(input[position]))) {
          chart[position + 1U].add(Item{item.rule, item.dot + 1U, item.origin});
        }
        continue;
      }

      const GeneralCfgNonterminal expected = next.value;
      for (const std::size_t predicted_rule : rules_by_lhs[expected]) {
        chart[position].add(Item{predicted_rule, 0U, position});
      }

      // A nullable completion can be processed before this waiter is inserted
      // into the same column. Catch up to such an already-completed state so
      // epsilon/unit/left-recursive grammars reach the least chart fixed point.
      const std::size_t current_count = chart[position].items.size();
      for (std::size_t index = 0; index < current_count; ++index) {
        const Item completed_item = chart[position].items[index];
        if (completed_item.origin != position ||
            !earley_detail::is_complete(completed_item, rules) ||
            rules[completed_item.rule].lhs != expected) {
          continue;
        }
        chart[position].add(Item{item.rule, item.dot + 1U, item.origin});
        break;
      }
    }
  }

  EarleyParseResult result;
  result.chart_item_counts.reserve(chart.size());
  for (const Column& column : chart) {
    result.chart_item_counts.push_back(column.items.size());
    if (column.items.size() >
        std::numeric_limits<std::size_t>::max() - result.total_items) {
      throw std::length_error("Earley item count overflows size_t");
    }
    result.total_items += column.items.size();
  }

  for (const Item& item : chart[n].items) {
    if (item.origin == 0U && earley_detail::is_complete(item, rules) &&
        rules[item.rule].lhs == grammar.start) {
      result.accepted = true;
      break;
    }
  }
  return result;
}

}  // namespace algorithms::automata
