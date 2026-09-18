#pragma once

#include "algorithms/automata/earley_parser.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::automata {

inline constexpr std::size_t kSlrEndOfInput = 256U;

enum class SlrActionKind : std::uint8_t {
  Shift,
  Reduce,
  Accept,
};

struct SlrAction {
  SlrActionKind kind = SlrActionKind::Shift;
  std::size_t value = 0;

  friend bool operator==(const SlrAction&, const SlrAction&) = default;
};

struct SlrConflict {
  std::size_t state = 0;
  std::size_t lookahead = 0;  // byte 0..255 or kSlrEndOfInput
  SlrAction existing;
  SlrAction incoming;

  friend bool operator==(const SlrConflict&, const SlrConflict&) = default;
};

enum class SlrParseStepKind : std::uint8_t {
  Shift,
  Reduce,
};

struct SlrParseStep {
  SlrParseStepKind kind = SlrParseStepKind::Shift;
  std::size_t value = 0;  // shifted byte or normalized rule index

  friend bool operator==(const SlrParseStep&, const SlrParseStep&) = default;
};

struct SlrParseResult {
  bool accepted = false;
  std::size_t consumed_bytes = 0;
  std::vector<SlrParseStep> trace;

  friend bool operator==(const SlrParseResult&, const SlrParseResult&) = default;
};

struct SlrParseTable {
  GeneralCfgNonterminal start = 0;
  std::vector<GeneralCfgRule> rules;
  std::vector<std::array<std::optional<SlrAction>, kSlrEndOfInput + 1U>> actions;
  std::vector<std::vector<std::optional<std::size_t>>> gotos;
  std::vector<SlrConflict> conflicts;

  [[nodiscard]] bool conflict_free() const noexcept { return conflicts.empty(); }
  [[nodiscard]] std::size_t state_count() const noexcept { return actions.size(); }

  friend bool operator==(const SlrParseTable&, const SlrParseTable&) = default;
};

namespace slr_detail {

struct Item {
  std::size_t rule = 0;  // internal rule index, 0 is the augmented rule
  std::size_t dot = 0;

  friend bool operator==(const Item&, const Item&) = default;
  friend bool operator<(const Item& left, const Item& right) {
    return std::tie(left.rule, left.dot) < std::tie(right.rule, right.dot);
  }
};

struct InternalRule {
  std::size_t lhs = 0;  // augmented start uses nonterminal_count
  std::vector<GeneralCfgSymbol> rhs;
  std::optional<std::size_t> normalized_rule;
};

struct FirstFollow {
  std::vector<std::bitset<256U>> first;
  std::vector<bool> nullable;
  std::vector<std::bitset<kSlrEndOfInput + 1U>> follow;
};

inline FirstFollow compute_first_follow(
    const std::size_t nonterminal_count, const GeneralCfgNonterminal start,
    const std::vector<GeneralCfgRule>& rules) {
  FirstFollow result;
  result.first.resize(nonterminal_count);
  result.nullable.assign(nonterminal_count, false);
  result.follow.resize(nonterminal_count);

  bool changed = true;
  while (changed) {
    changed = false;
    for (const GeneralCfgRule& rule : rules) {
      bool whole_rhs_nullable = true;
      for (const GeneralCfgSymbol& symbol : rule.rhs) {
        if (symbol.kind == GeneralCfgSymbolKind::Terminal) {
          const std::size_t before = result.first[rule.lhs].count();
          result.first[rule.lhs].set(symbol.value);
          changed = changed || result.first[rule.lhs].count() != before;
          whole_rhs_nullable = false;
          break;
        }

        const std::bitset<256U> before = result.first[rule.lhs];
        result.first[rule.lhs] |= result.first[symbol.value];
        changed = changed || result.first[rule.lhs] != before;
        if (!result.nullable[symbol.value]) {
          whole_rhs_nullable = false;
          break;
        }
      }
      if (whole_rhs_nullable && !result.nullable[rule.lhs]) {
        result.nullable[rule.lhs] = true;
        changed = true;
      }
    }
  }

  result.follow[start].set(kSlrEndOfInput);
  changed = true;
  while (changed) {
    changed = false;
    for (const GeneralCfgRule& rule : rules) {
      for (std::size_t index = 0; index < rule.rhs.size(); ++index) {
        const GeneralCfgSymbol& symbol = rule.rhs[index];
        if (symbol.kind != GeneralCfgSymbolKind::Nonterminal) {
          continue;
        }

        std::bitset<kSlrEndOfInput + 1U> additions;
        bool suffix_nullable = true;
        for (std::size_t next = index + 1U; next < rule.rhs.size(); ++next) {
          const GeneralCfgSymbol& following = rule.rhs[next];
          if (following.kind == GeneralCfgSymbolKind::Terminal) {
            additions.set(following.value);
            suffix_nullable = false;
            break;
          }
          for (std::size_t byte = 0; byte < 256U; ++byte) {
            if (result.first[following.value].test(byte)) {
              additions.set(byte);
            }
          }
          if (!result.nullable[following.value]) {
            suffix_nullable = false;
            break;
          }
        }
        if (suffix_nullable) {
          additions |= result.follow[rule.lhs];
        }

        const auto before = result.follow[symbol.value];
        result.follow[symbol.value] |= additions;
        changed = changed || result.follow[symbol.value] != before;
      }
    }
  }

  return result;
}

inline std::vector<Item> closure(
    std::vector<Item> items, const std::vector<InternalRule>& rules,
    const std::vector<std::vector<std::size_t>>& rules_by_lhs) {
  std::set<Item> seen(items.begin(), items.end());
  std::size_t cursor = 0;
  while (cursor < items.size()) {
    const Item item = items[cursor++];
    const InternalRule& rule = rules[item.rule];
    if (item.dot >= rule.rhs.size()) {
      continue;
    }
    const GeneralCfgSymbol& next = rule.rhs[item.dot];
    if (next.kind != GeneralCfgSymbolKind::Nonterminal) {
      continue;
    }
    for (const std::size_t child_rule : rules_by_lhs[next.value]) {
      const Item child{child_rule, 0U};
      if (seen.insert(child).second) {
        items.push_back(child);
      }
    }
  }
  std::sort(items.begin(), items.end());
  return items;
}

inline std::vector<Item> goto_items(
    const std::vector<Item>& state, const GeneralCfgSymbol& symbol,
    const std::vector<InternalRule>& rules,
    const std::vector<std::vector<std::size_t>>& rules_by_lhs) {
  std::vector<Item> moved;
  for (const Item& item : state) {
    const InternalRule& rule = rules[item.rule];
    if (item.dot < rule.rhs.size() && rule.rhs[item.dot] == symbol) {
      moved.push_back(Item{item.rule, item.dot + 1U});
    }
  }
  if (moved.empty()) {
    return {};
  }
  return closure(std::move(moved), rules, rules_by_lhs);
}

inline void install_action(
    SlrParseTable& table, const std::size_t state, const std::size_t lookahead,
    const SlrAction action) {
  std::optional<SlrAction>& slot = table.actions[state][lookahead];
  if (!slot.has_value()) {
    slot = action;
    return;
  }
  if (*slot == action) {
    return;
  }
  const SlrConflict conflict{state, lookahead, *slot, action};
  if (std::find(table.conflicts.begin(), table.conflicts.end(), conflict) ==
      table.conflicts.end()) {
    table.conflicts.push_back(conflict);
  }
}

}  // namespace slr_detail

