#include "algorithms/coding/huffman_byte_code.hpp"

#include "algorithms/data_structures/binary_heap.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace algorithms::coding {
namespace {

std::uint64_t checked_add(std::uint64_t first, std::uint64_t second,
                          const char* message) {
  if (second > std::numeric_limits<std::uint64_t>::max() - first) {
    throw std::overflow_error(message);
  }
  return first + second;
}

std::uint64_t checked_mul(std::uint64_t value, std::size_t factor,
                          const char* message) {
  if (factor != 0U &&
      value > std::numeric_limits<std::uint64_t>::max() /
                  static_cast<std::uint64_t>(factor)) {
    throw std::overflow_error(message);
  }
  return value * static_cast<std::uint64_t>(factor);
}

struct QueueEntry {
  std::uint64_t weight{};
  std::uint16_t min_symbol{};
  std::size_t node{};
};

struct QueueCompare {
  bool operator()(const QueueEntry& first, const QueueEntry& second) const {
    return std::tie(first.weight, first.min_symbol, first.node) <
           std::tie(second.weight, second.min_symbol, second.node);
  }
};

}  // namespace

HuffmanByteCodebook::HuffmanByteCodebook(
    const std::array<std::uint64_t, 256>& frequencies)
    : frequencies_(frequencies) {
  using Heap = algorithms::data_structures::BinaryHeap<QueueEntry, QueueCompare>;
  Heap heap;
  nodes_.reserve(511U);

  for (std::size_t symbol = 0; symbol < frequencies_.size(); ++symbol) {
    const std::uint64_t weight = frequencies_[symbol];
    if (weight == 0U) {
      continue;
    }
    ++symbol_count_;
    const auto byte = static_cast<std::uint8_t>(symbol);
    nodes_.push_back(Node{weight, static_cast<std::uint16_t>(symbol), byte,
                          std::nullopt, std::nullopt});
    heap.push(QueueEntry{weight, static_cast<std::uint16_t>(symbol),
                         nodes_.size() - 1U});
  }

  if (heap.empty()) {
    return;
  }

  if (heap.size() == 1U) {
    root_ = heap.pop().node;
    const auto symbol = nodes_[*root_].symbol;
    if (!symbol.has_value()) {
      throw std::logic_error("single Huffman root must be a leaf");
    }
    codes_[*symbol] = "0";
    weighted_bit_count_ = frequencies_[*symbol];
    return;
  }

  while (heap.size() > 1U) {
    const QueueEntry left = heap.pop();
    const QueueEntry right = heap.pop();
    const std::uint64_t merged_weight =
        checked_add(left.weight, right.weight,
                    "Huffman aggregate frequency is not uint64_t-representable");
    const std::uint16_t min_symbol = std::min(left.min_symbol, right.min_symbol);
    nodes_.push_back(Node{merged_weight, min_symbol, std::nullopt, left.node,
                          right.node});
    heap.push(QueueEntry{merged_weight, min_symbol, nodes_.size() - 1U});
  }
  root_ = heap.pop().node;

  struct Pending {
    std::size_t node{};
    std::string code;
  };
  std::vector<Pending> stack;
  stack.push_back(Pending{*root_, {}});
  while (!stack.empty()) {
    Pending current = std::move(stack.back());
    stack.pop_back();
    const Node& node = nodes_[current.node];
    if (node.symbol.has_value()) {
      if (current.code.empty()) {
        throw std::logic_error("multi-symbol Huffman leaf has empty code");
      }
      codes_[*node.symbol] = std::move(current.code);
      continue;
    }
    if (!node.left.has_value() || !node.right.has_value()) {
      throw std::logic_error("Huffman internal node is incomplete");
    }
    std::string right_code = current.code;
    right_code.push_back('1');
    current.code.push_back('0');
    stack.push_back(Pending{*node.right, std::move(right_code)});
    stack.push_back(Pending{*node.left, std::move(current.code)});
  }

  std::uint64_t cost = 0U;
  for (std::size_t symbol = 0; symbol < frequencies_.size(); ++symbol) {
    if (frequencies_[symbol] == 0U) {
      continue;
    }
    const std::uint64_t contribution =
        checked_mul(frequencies_[symbol], codes_[symbol].size(),
                    "Huffman weighted bit count is not uint64_t-representable");
    cost = checked_add(cost, contribution,
                       "Huffman weighted bit count is not uint64_t-representable");
  }
  weighted_bit_count_ = cost;
}

