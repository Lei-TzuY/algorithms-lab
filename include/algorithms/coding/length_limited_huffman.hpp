#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::coding {

class LengthLimitedHuffmanByteCodebook {
 public:
  explicit LengthLimitedHuffmanByteCodebook(
      const std::array<std::uint64_t, 256>& frequencies,
      std::size_t maximum_code_length)
      : frequencies_(frequencies), maximum_code_length_(maximum_code_length) {
    build();
  }

  [[nodiscard]] std::size_t symbol_count() const noexcept { return symbol_count_; }
  [[nodiscard]] std::size_t maximum_code_length() const noexcept {
    return maximum_code_length_;
  }
  [[nodiscard]] std::uint64_t weighted_bit_count() const noexcept {
    return weighted_bit_count_;
  }
  [[nodiscard]] const std::array<std::uint64_t, 256>& frequencies() const noexcept {
    return frequencies_;
  }
  [[nodiscard]] const std::array<std::size_t, 256>& code_lengths() const noexcept {
    return code_lengths_;
  }
  [[nodiscard]] std::optional<std::string_view> code(std::uint8_t symbol) const noexcept {
    if (frequencies_[symbol] == 0U) {
      return std::nullopt;
    }
    return std::string_view(codes_[symbol]);
  }

  [[nodiscard]] bool valid_codebook() const {
    std::size_t counted = 0U;
    std::uint64_t cost = 0U;
    for (std::size_t symbol = 0; symbol < frequencies_.size(); ++symbol) {
      if (frequencies_[symbol] == 0U) {
        if (code_lengths_[symbol] != 0U || !codes_[symbol].empty()) {
          return false;
        }
        continue;
      }
      ++counted;
      if (code_lengths_[symbol] == 0U ||
          code_lengths_[symbol] > maximum_code_length_ ||
          codes_[symbol].size() != code_lengths_[symbol]) {
        return false;
      }
      if (!std::all_of(codes_[symbol].begin(), codes_[symbol].end(),
                       [](char bit) { return bit == '0' || bit == '1'; })) {
        return false;
      }
      const std::uint64_t contribution =
          checked_mul(frequencies_[symbol], code_lengths_[symbol]);
      cost = checked_add(cost, contribution);
    }
    if (counted != symbol_count_ || cost != weighted_bit_count_) {
      return false;
    }

    for (std::size_t first = 0; first < codes_.size(); ++first) {
      if (frequencies_[first] == 0U) {
        continue;
      }
      for (std::size_t second = 0; second < codes_.size(); ++second) {
        if (first == second || frequencies_[second] == 0U) {
          continue;
        }
        if (codes_[second].size() >= codes_[first].size() &&
            codes_[second].compare(0U, codes_[first].size(), codes_[first]) == 0) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  struct Cost {
    std::uint64_t value{};
    bool overflow{};
  };

  struct Node {
    Cost cost;
    std::optional<std::uint8_t> symbol;
    std::optional<std::size_t> left;
    std::optional<std::size_t> right;
  };

  [[nodiscard]] static Cost add_cost(Cost first, Cost second) noexcept {
    if (first.overflow || second.overflow ||
        second.value > std::numeric_limits<std::uint64_t>::max() - first.value) {
      return Cost{std::numeric_limits<std::uint64_t>::max(), true};
    }
    return Cost{first.value + second.value, false};
  }

  [[nodiscard]] static std::uint64_t checked_add(std::uint64_t first,
                                                  std::uint64_t second) {
    if (second > std::numeric_limits<std::uint64_t>::max() - first) {
      throw std::overflow_error(
          "length-limited Huffman weighted bit count is not uint64_t-representable");
    }
    return first + second;
  }

  [[nodiscard]] static std::uint64_t checked_mul(std::uint64_t value,
                                                  std::size_t factor) {
    if (factor != 0U &&
        value > std::numeric_limits<std::uint64_t>::max() /
                    static_cast<std::uint64_t>(factor)) {
      throw std::overflow_error(
          "length-limited Huffman weighted bit count is not uint64_t-representable");
    }
    return value * static_cast<std::uint64_t>(factor);
  }

  static void increment_binary(std::string& bits) {
    for (std::size_t index = bits.size(); index-- > 0U;) {
      if (bits[index] == '0') {
        bits[index] = '1';
        std::fill(bits.begin() + static_cast<std::ptrdiff_t>(index + 1U),
                  bits.end(), '0');
        return;
      }
    }
    throw std::logic_error("canonical Huffman code overflow");
  }

  void build() {
    std::vector<std::uint8_t> active_symbols;
    active_symbols.reserve(frequencies_.size());
    for (std::size_t symbol = 0; symbol < frequencies_.size(); ++symbol) {
      if (frequencies_[symbol] != 0U) {
        active_symbols.push_back(static_cast<std::uint8_t>(symbol));
      }
    }
    symbol_count_ = active_symbols.size();
    if (symbol_count_ == 0U) {
      return;
    }
    if (maximum_code_length_ == 0U) {
      throw std::invalid_argument(
          "non-empty length-limited Huffman code requires positive maximum length");
    }
    if (symbol_count_ == 1U) {
      const std::uint8_t symbol = active_symbols.front();
      code_lengths_[symbol] = 1U;
      codes_[symbol] = "0";
      weighted_bit_count_ = frequencies_[symbol];
      return;
    }

    const std::size_t effective_limit =
        std::min(maximum_code_length_, symbol_count_ - 1U);

    nodes_.reserve(symbol_count_ * effective_limit * 2U);
    std::vector<std::size_t> leaves;
    leaves.reserve(symbol_count_);
    for (const std::uint8_t symbol : active_symbols) {
      nodes_.push_back(Node{Cost{frequencies_[symbol], false}, symbol,
                            std::nullopt, std::nullopt});
      leaves.push_back(nodes_.size() - 1U);
    }

    const auto less_node = [this](std::size_t first, std::size_t second) {
      const Cost a = nodes_[first].cost;
      const Cost b = nodes_[second].cost;
      if (a.overflow != b.overflow) {
        return !a.overflow;
      }
      if (!a.overflow && a.value != b.value) {
        return a.value < b.value;
      }
      if (nodes_[first].symbol.has_value() && nodes_[second].symbol.has_value()) {
        return *nodes_[first].symbol < *nodes_[second].symbol;
      }
      if (nodes_[first].symbol.has_value() != nodes_[second].symbol.has_value()) {
        return nodes_[first].symbol.has_value();
      }
      return first < second;
    };
    std::sort(leaves.begin(), leaves.end(), less_node);

    std::vector<std::size_t> current = leaves;
    for (std::size_t level = 1U; level < effective_limit; ++level) {
      std::vector<std::size_t> packages;
      packages.reserve(current.size() / 2U);
      for (std::size_t index = 0U; index + 1U < current.size(); index += 2U) {
        const std::size_t left = current[index];
        const std::size_t right = current[index + 1U];
        nodes_.push_back(Node{add_cost(nodes_[left].cost, nodes_[right].cost),
                              std::nullopt, left, right});
        packages.push_back(nodes_.size() - 1U);
      }
      std::sort(packages.begin(), packages.end(), less_node);

      std::vector<std::size_t> merged;
      merged.reserve(leaves.size() + packages.size());
      std::merge(leaves.begin(), leaves.end(), packages.begin(), packages.end(),
                 std::back_inserter(merged), less_node);
      current = std::move(merged);
    }

    const std::size_t needed = 2U * symbol_count_ - 2U;
    if (current.size() < needed) {
      throw std::invalid_argument(
          "maximum Huffman code length cannot represent all positive-frequency symbols");
    }

    std::vector<std::size_t> stack;
    stack.reserve(symbol_count_ * effective_limit);
    for (std::size_t index = 0U; index < needed; ++index) {
      stack.push_back(current[index]);
    }
    while (!stack.empty()) {
      const std::size_t node_index = stack.back();
      stack.pop_back();
      const Node& node = nodes_[node_index];
      if (node.symbol.has_value()) {
        ++code_lengths_[*node.symbol];
        continue;
      }
      if (!node.left.has_value() || !node.right.has_value()) {
        throw std::logic_error("Package-Merge package is incomplete");
      }
      stack.push_back(*node.left);
      stack.push_back(*node.right);
    }

    for (const std::uint8_t symbol : active_symbols) {
      if (code_lengths_[symbol] == 0U ||
          code_lengths_[symbol] > effective_limit) {
        throw std::logic_error("Package-Merge produced an invalid code length");
      }
    }

    std::vector<std::uint8_t> canonical_order = active_symbols;
    std::sort(canonical_order.begin(), canonical_order.end(),
              [this](std::uint8_t first, std::uint8_t second) {
                return std::tie(code_lengths_[first], first) <
                       std::tie(code_lengths_[second], second);
              });

    std::string current_code(code_lengths_[canonical_order.front()], '0');
    codes_[canonical_order.front()] = current_code;
    for (std::size_t index = 1U; index < canonical_order.size(); ++index) {
      increment_binary(current_code);
      const std::size_t next_length = code_lengths_[canonical_order[index]];
      if (next_length < current_code.size()) {
        throw std::logic_error("canonical Huffman lengths are not nondecreasing");
      }
      current_code.append(next_length - current_code.size(), '0');
      codes_[canonical_order[index]] = current_code;
    }

    std::uint64_t cost = 0U;
    for (const std::uint8_t symbol : active_symbols) {
      cost = checked_add(cost, checked_mul(frequencies_[symbol],
                                           code_lengths_[symbol]));
    }
    weighted_bit_count_ = cost;

    if (!valid_codebook()) {
      throw std::logic_error("constructed length-limited Huffman codebook is invalid");
    }
  }

  std::array<std::uint64_t, 256> frequencies_{};
  std::size_t maximum_code_length_{};
  std::array<std::size_t, 256> code_lengths_{};
  std::array<std::string, 256> codes_{};
  std::vector<Node> nodes_;
  std::size_t symbol_count_{};
  std::uint64_t weighted_bit_count_{};
};

}  // namespace algorithms::coding
