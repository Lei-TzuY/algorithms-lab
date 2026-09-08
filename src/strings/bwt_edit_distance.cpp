#include "algorithms/strings/bwt_edit_distance.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::strings {
namespace {

void checked_increment(std::size_t& value, const char* message) {
  if (value == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error(message);
  }
  ++value;
}

struct EditSearchNode {
  BidirectionalBwtState state;
  std::size_t pattern_consumed = 0U;
  std::size_t edits = 0U;
  std::string candidate;
};

using EditStateKey =
    std::tuple<std::size_t, std::size_t, std::size_t, std::size_t,
               std::size_t, std::size_t>;

EditStateKey state_key(const EditSearchNode& node) {
  return EditStateKey{node.pattern_consumed,
                      node.candidate.size(),
                      node.state.forward_begin(),
                      node.state.forward_end(),
                      node.state.reverse_begin(),
                      node.state.reverse_end()};
}

std::string prepend_byte(std::uint8_t value, const std::string& suffix) {
  if (suffix.size() == suffix.max_size()) {
    throw std::length_error("edit-distance BWT candidate length overflow");
  }
  std::string result;
  result.reserve(suffix.size() + 1U);
  result.push_back(std::bit_cast<char>(value));
  result.append(suffix);
  return result;
}

}  // namespace

EditDistanceBwtSearchResult locate_bwt_edit_distance(
    const BidirectionalBwtByteIndex& index, std::string_view pattern,
    std::size_t max_edits) {
  EditDistanceBwtSearchResult result;

  // The empty candidate is within budget whenever the pattern is empty or the
  // complete pattern can be deleted. In either case every text boundary is a
  // valid substring start, so expanding the exponential state space would add
  // no information.
  if (pattern.empty() || max_edits >= pattern.size()) {
    result.positions.reserve(index.row_count());
    for (std::size_t position = 0U; position < index.row_count(); ++position) {
      result.positions.push_back(position);
    }
    result.terminal_states = 1U;
    result.peak_frontier_size = 1U;
    return result;
  }

  std::deque<EditSearchNode> frontier;
  std::map<EditStateKey, std::size_t> best_edits;

  EditSearchNode start{index.empty_state(), 0U, 0U, {}};
  best_edits.emplace(state_key(start), 0U);
  frontier.push_back(std::move(start));
  result.peak_frontier_size = 1U;

  std::vector<std::uint8_t> seen_position(index.row_count(), 0U);

  const auto enqueue = [&](EditSearchNode candidate, bool zero_cost) {
    const EditStateKey key = state_key(candidate);
    const auto found = best_edits.find(key);
    if (found != best_edits.end() && found->second <= candidate.edits) {
      checked_increment(result.dominated_states,
                        "edit-distance BWT dominated-state count overflow");
      return;
    }
    best_edits[key] = candidate.edits;
    if (zero_cost) {
      frontier.push_front(std::move(candidate));
    } else {
      frontier.push_back(std::move(candidate));
    }
    result.peak_frontier_size =
        std::max(result.peak_frontier_size, frontier.size());
  };

  while (!frontier.empty()) {
    EditSearchNode node = std::move(frontier.front());
    frontier.pop_front();

    const auto best = best_edits.find(state_key(node));
    if (best == best_edits.end() || best->second != node.edits) {
      continue;
    }

    checked_increment(result.expanded_states,
                      "edit-distance BWT expanded-state count overflow");

    if (node.pattern_consumed == pattern.size()) {
      checked_increment(result.terminal_states,
                        "edit-distance BWT terminal-state count overflow");
      if (node.candidate.empty()) {
        throw std::logic_error(
            "edit-distance BWT empty terminal escaped fast path");
      }
      const auto exact = index.locate_hamming(node.candidate, 0U);
      for (const std::size_t position : exact.positions) {
        if (position >= seen_position.size()) {
          throw std::logic_error(
              "edit-distance BWT terminal position invariant violated");
        }
        seen_position[position] = 1U;
      }
    }

    if (node.edits < max_edits) {
      // Insert one text byte without consuming a pattern byte. Because search
      // runs right-to-left, this prepends the inserted byte to the exact BWT
      // candidate represented by the current state.
      for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
        checked_increment(result.transitions_considered,
                          "edit-distance BWT transition count overflow");
        const auto value = static_cast<std::uint8_t>(symbol);
        const BidirectionalBwtState extended =
            index.extend_left(node.state, value);
        if (extended.match_count() == 0U) {
          checked_increment(
              result.pruned_empty_transitions,
              "edit-distance BWT empty-transition count overflow");
          continue;
        }
        enqueue(EditSearchNode{extended, node.pattern_consumed,
                               node.edits + 1U,
                               prepend_byte(value, node.candidate)},
                false);
      }
    }

    if (node.pattern_consumed == pattern.size()) {
      continue;
    }

    const std::size_t pattern_index =
        pattern.size() - 1U - node.pattern_consumed;
    const auto expected = static_cast<std::uint8_t>(
        static_cast<unsigned char>(pattern[pattern_index]));

    // Exact byte match: consume one pattern byte and one candidate byte at
    // zero edit cost.
    checked_increment(result.transitions_considered,
                      "edit-distance BWT transition count overflow");
    const BidirectionalBwtState matched =
        index.extend_left(node.state, expected);
    if (matched.match_count() == 0U) {
      checked_increment(result.pruned_empty_transitions,
                        "edit-distance BWT empty-transition count overflow");
    } else {
      enqueue(EditSearchNode{matched, node.pattern_consumed + 1U, node.edits,
                             prepend_byte(expected, node.candidate)},
              true);
    }

    if (node.edits == max_edits) {
      continue;
    }

    // Delete one pattern byte: pattern progress advances without extending the
    // exact text candidate.
    checked_increment(result.transitions_considered,
                      "edit-distance BWT transition count overflow");
    enqueue(EditSearchNode{node.state, node.pattern_consumed + 1U,
                           node.edits + 1U, node.candidate},
            false);

    // Substitute the current pattern byte with every other byte value.
    for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
      const auto value = static_cast<std::uint8_t>(symbol);
      if (value == expected) {
        continue;
      }
      checked_increment(result.transitions_considered,
                        "edit-distance BWT transition count overflow");
      const BidirectionalBwtState substituted =
          index.extend_left(node.state, value);
      if (substituted.match_count() == 0U) {
        checked_increment(result.pruned_empty_transitions,
                          "edit-distance BWT empty-transition count overflow");
        continue;
      }
      enqueue(EditSearchNode{substituted, node.pattern_consumed + 1U,
                             node.edits + 1U,
                             prepend_byte(value, node.candidate)},
              false);
    }
  }

  for (std::size_t position = 0U; position < seen_position.size(); ++position) {
    if (seen_position[position] != 0U) {
      result.positions.push_back(position);
    }
  }
  return result;
}

}  // namespace algorithms::strings