std::size_t HuffmanByteCodebook::symbol_count() const noexcept {
  return symbol_count_;
}

std::uint64_t HuffmanByteCodebook::weighted_bit_count() const noexcept {
  return weighted_bit_count_;
}

const std::array<std::uint64_t, 256>& HuffmanByteCodebook::frequencies() const noexcept {
  return frequencies_;
}

std::optional<std::string_view> HuffmanByteCodebook::code(
    std::uint8_t symbol) const noexcept {
  if (frequencies_[symbol] == 0U) {
    return std::nullopt;
  }
  return std::string_view(codes_[symbol]);
}

std::string HuffmanByteCodebook::encode_bits(
    std::span<const std::uint8_t> input) const {
  std::size_t total_bits = 0U;
  for (const std::uint8_t symbol : input) {
    if (frequencies_[symbol] == 0U) {
      throw std::invalid_argument("input symbol has no Huffman code");
    }
    const std::size_t length = codes_[symbol].size();
    if (length > std::numeric_limits<std::size_t>::max() - total_bits) {
      throw std::length_error("encoded Huffman bit string is too large");
    }
    total_bits += length;
  }

  std::string bits;
  bits.reserve(total_bits);
  for (const std::uint8_t symbol : input) {
    bits += codes_[symbol];
  }
  return bits;
}

std::vector<std::uint8_t> HuffmanByteCodebook::decode_bits(
    std::string_view bits) const {
  if (!root_.has_value()) {
    if (!bits.empty()) {
      throw std::invalid_argument("cannot decode non-empty bits with empty codebook");
    }
    return {};
  }

  const Node& root = nodes_[*root_];
  if (root.symbol.has_value()) {
    std::vector<std::uint8_t> output;
    output.reserve(bits.size());
    for (const char bit : bits) {
      if (bit != '0') {
        throw std::invalid_argument("single-symbol Huffman stream must contain only zero bits");
      }
      output.push_back(*root.symbol);
    }
    return output;
  }

  std::vector<std::uint8_t> output;
  output.reserve(bits.size());
  std::size_t current = *root_;
  for (const char bit : bits) {
    if (bit != '0' && bit != '1') {
      throw std::invalid_argument("Huffman bit stream contains a non-binary character");
    }
    const Node& node = nodes_[current];
    const auto next = bit == '0' ? node.left : node.right;
    if (!next.has_value()) {
      throw std::invalid_argument("Huffman bit stream follows an invalid edge");
    }
    current = *next;
    const Node& reached = nodes_[current];
    if (reached.symbol.has_value()) {
      output.push_back(*reached.symbol);
      current = *root_;
    }
  }
  if (current != *root_) {
    throw std::invalid_argument("Huffman bit stream ends inside a codeword");
  }
  return output;
}

