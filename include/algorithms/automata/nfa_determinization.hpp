#pragma once

#include "algorithms/automata/dfa_minimization.hpp"

#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::automata {

// Nondeterministic finite automaton over the explicit alphabet represented by
// each transition row. Epsilon transitions consume no symbol. Duplicate target
// entries are allowed and have no semantic effect.
struct EpsilonNfa {
  std::size_t start_state{};
  std::vector<bool> accepting;
  std::vector<std::vector<std::size_t>> epsilon_transitions;
  std::vector<std::vector<std::vector<std::size_t>>> transitions;
};

struct DeterminizedNfa {
  Dfa dfa;
  // Sorted NFA-state subset represented by each canonical DFA state.
  std::vector<std::vector<std::size_t>> dfa_state_subsets;
};

namespace nfa_determinization_detail {

using State = std::size_t;
using Subset = std::vector<State>;

inline void validate(const EpsilonNfa& nfa) {
  const std::size_t state_count = nfa.accepting.size();
  if (state_count == 0U) {
    throw std::invalid_argument("epsilon-NFA must contain at least one state");
  }
  if (nfa.start_state >= state_count) {
    throw std::invalid_argument("epsilon-NFA start state is out of range");
  }
  if (nfa.epsilon_transitions.size() != state_count ||
      nfa.transitions.size() != state_count) {
    throw std::invalid_argument("epsilon-NFA state-vector size mismatch");
  }

  const std::size_t alphabet_size = nfa.transitions.front().size();
  for (State state = 0U; state < state_count; ++state) {
    if (nfa.transitions[state].size() != alphabet_size) {
      throw std::invalid_argument("epsilon-NFA transition rows must have equal size");
    }
    for (const State target : nfa.epsilon_transitions[state]) {
      if (target >= state_count) {
        throw std::invalid_argument("epsilon transition target is out of range");
      }
    }
    for (const auto& destinations : nfa.transitions[state]) {
      for (const State target : destinations) {
        if (target >= state_count) {
          throw std::invalid_argument("symbol transition target is out of range");
        }
      }
    }
  }
}

inline Subset epsilon_closure(const EpsilonNfa& nfa, const Subset& seeds) {
  const std::size_t state_count = nfa.accepting.size();
  std::vector<unsigned char> seen(state_count, 0U);
  std::vector<State> queue;
  queue.reserve(state_count);

  for (const State seed : seeds) {
    if (seen[seed] == 0U) {
      seen[seed] = 1U;
      queue.push_back(seed);
    }
  }
  for (std::size_t head = 0U; head < queue.size(); ++head) {
    for (const State next : nfa.epsilon_transitions[queue[head]]) {
      if (seen[next] == 0U) {
        seen[next] = 1U;
        queue.push_back(next);
      }
    }
  }

  Subset result;
  result.reserve(queue.size());
  for (State state = 0U; state < state_count; ++state) {
    if (seen[state] != 0U) {
      result.push_back(state);
    }
  }
  return result;
}

inline Subset move_then_close(const EpsilonNfa& nfa, const Subset& subset,
                              std::size_t symbol) {
  const std::size_t state_count = nfa.accepting.size();
  std::vector<unsigned char> selected(state_count, 0U);
  Subset seeds;
  for (const State state : subset) {
    for (const State next : nfa.transitions[state][symbol]) {
      if (selected[next] == 0U) {
        selected[next] = 1U;
        seeds.push_back(next);
      }
    }
  }
  return epsilon_closure(nfa, seeds);
}

inline bool accepts(const EpsilonNfa& nfa, const Subset& subset) {
  for (const State state : subset) {
    if (nfa.accepting[state]) {
      return true;
    }
  }
  return false;
}

}  // namespace nfa_determinization_detail

// Construct a complete DFA for the NFA language through epsilon-closure subset
// construction. DFA states are numbered canonically by BFS from the initial
// epsilon closure, visiting symbols in ascending order. The empty subset is an
// ordinary rejecting dead state when reachable.
//
// max_dfa_states is an explicit resident-state budget. It must be positive and
// determinization throws std::length_error before adding a state that would
// exceed the budget. The subset construction can require exponentially many
// DFA states in the number of NFA states.
[[nodiscard]] inline DeterminizedNfa determinize_epsilon_nfa(
    const EpsilonNfa& nfa, std::size_t max_dfa_states) {
  using nfa_determinization_detail::Subset;

  nfa_determinization_detail::validate(nfa);
  if (max_dfa_states == 0U) {
    throw std::invalid_argument("max_dfa_states must be positive");
  }

  DeterminizedNfa result;
  result.dfa.start_state = 0U;
  const std::size_t alphabet_size = nfa.transitions.front().size();
  const Subset initial = nfa_determinization_detail::epsilon_closure(
      nfa, Subset{nfa.start_state});

  std::map<Subset, std::size_t> id_of;
  id_of.emplace(initial, 0U);
  result.dfa_state_subsets.push_back(initial);
  result.dfa.accepting.push_back(
      nfa_determinization_detail::accepts(nfa, initial));
  result.dfa.transitions.emplace_back(alphabet_size, 0U);

  for (std::size_t head = 0U; head < result.dfa_state_subsets.size(); ++head) {
    for (std::size_t symbol = 0U; symbol < alphabet_size; ++symbol) {
      Subset next = nfa_determinization_detail::move_then_close(
          nfa, result.dfa_state_subsets[head], symbol);
      auto found = id_of.find(next);
      if (found == id_of.end()) {
        if (result.dfa_state_subsets.size() >= max_dfa_states) {
          throw std::length_error(
              "epsilon-NFA determinization state budget exceeded");
        }
        const std::size_t new_id = result.dfa_state_subsets.size();
        auto inserted = id_of.emplace(next, new_id);
        found = inserted.first;
        result.dfa_state_subsets.push_back(std::move(next));
        result.dfa.accepting.push_back(nfa_determinization_detail::accepts(
            nfa, result.dfa_state_subsets.back()));
        result.dfa.transitions.emplace_back(alphabet_size, 0U);
      }
      result.dfa.transitions[head][symbol] = found->second;
    }
  }

  return result;
}

}  // namespace algorithms::automata
