#include "algorithms/strings/suffix_automaton.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::strings {
namespace {

std::size_t checked_add(std::size_t left, std::size_t right,
                        const char* message) {
  if (right > std::numeric_limits<std::size_t>::max() - left) {
    throw std::overflow_error(message);
  }
  return left + right;
}

}  // namespace

SuffixAutomatonByteIndex::SuffixAutomatonByteIndex(std::string_view text)
    : text_length_(text.size()) {
  if (text.size() > (std::numeric_limits<std::size_t>::max() - 1U) / 2U) {
    throw std::length_error("suffix automaton text is too large");
  }

  states_.reserve(checked_add(text.size() * 2U, 1U,
                              "suffix automaton state capacity overflow"));
  states_.push_back(State{});

  for (const char raw_byte : text) {
    extend(static_cast<unsigned char>(raw_byte));
  }
  finalize_occurrences_and_distinct_count();
}

void SuffixAutomatonByteIndex::extend(unsigned char byte) {
  const std::size_t current = states_.size();
  states_.push_back(State{});
  states_[current].length = checked_add(states_[last_].length, 1U,
                                        "suffix automaton length overflow");
  states_[current].occurrences = 1U;

  std::size_t cursor = last_;
  while (cursor != kNoState && !states_[cursor].next.contains(byte)) {
    states_[cursor].next.emplace(byte, current);
    cursor = states_[cursor].link;
  }

  if (cursor == kNoState) {
    states_[current].link = 0U;
    last_ = current;
    return;
  }

  const std::size_t target = states_[cursor].next.at(byte);
  const std::size_t expected_length =
      checked_add(states_[cursor].length, 1U,
                  "suffix automaton transition length overflow");
  if (expected_length == states_[target].length) {
    states_[current].link = target;
    last_ = current;
    return;
  }

  const std::size_t clone = states_.size();
  states_.push_back(states_[target]);
  states_[clone].length = expected_length;
  states_[clone].occurrences = 0U;

  while (cursor != kNoState) {
    const auto transition = states_[cursor].next.find(byte);
    if (transition == states_[cursor].next.end() ||
        transition->second != target) {
      break;
    }
    transition->second = clone;
    cursor = states_[cursor].link;
  }

  states_[target].link = clone;
  states_[current].link = clone;
  last_ = current;
}

void SuffixAutomatonByteIndex::finalize_occurrences_and_distinct_count() {
  std::vector<std::size_t> order(states_.size());
  for (std::size_t index = 0; index < states_.size(); ++index) {
    order[index] = index;
  }
  std::sort(order.begin(), order.end(), [this](std::size_t left,
                                                std::size_t right) {
    if (states_[left].length != states_[right].length) {
      return states_[left].length > states_[right].length;
    }
    return left > right;
  });

  for (const std::size_t state : order) {
    if (state == 0U) {
      continue;
    }
    const std::size_t link = states_[state].link;
    if (link == kNoState || states_[link].length >= states_[state].length) {
      throw std::logic_error("suffix automaton link invariant violated");
    }
    states_[link].occurrences = checked_add(
        states_[link].occurrences, states_[state].occurrences,
        "suffix automaton occurrence count overflow");
    const std::size_t contribution = states_[state].length - states_[link].length;
    distinct_substring_count_ = checked_add(
        distinct_substring_count_, contribution,
        "suffix automaton distinct-substring count overflow");
  }
}

std::size_t SuffixAutomatonByteIndex::find_state(std::string_view pattern) const {
  std::size_t state = 0U;
  for (const char raw_byte : pattern) {
    const unsigned char byte = static_cast<unsigned char>(raw_byte);
    const auto transition = states_[state].next.find(byte);
    if (transition == states_[state].next.end()) {
      return kNoState;
    }
    state = transition->second;
  }
  return state;
}

bool SuffixAutomatonByteIndex::contains(std::string_view pattern) const {
  return find_state(pattern) != kNoState;
}

std::size_t SuffixAutomatonByteIndex::occurrence_count(
    std::string_view pattern) const {
  if (pattern.empty()) {
    return checked_add(text_length_, 1U,
                       "empty-pattern occurrence count overflow");
  }
  const std::size_t state = find_state(pattern);
  return state == kNoState ? 0U : states_[state].occurrences;
}

std::size_t SuffixAutomatonByteIndex::distinct_substring_count() const noexcept {
  return distinct_substring_count_;
}

std::size_t SuffixAutomatonByteIndex::state_count() const noexcept {
  return states_.size();
}

std::size_t SuffixAutomatonByteIndex::text_length() const noexcept {
  return text_length_;
}

}  // namespace algorithms::strings
