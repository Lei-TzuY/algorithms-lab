#include "algorithms/strings/aho_corasick.hpp"

#include <algorithm>
#include <queue>

namespace algorithms::strings {

AhoCorasickByteMatcher::State::State() { next.fill(kNoState); }

AhoCorasickByteMatcher::AhoCorasickByteMatcher(
    const std::vector<std::string>& patterns) {
  states_.emplace_back();
  pattern_lengths_.reserve(patterns.size());

  for (std::size_t pattern_index = 0; pattern_index < patterns.size();
       ++pattern_index) {
    const std::string& pattern = patterns[pattern_index];
    pattern_lengths_.push_back(pattern.size());

    std::size_t state = 0;
    for (const char character : pattern) {
      const auto symbol = static_cast<unsigned char>(character);
      const std::size_t symbol_index = static_cast<std::size_t>(symbol);
      if (states_[state].next[symbol_index] == kNoState) {
        states_[state].next[symbol_index] = states_.size();
        states_.emplace_back();
      }
      state = states_[state].next[symbol_index];
    }
    states_[state].terminal_pattern_indices.push_back(pattern_index);
  }

  std::queue<std::size_t> pending;
  for (std::size_t symbol = 0; symbol < kAlphabetSize; ++symbol) {
    std::size_t& transition = states_[0].next[symbol];
    if (transition == kNoState) {
      transition = 0;
      continue;
    }
    states_[transition].failure = 0;
    states_[transition].output_link =
        states_[0].terminal_pattern_indices.empty() ? kNoState : 0;
    pending.push(transition);
  }

  while (!pending.empty()) {
    const std::size_t state = pending.front();
    pending.pop();

    for (std::size_t symbol = 0; symbol < kAlphabetSize; ++symbol) {
      std::size_t& transition = states_[state].next[symbol];
      if (transition == kNoState) {
        transition = states_[states_[state].failure].next[symbol];
        continue;
      }

      const std::size_t failure = states_[states_[state].failure].next[symbol];
      states_[transition].failure = failure;
      states_[transition].output_link =
          states_[failure].terminal_pattern_indices.empty()
              ? states_[failure].output_link
              : failure;
      pending.push(transition);
    }
  }
}

std::size_t AhoCorasickByteMatcher::pattern_count() const noexcept {
  return pattern_lengths_.size();
}

std::size_t AhoCorasickByteMatcher::state_count() const noexcept {
  return states_.size();
}

void AhoCorasickByteMatcher::emit_state_matches(
    std::size_t state, std::size_t end,
    std::vector<AhoCorasickMatch>& matches) const {
  std::size_t output_state = state;
  while (output_state != kNoState) {
    for (const std::size_t pattern_index :
         states_[output_state].terminal_pattern_indices) {
      const std::size_t length = pattern_lengths_[pattern_index];
      matches.push_back(AhoCorasickMatch{pattern_index, end - length, end});
    }
    output_state = states_[output_state].output_link;
  }
}

std::vector<AhoCorasickMatch> AhoCorasickByteMatcher::find_all(
    std::string_view text) const {
  std::vector<AhoCorasickMatch> matches;

  if (!states_[0].terminal_pattern_indices.empty()) {
    emit_state_matches(0, 0, matches);
  }

  std::size_t state = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto symbol = static_cast<unsigned char>(text[index]);
    state = states_[state].next[static_cast<std::size_t>(symbol)];
    emit_state_matches(state, index + 1, matches);
  }
  return matches;
}

}  // namespace algorithms::strings
