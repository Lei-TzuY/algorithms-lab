#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::randomized {

struct KnuthYaoSample {
  std::size_t symbol{};
  std::size_t bits_consumed{};

  friend bool operator==(const KnuthYaoSample&,
                         const KnuthYaoSample&) = default;
};

// Exact finite Knuth-Yao discrete-distribution-generating tree for dyadic
// integer weights.
//
// The weights must sum to a non-zero power of two. A caller-supplied bit source
// provides successive fair bits. The sampler consumes only the prefix needed
// to reach a DDG-tree leaf; zero-weight symbols have no leaves.
//
// This class verifies only the combinatorial distribution tree. Fairness and
// independence of the caller's bit source are external assumptions.
class DyadicKnuthYaoSampler {
 public:
  explicit DyadicKnuthYaoSampler(std::vector<std::uint64_t> weights)
      : weights_(std::move(weights)) {
    if (weights_.empty()) {
      throw std::invalid_argument(
          "Knuth-Yao sampler requires at least one symbol");
    }

    for (const std::uint64_t weight : weights_) {
      if (weight >
          std::numeric_limits<std::uint64_t>::max() - total_weight_) {
        throw std::overflow_error(
            "Knuth-Yao weight sum exceeds uint64_t");
      }
      total_weight_ += weight;
    }

    if (total_weight_ == 0U) {
      throw std::invalid_argument(
          "Knuth-Yao weights must have positive total mass");
    }
    if ((total_weight_ & (total_weight_ - 1U)) != 0U) {
      throw std::invalid_argument(
          "Knuth-Yao dyadic weights must sum to a power of two");
    }

    const std::size_t denominator_bits =
        static_cast<std::size_t>(std::bit_width(total_weight_) - 1U);
    normalization_shift_ = denominator_bits;
    for (const std::uint64_t weight : weights_) {
      if (weight == 0U) {
        continue;
      }
      normalization_shift_ =
          std::min(normalization_shift_,
                   static_cast<std::size_t>(std::countr_zero(weight)));
    }

    const std::uint64_t normalized_total =
        total_weight_ >> normalization_shift_;
    precision_bits_ = static_cast<std::size_t>(
        std::bit_width(normalized_total) - 1U);
    build_tree();

    if (!valid_structure()) {
      throw std::logic_error(
          "Knuth-Yao DDG construction violated its invariants");
    }
  }

  [[nodiscard]] std::size_t symbol_count() const noexcept {
    return weights_.size();
  }

  [[nodiscard]] std::uint64_t total_weight() const noexcept {
    return total_weight_;
  }

  [[nodiscard]] std::size_t max_bits_per_sample() const noexcept {
    return precision_bits_;
  }

  [[nodiscard]] const std::vector<std::uint64_t>& weights() const noexcept {
    return weights_;
  }

  template <class BitSource>
  [[nodiscard]] KnuthYaoSample sample(BitSource&& next_bit) const {
    if (nodes_.empty()) {
      throw std::logic_error("Knuth-Yao sampler has no root");
    }

    std::size_t node_index = 0U;
    std::size_t consumed = 0U;

    for (;;) {
      const Node& node = nodes_[node_index];
      if (node.symbol.has_value()) {
        return KnuthYaoSample{*node.symbol, consumed};
      }

      if (consumed >= precision_bits_) {
        throw std::logic_error(
            "Knuth-Yao traversal exceeded dyadic precision");
      }

      const bool bit = static_cast<bool>(next_bit());
      ++consumed;
      const std::size_t next =
          bit ? node.right : node.left;
      if (next == kNoNode || next >= nodes_.size()) {
        throw std::logic_error(
            "Knuth-Yao traversal reached an invalid child");
      }
      node_index = next;
    }
  }

