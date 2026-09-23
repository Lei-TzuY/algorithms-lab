#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct SardinasPattersonResult {
  bool uniquely_decodable{true};
  std::vector<std::size_t> first_sequence;
  std::vector<std::size_t> second_sequence;

  friend bool operator==(const SardinasPattersonResult&,
                         const SardinasPattersonResult&) = default;
};

namespace sardinas_patterson_detail {

inline constexpr std::size_t kNoIndex =
    std::numeric_limits<std::size_t>::max();

enum class LongerSide : unsigned char {
  left,
  right,
};

struct ResidualState {
  std::string residual;
  LongerSide longer_side{LongerSide::left};

  std::size_t parent{kNoIndex};

  // Populated only for root states.
  std::size_t first_left{kNoIndex};
  std::size_t first_right{kNoIndex};

  // Populated only for non-root states.
  std::size_t appended{kNoIndex};
  bool appended_to_left{false};
};

[[nodiscard]] inline bool starts_with_bytes(
    const std::string_view whole,
    const std::string_view prefix) noexcept {
  return whole.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), whole.begin());
}

[[nodiscard]] inline std::pair<std::vector<std::size_t>,
                               std::vector<std::size_t>>
reconstruct_sequences(
    const std::vector<ResidualState>& states,
    std::size_t state_index) {
  if (state_index >= states.size()) {
    throw std::logic_error(
        "Sardinas-Patterson witness state index is invalid");
  }

  std::vector<std::size_t> reversed_children;
  std::size_t root = state_index;
  while (states[root].parent != kNoIndex) {
    reversed_children.push_back(root);
    root = states[root].parent;
    if (root >= states.size()) {
      throw std::logic_error(
          "Sardinas-Patterson witness parent is invalid");
    }
  }

  const ResidualState& root_state = states[root];
  if (root_state.first_left == kNoIndex ||
      root_state.first_right == kNoIndex) {
    throw std::logic_error(
        "Sardinas-Patterson root witness is incomplete");
  }

  std::vector<std::size_t> left{root_state.first_left};
  std::vector<std::size_t> right{root_state.first_right};

  std::reverse(reversed_children.begin(), reversed_children.end());
  for (const std::size_t child_index : reversed_children) {
    const ResidualState& child = states[child_index];
    if (child.appended == kNoIndex) {
      throw std::logic_error(
          "Sardinas-Patterson transition witness is incomplete");
    }
    if (child.appended_to_left) {
      left.push_back(child.appended);
    } else {
      right.push_back(child.appended);
    }
  }

  return {std::move(left), std::move(right)};
}

}  // namespace sardinas_patterson_detail

