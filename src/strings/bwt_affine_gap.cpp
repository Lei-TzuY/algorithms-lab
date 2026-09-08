#include "algorithms/strings/bwt_affine_gap.hpp"

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::strings {
namespace {

enum class GapMode : std::uint8_t { none = 0U, insertion = 1U, deletion = 2U };

void checked_increment(std::size_t& value, const char* message) {
  if (value == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error(message);
  }
  ++value;
}

void validate_costs(AffineGapCosts costs) {
  if (costs.substitution == 0U || costs.gap_open == 0U ||
      costs.gap_extend == 0U) {
    throw std::invalid_argument(
        "affine-gap BWT non-match costs must be positive");
  }
}

[[nodiscard]] bool gap_run_within_budget(std::size_t length,
                                         std::size_t max_score,
                                         AffineGapCosts costs) {
  if (length == 0U) {
    return true;
  }
  if (costs.gap_open > max_score) {
    return false;
  }
  if (length == 1U) {
    return true;
  }
  const std::size_t remaining = max_score - costs.gap_open;
  return length - 1U <= remaining / costs.gap_extend;
}

[[nodiscard]] bool add_within_budget(std::size_t score, std::size_t delta,
                                     std::size_t max_score,
                                     std::size_t& result) {
  if (score > max_score || delta > max_score - score) {
    return false;
  }
  result = score + delta;
  return true;
}

struct AffineSearchNode {
  BidirectionalBwtState state;
  std::size_t pattern_consumed = 0U;
  std::size_t score = 0U;
  GapMode gap_mode = GapMode::none;
  std::string candidate;
};

using AffineStateKey =
    std::tuple<std::size_t, std::size_t, std::size_t, std::size_t,
               std::size_t, std::size_t, std::uint8_t>;

[[nodiscard]] AffineStateKey state_key(const AffineSearchNode& node) {
  return AffineStateKey{node.pattern_consumed,
                        node.candidate.size(),
                        node.state.forward_begin(),
                        node.state.forward_end(),
                        node.state.reverse_begin(),
                        node.state.reverse_end(),
                        static_cast<std::uint8_t>(node.gap_mode)};
}

[[nodiscard]] std::string prepend_byte(std::uint8_t value,
                                       const std::string& suffix) {
  if (suffix.size() == suffix.max_size()) {
    throw std::length_error("affine-gap BWT candidate length overflow");
  }
  std::string result;
  result.reserve(suffix.size() + 1U);
  result.push_back(std::bit_cast<char>(value));
  result.append(suffix);
  return result;
}

struct QueueEntry {
  AffineSearchNode node;
  std::size_t serial = 0U;
};

struct QueueCompare {
  [[nodiscard]] bool operator()(const QueueEntry& left,
                                const QueueEntry& right) const noexcept {
    return std::tie(left.node.score, left.serial) <
           std::tie(right.node.score, right.serial);
  }
};

}  // namespace

AffineGapBwtSearchResult locate_bwt_affine_gap(
    const BidirectionalBwtByteIndex& index, std::string_view pattern,
    std::size_t max_score, AffineGapCosts costs) {
  validate_costs(costs);
  AffineGapBwtSearchResult result;

  if (pattern.empty() ||
      gap_run_within_budget(pattern.size(), max_score, costs)) {
    result.positions.reserve(index.row_count());
    for (std::size_t position = 0U; position < index.row_count(); ++position) {
      result.positions.push_back(position);
    }
    result.terminal_states = 1U;
    result.peak_frontier_size = 1U;
    return result;
  }

  using Queue = algorithms::data_structures::BinaryHeap<QueueEntry, QueueCompare>;
  Queue frontier;
  std::map<AffineStateKey, std::size_t> best_score;
  std::size_t next_serial = 0U;

  const auto push_queue = [&](AffineSearchNode node) {
    if (next_serial == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("affine-gap BWT queue serial overflow");
    }
    frontier.push(QueueEntry{std::move(node), next_serial});
    ++next_serial;
    result.peak_frontier_size =
        std::max(result.peak_frontier_size, frontier.size());
  };

  AffineSearchNode start{index.empty_state(), 0U, 0U, GapMode::none, {}};
  best_score.emplace(state_key(start), 0U);
  push_queue(std::move(start));

  std::vector<std::uint8_t> seen_position(index.row_count(), 0U);

  const auto enqueue = [&](AffineSearchNode candidate) {
    const AffineStateKey key = state_key(candidate);
    const auto found = best_score.find(key);
    if (found != best_score.end() && found->second <= candidate.score) {
      checked_increment(result.dominated_states,
                        "affine-gap BWT dominated-state count overflow");
      return;
    }
    best_score[key] = candidate.score;
    push_queue(std::move(candidate));
  };

  const auto transition_score = [&](std::size_t current, std::size_t delta,
                                    std::size_t& next) {
    checked_increment(result.transitions_considered,
                      "affine-gap BWT transition count overflow");
    if (!add_within_budget(current, delta, max_score, next)) {
      checked_increment(
          result.pruned_over_budget_transitions,
          "affine-gap BWT over-budget transition count overflow");
      return false;
    }
    return true;
  };

  while (!frontier.empty()) {
    QueueEntry entry = frontier.pop();
    AffineSearchNode node = std::move(entry.node);
    const auto best = best_score.find(state_key(node));
    if (best == best_score.end() || best->second != node.score) {
      continue;
    }

    checked_increment(result.expanded_states,
                      "affine-gap BWT expanded-state count overflow");

    if (node.pattern_consumed == pattern.size()) {
      checked_increment(result.terminal_states,
                        "affine-gap BWT terminal-state count overflow");
      if (node.candidate.empty()) {
        throw std::logic_error("affine-gap BWT empty terminal escaped fast path");
      }
      const auto exact = index.locate_hamming(node.candidate, 0U);
      for (const std::size_t position : exact.positions) {
        if (position >= seen_position.size()) {
          throw std::logic_error(
              "affine-gap BWT terminal position invariant violated");
        }
        seen_position[position] = 1U;
      }
    }

    const std::size_t insertion_cost =
        node.gap_mode == GapMode::insertion ? costs.gap_extend : costs.gap_open;
    std::size_t insertion_score = 0U;
    if (transition_score(node.score, insertion_cost, insertion_score)) {
      for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
        if (symbol != 0U) {
          checked_increment(result.transitions_considered,
                            "affine-gap BWT transition count overflow");
        }
        const auto value = static_cast<std::uint8_t>(symbol);
        const BidirectionalBwtState extended = index.extend_left(node.state, value);
        if (extended.match_count() == 0U) {
          checked_increment(result.pruned_empty_transitions,
                            "affine-gap BWT empty-transition count overflow");
          continue;
        }
        enqueue(AffineSearchNode{extended, node.pattern_consumed,
                                 insertion_score, GapMode::insertion,
                                 prepend_byte(value, node.candidate)});
      }
    } else {
      // transition_score accounts for one semantic insertion branch. The
      // byte-oriented search has 256 such branches, so account for the other
      // 255 without performing BWT extensions.
      for (std::size_t symbol = 1U; symbol < 256U; ++symbol) {
        checked_increment(result.transitions_considered,
                          "affine-gap BWT transition count overflow");
        checked_increment(
            result.pruned_over_budget_transitions,
            "affine-gap BWT over-budget transition count overflow");
      }
    }

    if (node.pattern_consumed == pattern.size()) {
      continue;
    }

    const std::size_t pattern_index =
        pattern.size() - 1U - node.pattern_consumed;
    const auto expected = static_cast<std::uint8_t>(
        static_cast<unsigned char>(pattern[pattern_index]));

    checked_increment(result.transitions_considered,
                      "affine-gap BWT transition count overflow");
    const BidirectionalBwtState matched = index.extend_left(node.state, expected);
    if (matched.match_count() == 0U) {
      checked_increment(result.pruned_empty_transitions,
                        "affine-gap BWT empty-transition count overflow");
    } else {
      enqueue(AffineSearchNode{matched, node.pattern_consumed + 1U, node.score,
                               GapMode::none,
                               prepend_byte(expected, node.candidate)});
    }

    std::size_t substitution_score = 0U;
    if (add_within_budget(node.score, costs.substitution, max_score,
                          substitution_score)) {
      for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
        const auto value = static_cast<std::uint8_t>(symbol);
        if (value == expected) {
          continue;
        }
        checked_increment(result.transitions_considered,
                          "affine-gap BWT transition count overflow");
        const BidirectionalBwtState substituted =
            index.extend_left(node.state, value);
        if (substituted.match_count() == 0U) {
          checked_increment(result.pruned_empty_transitions,
                            "affine-gap BWT empty-transition count overflow");
          continue;
        }
        enqueue(AffineSearchNode{substituted, node.pattern_consumed + 1U,
                                 substitution_score, GapMode::none,
                                 prepend_byte(value, node.candidate)});
      }
    } else {
      for (std::size_t symbol = 0U; symbol < 255U; ++symbol) {
        checked_increment(result.transitions_considered,
                          "affine-gap BWT transition count overflow");
        checked_increment(
            result.pruned_over_budget_transitions,
            "affine-gap BWT over-budget transition count overflow");
      }
    }

    const std::size_t deletion_cost =
        node.gap_mode == GapMode::deletion ? costs.gap_extend : costs.gap_open;
    std::size_t deletion_score = 0U;
    if (transition_score(node.score, deletion_cost, deletion_score)) {
      enqueue(AffineSearchNode{node.state, node.pattern_consumed + 1U,
                               deletion_score, GapMode::deletion,
                               node.candidate});
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