// Build the canonical LR(0) item automaton and then install SLR actions using
// FOLLOW sets. Grammar symbols and normalized rule ordering reuse the sealed
// general-CFG representation; recognition itself does not reuse Earley state.
// Conflicts are returned explicitly instead of resolved by precedence rules.
[[nodiscard]] inline SlrParseTable build_slr_parse_table(
    const GeneralCfgGrammar& grammar) {
  if (grammar.nonterminal_count == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error("SLR augmented nonterminal id overflows size_t");
  }
  const std::vector<GeneralCfgRule> normalized = earley_detail::normalize(grammar);
  const auto first_follow = slr_detail::compute_first_follow(
      grammar.nonterminal_count, grammar.start, normalized);

  std::vector<slr_detail::InternalRule> internal_rules;
  internal_rules.reserve(normalized.size() + 1U);
  internal_rules.push_back(slr_detail::InternalRule{
      grammar.nonterminal_count,
      {GeneralCfgSymbol::nonterminal(grammar.start)}, std::nullopt});
  for (std::size_t index = 0; index < normalized.size(); ++index) {
    internal_rules.push_back(
        slr_detail::InternalRule{normalized[index].lhs, normalized[index].rhs,
                                 index});
  }

  std::vector<std::vector<std::size_t>> rules_by_lhs(grammar.nonterminal_count);
  for (std::size_t internal = 1U; internal < internal_rules.size(); ++internal) {
    rules_by_lhs[internal_rules[internal].lhs].push_back(internal);
  }

  std::vector<std::vector<slr_detail::Item>> states;
  std::vector<std::vector<std::pair<GeneralCfgSymbol, std::size_t>>> transitions;
  std::map<std::vector<slr_detail::Item>, std::size_t> state_ids;

  std::vector<slr_detail::Item> start_state = slr_detail::closure(
      {slr_detail::Item{0U, 0U}}, internal_rules, rules_by_lhs);
  states.push_back(start_state);
  transitions.emplace_back();
  state_ids.emplace(start_state, 0U);

  for (std::size_t state = 0; state < states.size(); ++state) {
    std::vector<GeneralCfgSymbol> symbols;
    for (const slr_detail::Item& item : states[state]) {
      const slr_detail::InternalRule& rule = internal_rules[item.rule];
      if (item.dot < rule.rhs.size()) {
        const GeneralCfgSymbol symbol = rule.rhs[item.dot];
        if (std::find(symbols.begin(), symbols.end(), symbol) == symbols.end()) {
          symbols.push_back(symbol);
        }
      }
    }
    std::sort(symbols.begin(), symbols.end(), earley_detail::symbol_less);

    for (const GeneralCfgSymbol& symbol : symbols) {
      std::vector<slr_detail::Item> target = slr_detail::goto_items(
          states[state], symbol, internal_rules, rules_by_lhs);
      if (target.empty()) {
        continue;
      }
      auto [iterator, inserted] = state_ids.emplace(target, states.size());
      if (inserted) {
        states.push_back(target);
        transitions.emplace_back();
      }
      transitions[state].push_back({symbol, iterator->second});
    }
  }

  SlrParseTable table;
  table.start = grammar.start;
  table.rules = normalized;
  table.actions.resize(states.size());
  table.gotos.assign(
      states.size(),
      std::vector<std::optional<std::size_t>>(grammar.nonterminal_count));

  for (std::size_t state = 0; state < states.size(); ++state) {
    for (const auto& [symbol, target] : transitions[state]) {
      if (symbol.kind == GeneralCfgSymbolKind::Terminal) {
        slr_detail::install_action(
            table, state, symbol.value,
            SlrAction{SlrActionKind::Shift, target});
      } else {
        table.gotos[state][symbol.value] = target;
      }
    }

    for (const slr_detail::Item& item : states[state]) {
      const slr_detail::InternalRule& rule = internal_rules[item.rule];
      if (item.dot != rule.rhs.size()) {
        continue;
      }
      if (item.rule == 0U) {
        slr_detail::install_action(
            table, state, kSlrEndOfInput,
            SlrAction{SlrActionKind::Accept, 0U});
        continue;
      }

      const std::size_t normalized_rule = *rule.normalized_rule;
      for (std::size_t lookahead = 0; lookahead <= kSlrEndOfInput;
           ++lookahead) {
        if (first_follow.follow[rule.lhs].test(lookahead)) {
          slr_detail::install_action(
              table, state, lookahead,
              SlrAction{SlrActionKind::Reduce, normalized_rule});
        }
      }
    }
  }

  return table;
}