  // Expensive exact structural replay. Each leaf at depth d contributes
  // total_weight / 2^d units to its symbol. The replay must reconstruct the
  // original integer weights exactly.
  [[nodiscard]] bool valid_structure() const noexcept {
    try {
      if (nodes_.empty()) {
        return false;
      }

      std::vector<bool> seen(nodes_.size(), false);
      std::vector<std::pair<std::size_t, std::size_t>> stack;
      std::vector<std::uint64_t> reconstructed(weights_.size(), 0U);
      stack.emplace_back(0U, 0U);

      std::size_t visited = 0U;
      while (!stack.empty()) {
        const auto [index, depth] = stack.back();
        stack.pop_back();

        if (index >= nodes_.size() || seen[index] ||
            depth > precision_bits_) {
          return false;
        }
        seen[index] = true;
        ++visited;

        const Node& node = nodes_[index];
        if (node.symbol.has_value()) {
          if (node.left != kNoNode || node.right != kNoNode ||
              *node.symbol >= weights_.size()) {
            return false;
          }

          const std::uint64_t mass = total_weight_ >> depth;
          if (mass == 0U ||
              reconstructed[*node.symbol] >
                  std::numeric_limits<std::uint64_t>::max() - mass) {
            return false;
          }
          reconstructed[*node.symbol] += mass;
          continue;
        }

        if (depth >= precision_bits_ ||
            node.left == kNoNode || node.right == kNoNode ||
            node.left >= nodes_.size() || node.right >= nodes_.size() ||
            node.left == node.right) {
          return false;
        }

        stack.emplace_back(node.right, depth + 1U);
        stack.emplace_back(node.left, depth + 1U);
      }

      return visited == nodes_.size() && reconstructed == weights_;
    } catch (...) {
      return false;
    }
  }

 private:
  static constexpr std::size_t kNoNode =
      std::numeric_limits<std::size_t>::max();

  struct Node {
    std::size_t left{kNoNode};
    std::size_t right{kNoNode};
    std::optional<std::size_t> symbol;
  };

  std::vector<std::uint64_t> weights_;
  std::uint64_t total_weight_{};
  std::size_t precision_bits_{};
  std::size_t normalization_shift_{};
  std::vector<Node> nodes_;

  [[nodiscard]] std::size_t append_node() {
    if (nodes_.size() == nodes_.max_size()) {
      throw std::length_error(
          "Knuth-Yao DDG tree exceeds vector capacity");
    }
    nodes_.push_back(Node{});
    return nodes_.size() - 1U;
  }

  void build_tree() {
    nodes_.clear();
    nodes_.push_back(Node{});

    if (precision_bits_ == 0U) {
      std::optional<std::size_t> sole;
      for (std::size_t symbol = 0U; symbol < weights_.size(); ++symbol) {
        if (weights_[symbol] == 0U) {
          continue;
        }
        if (weights_[symbol] != 1U || sole.has_value()) {
          throw std::logic_error(
              "unit Knuth-Yao distribution is not deterministic");
        }
        sole = symbol;
      }
      if (!sole.has_value()) {
        throw std::logic_error(
            "unit Knuth-Yao distribution has no positive symbol");
      }
      nodes_[0U].symbol = *sole;
      return;
    }

    std::vector<std::size_t> frontier{0U};

    for (std::size_t depth = 1U;
         depth <= precision_bits_; ++depth) {
      if (frontier.size() >
          std::numeric_limits<std::size_t>::max() / 2U) {
        throw std::length_error(
            "Knuth-Yao frontier size overflows size_t");
      }

      std::vector<std::size_t> slots;
      slots.reserve(frontier.size() * 2U);

      for (const std::size_t parent : frontier) {
        const std::size_t left = append_node();
        const std::size_t right = append_node();
        nodes_[parent].left = left;
        nodes_[parent].right = right;
        slots.push_back(left);
        slots.push_back(right);
      }

      const std::size_t shift = precision_bits_ - depth;
      std::size_t used = 0U;
      for (std::size_t symbol = 0U;
           symbol < weights_.size(); ++symbol) {
        const std::uint64_t normalized_weight =
            weights_[symbol] >> normalization_shift_;
        if (((normalized_weight >> shift) & UINT64_C(1)) == 0U) {
          continue;
        }
        if (used >= slots.size()) {
          throw std::logic_error(
              "Knuth-Yao probability matrix exceeds available leaves");
        }
        nodes_[slots[used]].symbol = symbol;
        ++used;
      }

      frontier.assign(slots.begin() +
                          static_cast<std::ptrdiff_t>(used),
                      slots.end());
    }

    if (!frontier.empty()) {
      throw std::logic_error(
          "Knuth-Yao dyadic tree leaves unused probability mass");
    }
  }
};

}  // namespace algorithms::randomized
