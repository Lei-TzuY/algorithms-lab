#pragma once

#include "algorithms/data_structures/packed_rank_select.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Immutable rooted ordered-tree topology encoded as one balanced-parentheses
// sequence (1=open, 0=close). Node ids are preorder ranks of opening bits.
//
// The topology bits reuse PackedRankSelectBitVector. For explicit constant-time
// parent/matching-close navigation this baseline also stores two size_t tables;
// consequently it does not claim n+o(n) succinct-tree space.
class BalancedParenthesesTreeIndex {
 public:
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  explicit BalancedParenthesesTreeIndex(std::span<const std::uint8_t> parentheses)
      : bits_(parentheses) {
    if (parentheses.empty()) return;
    if ((parentheses.size() & 1U) != 0U) {
      throw std::invalid_argument("balanced-parentheses tree length must be even");
    }
    if (parentheses.front() != 1U) {
      throw std::invalid_argument(
          "balanced-parentheses tree must begin with an opening parenthesis");
    }

    parent_.reserve(parentheses.size() / 2U);
    close_position_.reserve(parentheses.size() / 2U);
    std::vector<std::size_t> stack;
    stack.reserve(parentheses.size() / 2U);

    for (std::size_t position = 0U; position < parentheses.size(); ++position) {
      if (parentheses[position] == 1U) {
        const std::size_t node = parent_.size();
        parent_.push_back(stack.empty() ? npos : stack.back());
        close_position_.push_back(npos);
        stack.push_back(node);
      } else {
        if (stack.empty()) {
          throw std::invalid_argument("balanced-parentheses tree prefix underflows");
        }
        const std::size_t node = stack.back();
        stack.pop_back();
        close_position_[node] = position;
        if (stack.empty() && position + 1U != parentheses.size()) {
          throw std::invalid_argument(
              "balanced-parentheses input encodes a forest, not one rooted tree");
        }
      }
    }
    if (!stack.empty()) {
      throw std::invalid_argument("balanced-parentheses tree has unclosed nodes");
    }
    if (parent_.size() != bits_.zero_count()) {
      throw std::invalid_argument(
          "balanced-parentheses tree has unequal open/close counts");
    }
  }

  [[nodiscard]] bool empty() const noexcept { return parent_.empty(); }
  [[nodiscard]] std::size_t node_count() const noexcept { return parent_.size(); }
  [[nodiscard]] std::size_t bit_count() const noexcept { return bits_.size(); }
  [[nodiscard]] std::size_t packed_topology_payload_bytes() const noexcept {
    return bits_.logical_payload_bytes();
  }

  [[nodiscard]] std::size_t open_position(std::size_t node) const {
    validate_node(node);
    const auto position = bits_.select1(node);
    if (!position) {
      throw std::logic_error("balanced-parentheses open index is inconsistent");
    }
    return *position;
  }

  [[nodiscard]] std::size_t close_position(std::size_t node) const {
    validate_node(node);
    return close_position_[node];
  }

  [[nodiscard]] std::optional<std::size_t> node_at_open_position(
      std::size_t position) const {
    if (position >= bits_.size() || !bits_.bit(position)) return std::nullopt;
    return bits_.rank1(position + 1U) - 1U;
  }

  [[nodiscard]] std::size_t find_close(std::size_t open_position_value) const {
    const auto node = require_open_position(open_position_value);
    return close_position_[node];
  }

  [[nodiscard]] std::optional<std::size_t> enclose(
      std::size_t open_position_value) const {
    const auto node = require_open_position(open_position_value);
    if (parent_[node] == npos) return std::nullopt;
    return open_position(parent_[node]);
  }

  [[nodiscard]] std::optional<std::size_t> parent(std::size_t node) const {
    validate_node(node);
    return parent_[node] == npos ? std::nullopt
                                 : std::optional<std::size_t>{parent_[node]};
  }

  [[nodiscard]] std::optional<std::size_t> first_child(std::size_t node) const {
    const std::size_t open = open_position(node);
    const std::size_t candidate = open + 1U;
    if (candidate >= close_position_[node] || !bits_.bit(candidate)) {
      return std::nullopt;
    }
    return node_at_open_position(candidate);
  }

  [[nodiscard]] std::optional<std::size_t> next_sibling(std::size_t node) const {
    validate_node(node);
    if (parent_[node] == npos) return std::nullopt;
    const std::size_t candidate = close_position_[node] + 1U;
    if (candidate >= close_position_[parent_[node]] || !bits_.bit(candidate)) {
      return std::nullopt;
    }
    return node_at_open_position(candidate);
  }

  [[nodiscard]] std::size_t depth(std::size_t node) const {
    const std::size_t open = open_position(node);
    const std::size_t opens = bits_.rank1(open);
    const std::size_t closes = bits_.rank0(open);
    if (opens < closes) {
      throw std::logic_error("balanced-parentheses prefix excess is invalid");
    }
    return opens - closes;
  }

  [[nodiscard]] std::size_t subtree_size(std::size_t node) const {
    const std::size_t open = open_position(node);
    return bits_.rank1(close_position_[node] + 1U) - bits_.rank1(open);
  }

  [[nodiscard]] bool is_ancestor(std::size_t ancestor,
                                 std::size_t node) const {
    const std::size_t ancestor_open = open_position(ancestor);
    const std::size_t node_open = open_position(node);
    return ancestor_open <= node_open &&
           node_open < close_position_[ancestor];
  }

  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      if (parent_.size() != close_position_.size()) return false;
      if (parent_.empty()) return bits_.empty();
      if (bits_.one_count() != parent_.size() ||
          bits_.zero_count() != parent_.size()) {
        return false;
      }
      std::vector<std::size_t> stack;
      stack.reserve(parent_.size());
      std::size_t next_node = 0U;
      for (std::size_t position = 0U; position < bits_.size(); ++position) {
        if (bits_.bit(position)) {
          if (next_node >= parent_.size()) return false;
          const std::size_t expected_parent =
              stack.empty() ? npos : stack.back();
          if (parent_[next_node] != expected_parent) return false;
          stack.push_back(next_node++);
        } else {
          if (stack.empty()) return false;
          const std::size_t node = stack.back();
          stack.pop_back();
          if (close_position_[node] != position) return false;
          if (stack.empty() && position + 1U != bits_.size()) return false;
        }
      }
      return stack.empty() && next_node == parent_.size();
    } catch (...) {
      return false;
    }
  }

 private:
  void validate_node(std::size_t node) const {
    if (node >= parent_.size()) {
      throw std::out_of_range("balanced-parentheses node index out of range");
    }
  }

  [[nodiscard]] std::size_t require_open_position(std::size_t position) const {
    if (position >= bits_.size()) {
      throw std::out_of_range("balanced-parentheses bit position out of range");
    }
    if (!bits_.bit(position)) {
      throw std::invalid_argument("balanced-parentheses position is not an opening bit");
    }
    return bits_.rank1(position + 1U) - 1U;
  }

  PackedRankSelectBitVector bits_;
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> close_position_;
};

}  // namespace algorithms::data_structures
