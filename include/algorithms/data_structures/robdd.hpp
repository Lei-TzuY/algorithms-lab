#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

using BddNodeId = std::size_t;

enum class BddBinaryOp : unsigned char {
  logical_and,
  logical_or,
  logical_xor,
};

struct BddDecisionNode {
  std::size_t variable{};
  BddNodeId low{};
  BddNodeId high{};

  friend bool operator==(const BddDecisionNode&, const BddDecisionNode&) = default;
};

// Reduced ordered binary decision diagram manager for the fixed variable order
// 0 < 1 < ... < variable_count-1. Node ids are stable within one manager.
class ReducedOrderedBdd {
 public:
  explicit ReducedOrderedBdd(std::size_t variable_count)
      : variable_count_(variable_count),
        nodes_{{variable_count, 0U, 0U}, {variable_count, 1U, 1U}} {}

  [[nodiscard]] std::size_t variable_count() const noexcept {
    return variable_count_;
  }

  [[nodiscard]] static constexpr BddNodeId false_node() noexcept { return 0U; }
  [[nodiscard]] static constexpr BddNodeId true_node() noexcept { return 1U; }

  [[nodiscard]] std::size_t allocated_node_count() const noexcept {
    return nodes_.size();
  }

  [[nodiscard]] BddNodeId variable(std::size_t variable_index) {
    if (variable_index >= variable_count_) {
      throw std::out_of_range("ROBDD variable index out of range");
    }
    return make_node(variable_index, false_node(), true_node());
  }

  [[nodiscard]] BddNodeId negate(BddNodeId root) {
    validate_node(root);
    if (root == false_node()) {
      return true_node();
    }
    if (root == true_node()) {
      return false_node();
    }
    const auto cached = negate_cache_.find(root);
    if (cached != negate_cache_.end()) {
      return cached->second;
    }

    const StoredNode node = nodes_[root];
    const BddNodeId low = negate(node.low);
    const BddNodeId high = negate(node.high);
    const BddNodeId result = make_node(node.variable, low, high);
    negate_cache_.emplace(root, result);
    negate_cache_.emplace(result, root);
    return result;
  }

  [[nodiscard]] BddNodeId apply(BddBinaryOp op, BddNodeId lhs, BddNodeId rhs) {
    validate_node(lhs);
    validate_node(rhs);

    if (lhs > rhs) {
      std::swap(lhs, rhs);
    }

    switch (op) {
      case BddBinaryOp::logical_and:
        if (lhs == false_node()) {
          return false_node();
        }
        if (lhs == true_node()) {
          return rhs;
        }
        if (lhs == rhs) {
          return lhs;
        }
        break;
      case BddBinaryOp::logical_or:
        if (rhs == true_node()) {
          return true_node();
        }
        if (lhs == false_node()) {
          return rhs;
        }
        if (lhs == rhs) {
          return lhs;
        }
        break;
      case BddBinaryOp::logical_xor:
        if (lhs == false_node()) {
          return rhs;
        }
        if (rhs == true_node()) {
          return negate(lhs);
        }
        if (lhs == rhs) {
          return false_node();
        }
        break;
      default:
        throw std::invalid_argument("unknown ROBDD binary operation");
    }

    const ApplyKey key{op, lhs, rhs};
    const auto cached = apply_cache_.find(key);
    if (cached != apply_cache_.end()) {
      return cached->second;
    }

    if (is_terminal(lhs) && is_terminal(rhs)) {
      const bool left = lhs == true_node();
      const bool right = rhs == true_node();
      bool value = false;
      switch (op) {
        case BddBinaryOp::logical_and:
          value = left && right;
          break;
        case BddBinaryOp::logical_or:
          value = left || right;
          break;
        case BddBinaryOp::logical_xor:
          value = left != right;
          break;
        default:
          throw std::invalid_argument("unknown ROBDD binary operation");
      }
      return value ? true_node() : false_node();
    }

    const std::size_t variable_index =
        top_variable(lhs) < top_variable(rhs) ? top_variable(lhs) : top_variable(rhs);
    const BddNodeId lhs_low = cofactor(lhs, variable_index, false);
    const BddNodeId lhs_high = cofactor(lhs, variable_index, true);
    const BddNodeId rhs_low = cofactor(rhs, variable_index, false);
    const BddNodeId rhs_high = cofactor(rhs, variable_index, true);

    const BddNodeId low = apply(op, lhs_low, rhs_low);
    const BddNodeId high = apply(op, lhs_high, rhs_high);
    const BddNodeId result = make_node(variable_index, low, high);
    apply_cache_.emplace(key, result);
    return result;
  }