bool HuffmanByteCodebook::valid_codebook() const {
  std::size_t counted_symbols = 0U;
  std::uint64_t cost = 0U;
  for (std::size_t symbol = 0; symbol < frequencies_.size(); ++symbol) {
    if (frequencies_[symbol] == 0U) {
      if (!codes_[symbol].empty()) {
        return false;
      }
      continue;
    }
    ++counted_symbols;
    if (codes_[symbol].empty()) {
      return false;
    }
    if (!std::all_of(codes_[symbol].begin(), codes_[symbol].end(),
                     [](char bit) { return bit == '0' || bit == '1'; })) {
      return false;
    }
    if (codes_[symbol].size() >
        std::numeric_limits<std::uint64_t>::max() / frequencies_[symbol]) {
      return false;
    }
    const std::uint64_t contribution =
        frequencies_[symbol] * static_cast<std::uint64_t>(codes_[symbol].size());
    if (contribution > std::numeric_limits<std::uint64_t>::max() - cost) {
      return false;
    }
    cost += contribution;
  }
  if (counted_symbols != symbol_count_ || cost != weighted_bit_count_) {
    return false;
  }
  if (counted_symbols == 0U) {
    return !root_.has_value() && nodes_.empty();
  }
  if (!root_.has_value() || *root_ >= nodes_.size()) {
    return false;
  }

  for (std::size_t first = 0; first < codes_.size(); ++first) {
    if (frequencies_[first] == 0U) {
      continue;
    }
    for (std::size_t second = first + 1U; second < codes_.size(); ++second) {
      if (frequencies_[second] == 0U) {
        continue;
      }
      const std::string& a = codes_[first];
      const std::string& b = codes_[second];
      const std::size_t shared = std::min(a.size(), b.size());
      if (a.compare(0U, shared, b, 0U, shared) == 0) {
        return false;
      }
    }
  }

  std::vector<bool> visited(nodes_.size(), false);
  std::vector<std::size_t> stack{*root_};
  std::size_t leaf_count = 0U;
  while (!stack.empty()) {
    const std::size_t index = stack.back();
    stack.pop_back();
    if (index >= nodes_.size() || visited[index]) {
      return false;
    }
    visited[index] = true;
    const Node& node = nodes_[index];
    if (node.symbol.has_value()) {
      ++leaf_count;
      if (frequencies_[*node.symbol] == 0U ||
          node.weight != frequencies_[*node.symbol] ||
          node.min_symbol != static_cast<std::uint16_t>(*node.symbol)) {
        return false;
      }
      if (node.left.has_value() || node.right.has_value()) {
        return false;
      }
    } else {
      if (!node.left.has_value() || !node.right.has_value() ||
          *node.left >= nodes_.size() || *node.right >= nodes_.size()) {
        return false;
      }
      const Node& left = nodes_[*node.left];
      const Node& right = nodes_[*node.right];
      if (right.weight > std::numeric_limits<std::uint64_t>::max() - left.weight ||
          node.weight != left.weight + right.weight ||
          node.min_symbol != std::min(left.min_symbol, right.min_symbol)) {
        return false;
      }
      stack.push_back(*node.left);
      stack.push_back(*node.right);
    }
  }
  if (leaf_count != symbol_count_ ||
      !std::all_of(visited.begin(), visited.end(), [](bool value) { return value; })) {
    return false;
  }

  if (symbol_count_ == 1U) {
    const Node& only = nodes_[*root_];
    return only.symbol.has_value() && codes_[*only.symbol] == "0";
  }
  for (std::size_t symbol = 0; symbol < codes_.size(); ++symbol) {
    if (frequencies_[symbol] == 0U) {
      continue;
    }
    std::size_t current = *root_;
    const std::string& bits = codes_[symbol];
    for (std::size_t offset = 0; offset < bits.size(); ++offset) {
      const Node& node = nodes_[current];
      if (node.symbol.has_value()) {
        return false;
      }
      const auto next = bits[offset] == '0' ? node.left : node.right;
      if (!next.has_value()) {
        return false;
      }
      current = *next;
      if (offset + 1U < bits.size() && nodes_[current].symbol.has_value()) {
        return false;
      }
    }
    const Node& leaf = nodes_[current];
    if (!leaf.symbol.has_value() ||
        *leaf.symbol != static_cast<std::uint8_t>(symbol)) {
      return false;
    }
  }
  return true;
}

}  // namespace algorithms::coding
