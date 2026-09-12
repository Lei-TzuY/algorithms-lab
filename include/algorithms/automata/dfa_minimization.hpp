#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::automata {

struct Dfa {
  std::size_t start_state{};
  std::vector<std::vector<std::size_t>> transitions;
  std::vector<bool> accepting;
};

struct MinimizedDfa {
  std::size_t start_state{};
  std::vector<std::vector<std::size_t>> transitions;
  std::vector<bool> accepting;
  std::vector<std::optional<std::size_t>> original_to_minimized;
  std::vector<std::vector<std::size_t>> minimized_to_original;
};

// Minimize the language reachable from `dfa.start_state` using Hopcroft
// partition refinement. Unreachable original states map to std::nullopt.
//
// The quotient is numbered canonically by BFS from the minimized start state,
// following symbols in ascending index order. The minimized start state is
// therefore always 0.
//
// Preconditions: at least one state, accepting.size()==state_count, every
// transition row has the same alphabet size, and every transition target and
// start_state are in range. The alphabet may be empty.
[[nodiscard]] MinimizedDfa minimize_dfa(const Dfa& dfa);

}  // namespace algorithms::automata