  // Truth-table rows are indexed by assignments in lexicographic binary order:
  // variable 0 is the most-significant assignment bit. Values must be 0 or 1.
  [[nodiscard]] BddNodeId from_truth_table(
      std::span<const std::uint8_t> truth_table) {
    constexpr std::size_t bit_count = std::numeric_limits<std::size_t>::digits;
    if (variable_count_ >= bit_count) {
      throw std::length_error("ROBDD truth table length is not representable");
    }
    const std::size_t expected_size = std::size_t{1} << variable_count_;
    if (truth_table.size() != expected_size) {
      throw std::invalid_argument("ROBDD truth table has wrong size");
    }
    for (const std::uint8_t value : truth_table) {
      if (value > 1U) {
        throw std::invalid_argument("ROBDD truth table values must be 0 or 1");
      }
    }
    return build_truth_table(0U, truth_table);
  }

  [[nodiscard]] bool evaluate(BddNodeId root,
                              std::span<const std::uint8_t> assignment) const {
    validate_node(root);
    if (assignment.size() != variable_count_) {
      throw std::invalid_argument("ROBDD assignment has wrong size");
    }
    for (const std::uint8_t value : assignment) {
      if (value > 1U) {
        throw std::invalid_argument("ROBDD assignment values must be 0 or 1");
      }
    }

    BddNodeId current = root;
    while (!is_terminal(current)) {
      const StoredNode& node = nodes_[current];
      current = assignment[node.variable] == 0U ? node.low : node.high;
    }
    return current == true_node();
  }

  [[nodiscard]] std::optional<BddDecisionNode> decision_node(BddNodeId id) const {
    validate_node(id);
    if (is_terminal(id)) {
      return std::nullopt;
    }
    const StoredNode& node = nodes_[id];
    return BddDecisionNode{node.variable, node.low, node.high};
  }

 private:
  struct StoredNode {
    std::size_t variable{};
    BddNodeId low{};
    BddNodeId high{};
  };

  struct ApplyKey {
    BddBinaryOp op{};
    BddNodeId lhs{};
    BddNodeId rhs{};

    friend bool operator<(const ApplyKey& left, const ApplyKey& right) noexcept {
      return std::tie(left.op, left.lhs, left.rhs) <
             std::tie(right.op, right.lhs, right.rhs);
    }
  };

  [[nodiscard]] bool is_terminal(BddNodeId id) const noexcept { return id < 2U; }

  void validate_node(BddNodeId id) const {
    if (id >= nodes_.size()) {
      throw std::out_of_range("ROBDD node id out of range");
    }
  }

  [[nodiscard]] std::size_t top_variable(BddNodeId id) const noexcept {
    return is_terminal(id) ? variable_count_ : nodes_[id].variable;
  }

  [[nodiscard]] BddNodeId cofactor(BddNodeId id, std::size_t variable_index,
                                   bool high) const {
    if (is_terminal(id) || nodes_[id].variable != variable_index) {
      return id;
    }
    return high ? nodes_[id].high : nodes_[id].low;
  }

  [[nodiscard]] BddNodeId make_node(std::size_t variable_index, BddNodeId low,
                                    BddNodeId high) {
    if (low == high) {
      return low;
    }
    if (variable_index >= variable_count_) {
      throw std::logic_error("ROBDD decision variable violates fixed order");
    }
    validate_node(low);
    validate_node(high);
    if ((!is_terminal(low) && nodes_[low].variable <= variable_index) ||
        (!is_terminal(high) && nodes_[high].variable <= variable_index)) {
      throw std::logic_error("ROBDD child violates fixed variable order");
    }

    const auto key = std::make_tuple(variable_index, low, high);
    const auto existing = unique_table_.find(key);
    if (existing != unique_table_.end()) {
      return existing->second;
    }
    if (nodes_.size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("ROBDD node table exhausted");
    }
    const BddNodeId id = nodes_.size();
    nodes_.push_back(StoredNode{variable_index, low, high});
    unique_table_.emplace(key, id);
    return id;
  }

  [[nodiscard]] BddNodeId build_truth_table(
      std::size_t variable_index, std::span<const std::uint8_t> table) {
    if (variable_index == variable_count_) {
      return table.front() == 0U ? false_node() : true_node();
    }
    const std::size_t half = table.size() / 2U;
    const BddNodeId low = build_truth_table(variable_index + 1U, table.first(half));
    const BddNodeId high =
        build_truth_table(variable_index + 1U, table.subspan(half, half));
    return make_node(variable_index, low, high);
  }

  std::size_t variable_count_{};
  std::vector<StoredNode> nodes_;
  std::map<std::tuple<std::size_t, BddNodeId, BddNodeId>, BddNodeId> unique_table_;
  std::map<ApplyKey, BddNodeId> apply_cache_;
  std::map<BddNodeId, BddNodeId> negate_cache_;
};

}  // namespace algorithms::data_structures