// Decide exact unique decodability of a finite arbitrary-byte codebook.
//
// Input codewords must be non-empty and pairwise distinct. Codeword indices are
// the source symbols used by any ambiguity witness.
//
// If the code is uniquely decodable, both witness sequences are empty.
// Otherwise the returned non-empty index sequences are distinct and their
// concatenated codewords are byte-for-byte identical.
//
// The algorithm is a witness-carrying Sardinas-Patterson residual search. A
// state records which side of a partial decoding is longer and the unmatched
// suffix. Future behavior depends only on this oriented residual, so the first
// visit to each (side, suffix) is sufficient. Residuals are always non-empty
// proper suffixes induced by prefix cancellation, yielding a finite state set.
[[nodiscard]] inline SardinasPattersonResult
sardinas_patterson_unique_decodability(
    const std::span<const std::string> codewords) {
  using sardinas_patterson_detail::LongerSide;
  using sardinas_patterson_detail::ResidualState;
  using sardinas_patterson_detail::kNoIndex;

  std::set<std::string> distinct;
  for (const std::string& codeword : codewords) {
    if (codeword.empty()) {
      throw std::invalid_argument(
          "Sardinas-Patterson codewords must be non-empty");
    }
    if (!distinct.insert(codeword).second) {
      throw std::invalid_argument(
          "Sardinas-Patterson codewords must be pairwise distinct");
    }
  }

  if (codewords.size() <= 1U) {
    return SardinasPattersonResult{};
  }

  std::vector<ResidualState> states;
  std::map<std::pair<LongerSide, std::string>, std::size_t> visited;

  const auto add_root =
      [&](std::string residual, const LongerSide side,
          const std::size_t left, const std::size_t right) {
        if (residual.empty()) {
          throw std::logic_error(
              "Sardinas-Patterson root residual must be non-empty");
        }
        const auto key = std::make_pair(side, residual);
        if (visited.contains(key)) {
          return;
        }
        const std::size_t index = states.size();
        visited.emplace(key, index);
        states.push_back(
            ResidualState{std::move(residual), side, kNoIndex,
                          left, right, kNoIndex, false});
      };

  const auto add_transition =
      [&](std::string residual, const LongerSide side,
          const std::size_t parent, const std::size_t appended,
          const bool appended_to_left) {
        if (residual.empty()) {
          throw std::logic_error(
              "Sardinas-Patterson transition residual must be non-empty");
        }
        const auto key = std::make_pair(side, residual);
        if (visited.contains(key)) {
          return;
        }
        const std::size_t index = states.size();
        visited.emplace(key, index);
        states.push_back(
            ResidualState{std::move(residual), side, parent,
                          kNoIndex, kNoIndex, appended,
                          appended_to_left});
      };

  // Seed the first Sardinas-Patterson residuals from pairs of distinct source
  // symbols whose codewords are prefix-comparable.
  for (std::size_t left = 0U; left < codewords.size(); ++left) {
    for (std::size_t right = left + 1U;
         right < codewords.size(); ++right) {
      const std::string& left_word = codewords[left];
      const std::string& right_word = codewords[right];

      if (sardinas_patterson_detail::starts_with_bytes(
              left_word, right_word)) {
        add_root(left_word.substr(right_word.size()),
                 LongerSide::left, left, right);
      } else if (sardinas_patterson_detail::starts_with_bytes(
                     right_word, left_word)) {
        add_root(right_word.substr(left_word.size()),
                 LongerSide::right, left, right);
      }
    }
  }

  // states is also the deterministic BFS queue. Newly discovered residuals are
  // appended; cursor advances monotonically.
  for (std::size_t cursor = 0U; cursor < states.size(); ++cursor) {
    // Copy the state because add_transition may reallocate states.
    const ResidualState state = states[cursor];

    for (std::size_t symbol = 0U; symbol < codewords.size(); ++symbol) {
      const std::string& word = codewords[symbol];
      const std::string& residual = state.residual;

      // The next codeword is always appended to the currently shorter side.
      const bool append_to_left =
          state.longer_side == LongerSide::right;

      if (word == residual) {
        auto [left_sequence, right_sequence] =
            sardinas_patterson_detail::reconstruct_sequences(
                states, cursor);
        if (append_to_left) {
          left_sequence.push_back(symbol);
        } else {
          right_sequence.push_back(symbol);
        }

        if (left_sequence == right_sequence) {
          throw std::logic_error(
              "Sardinas-Patterson ambiguity witness is not distinct");
        }

        return SardinasPattersonResult{
            false, std::move(left_sequence),
            std::move(right_sequence)};
      }

      if (sardinas_patterson_detail::starts_with_bytes(
              residual, word)) {
        // The appended codeword consumes only part of the residual, so the
        // same side remains longer.
        add_transition(
            residual.substr(word.size()),
            state.longer_side, cursor, symbol, append_to_left);
      } else if (sardinas_patterson_detail::starts_with_bytes(
                     word, residual)) {
        // The appended codeword overruns the residual and becomes the new
        // unmatched suffix on the opposite side.
        const LongerSide flipped =
            state.longer_side == LongerSide::left
                ? LongerSide::right
                : LongerSide::left;
        add_transition(
            word.substr(residual.size()),
            flipped, cursor, symbol, append_to_left);
      }
    }
  }

  return SardinasPattersonResult{};
}

}  // namespace algorithms::coding