// Execute a conflict-free SLR table. Missing actions reject normally. A
// conflicted table is not silently resolved. Shift/reduce trace steps form a
// replayable derivation witness over table.rules.
[[nodiscard]] inline SlrParseResult slr_parse(const SlrParseTable& table,
                                              std::string_view input) {
  if (!table.conflict_free()) {
    throw std::logic_error("cannot execute a conflicted SLR parse table");
  }
  if (table.actions.empty() || table.gotos.empty()) {
    throw std::invalid_argument("SLR parse table has no states");
  }

  SlrParseResult result;
  std::vector<std::size_t> state_stack{0U};
  std::set<std::pair<std::size_t, std::vector<std::size_t>>> seen;
  std::size_t position = 0;

  while (true) {
    if (!seen.emplace(position, state_stack).second) {
      throw std::logic_error("SLR parser entered a non-progressing cycle");
    }

    const std::size_t state = state_stack.back();
    if (state >= table.actions.size()) {
      throw std::logic_error("SLR parser state is outside action table");
    }
    const std::size_t lookahead =
        position < input.size()
            ? static_cast<std::size_t>(
                  static_cast<unsigned char>(input[position]))
            : kSlrEndOfInput;
    const std::optional<SlrAction>& slot = table.actions[state][lookahead];
    if (!slot.has_value()) {
      result.consumed_bytes = position;
      return result;
    }

    const SlrAction action = *slot;
    if (action.kind == SlrActionKind::Shift) {
      if (position >= input.size() || action.value >= table.actions.size()) {
        throw std::logic_error("invalid SLR shift action");
      }
      result.trace.push_back(
          SlrParseStep{SlrParseStepKind::Shift, lookahead});
      state_stack.push_back(action.value);
      ++position;
      continue;
    }

    if (action.kind == SlrActionKind::Reduce) {
      if (action.value >= table.rules.size()) {
        throw std::logic_error("invalid SLR reduce action");
      }
      const GeneralCfgRule& rule = table.rules[action.value];
      if (rule.rhs.size() >= state_stack.size()) {
        throw std::logic_error("SLR reduce action underflows state stack");
      }
      state_stack.resize(state_stack.size() - rule.rhs.size());
      const std::size_t goto_source = state_stack.back();
      if (goto_source >= table.gotos.size() ||
          rule.lhs >= table.gotos[goto_source].size() ||
          !table.gotos[goto_source][rule.lhs].has_value()) {
        throw std::logic_error("SLR reduce action has no goto target");
      }
      const std::size_t goto_target = *table.gotos[goto_source][rule.lhs];
      if (goto_target >= table.actions.size()) {
        throw std::logic_error("SLR goto target is outside action table");
      }
      result.trace.push_back(
          SlrParseStep{SlrParseStepKind::Reduce, action.value});
      state_stack.push_back(goto_target);
      continue;
    }

    if (action.kind != SlrActionKind::Accept || position != input.size()) {
      throw std::logic_error("invalid SLR accept action");
    }
    result.accepted = true;
    result.consumed_bytes = position;
    return result;
  }
}

}  // namespace algorithms::automata
