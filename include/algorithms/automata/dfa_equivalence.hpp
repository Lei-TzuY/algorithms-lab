#pragma once

#include "algorithms/automata/dfa_minimization.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::automata {

struct DfaEquivalenceResult {
  bool equivalent{};
  // Symbol indices forming the shortest distinguishing word when inequivalent.
  // The empty word is a valid witness when the start states disagree.
  std::vector<std::size_t> distinguishing_word;
  // Number of reachable product states materialized before the result was known.
  std::size_t explored_product_states{};
};

namespace dfa_equivalence_detail {

inline std::size_t validate_complete_dfa(const Dfa& dfa) {
  const std::size_t state_count = dfa.transitions.size();
  if (state_count == 0U) {
    throw std::invalid_argument("DFA must contain at least one state");
  }
  if (dfa.accepting.size() != state_count) {
    throw std::invalid_argument("DFA accepting vector size mismatch");
  }
  if (dfa.start_state >= state_count) {
    throw std::invalid_argument("DFA start state is out of range");
  }

  const std::size_t alphabet_size = dfa.transitions.front().size();
  for (const auto& row : dfa.transitions) {
    if (row.size() != alphabet_size) {
      throw std::invalid_argument(
          "DFA transition rows must have equal size");
    }
    for (const std::size_t target : row) {
      if (target >= state_count) {
        throw std::invalid_argument(
            "DFA transition target is out of range");
      }
    }
  }
  return alphabet_size;
}

}  // namespace dfa_equivalence_detail

// Compare the languages of two complete DFAs over the same explicit alphabet.
//
// The search is BFS on the reachable product automaton. Symbols are expanded in
// ascending index order, so the first acceptance-disagreeing product state is
// reached by a shortest distinguishing word and, among shortest witnesses, the
// lexicographically smallest one.
//
// max_product_states is a resident-state budget over reachable product states.
// It must be positive. The function throws std::length_error before adding a
// state that would exceed the budget.
//
// Malformed DFAs or mismatched alphabet sizes are rejected with
// std::invalid_argument.
[[nodiscard]] inline DfaEquivalenceResult compare_dfa_languages(
    const Dfa& first, const Dfa& second,
    const std::size_t max_product_states) {
  const std::size_t first_alphabet =
      dfa_equivalence_detail::validate_complete_dfa(first);
  const std::size_t second_alphabet =
      dfa_equivalence_detail::validate_complete_dfa(second);
  if (first_alphabet != second_alphabet) {
    throw std::invalid_argument("DFA alphabet sizes must match");
  }
  if (max_product_states == 0U) {
    throw std::invalid_argument(
        "max_product_states must be positive");
  }

  using ProductState = std::pair<std::size_t, std::size_t>;
  constexpr std::size_t kNoPredecessor =
      std::numeric_limits<std::size_t>::max();

  const ProductState start{first.start_state, second.start_state};
  std::map<ProductState, std::size_t> id_of;
  std::vector<ProductState> states;
  std::vector<std::size_t> predecessor;
  std::vector<std::size_t> via_symbol;

  id_of.emplace(start, 0U);
  states.push_back(start);
  predecessor.push_back(kNoPredecessor);
  via_symbol.push_back(0U);

  const auto reconstruct =
      [&](std::size_t id) {
        std::vector<std::size_t> reversed;
        while (predecessor[id] != kNoPredecessor) {
          reversed.push_back(via_symbol[id]);
          id = predecessor[id];
        }
        std::reverse(reversed.begin(), reversed.end());
        return reversed;
      };

  if (first.accepting[start.first] != second.accepting[start.second]) {
    return DfaEquivalenceResult{false, {}, 1U};
  }

  for (std::size_t head = 0U; head < states.size(); ++head) {
    const auto [left, right] = states[head];
    for (std::size_t symbol = 0U;
         symbol < first_alphabet; ++symbol) {
      const ProductState next{
          first.transitions[left][symbol],
          second.transitions[right][symbol]};

      if (id_of.find(next) != id_of.end()) {
        continue;
      }
      if (states.size() >= max_product_states) {
        throw std::length_error(
            "DFA product-state budget exceeded");
      }

      const std::size_t next_id = states.size();
      id_of.emplace(next, next_id);
      states.push_back(next);
      predecessor.push_back(head);
      via_symbol.push_back(symbol);

      if (first.accepting[next.first] !=
          second.accepting[next.second]) {
        return DfaEquivalenceResult{
            false, reconstruct(next_id), states.size()};
      }
    }
  }

  return DfaEquivalenceResult{true, {}, states.size()};
}

}  // namespace algorithms::automata
