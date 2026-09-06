#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace algorithms::data_structures {

// Byte-oriented multiset trie. Keys are arbitrary byte strings: empty keys,
// embedded nulls, and high-bit bytes are all ordinary input.
//
// Node invariant:
//   subtree_count = terminal_count + sum(child.subtree_count)
// where counts include duplicate insertions. The root represents the empty
// prefix, so root.subtree_count is the total inserted multiplicity.
//
// insert/count/count_prefix: O(key length), with a fixed 256-way byte alphabet.
class ByteTrie {
 public:
  ByteTrie() : nodes_(1) {}

  [[nodiscard]] std::size_t total_count() const noexcept {
    return nodes_[0].subtree_count;
  }

  void insert(std::string_view key) {
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();

    // Discover the already-existing prefix first. Every existing path node
    // will gain one subtree count, so reject counter overflow before changing
    // topology or counts.
    std::size_t current = 0;
    std::size_t existing_length = 0;
    if (nodes_[0].subtree_count == maximum) {
      throw std::overflow_error("trie multiplicity overflow");
    }

    while (existing_length < key.size()) {
      const std::size_t edge = byte_index(key[existing_length]);
      const std::size_t child = nodes_[current].children[edge];
      if (child == npos) {
        break;
      }
      current = child;
      ++existing_length;
      if (nodes_[current].subtree_count == maximum) {
        throw std::overflow_error("trie multiplicity overflow");
      }
    }

    if (existing_length == key.size() &&
        nodes_[current].terminal_count == maximum) {
      throw std::overflow_error("trie terminal multiplicity overflow");
    }

    const std::size_t missing = key.size() - existing_length;
    if (missing > maximum - nodes_.size()) {
      throw std::length_error("trie node count is too large");
    }

    // Reserve all missing suffix nodes before publishing any new edge. Once
    // reserve succeeds, Node construction and index assignments are non-
    // allocating operations, so an allocation failure cannot leave a partial
    // key path visible.
    nodes_.reserve(nodes_.size() + missing);
    for (std::size_t position = existing_length; position < key.size();
         ++position) {
      const std::size_t edge = byte_index(key[position]);
      const std::size_t new_node = nodes_.size();
      nodes_.emplace_back();
      nodes_[current].children[edge] = new_node;
      current = new_node;
    }

    // All existing counters that will change were preflighted above; newly
    // created nodes start at zero. Apply the single insertion along the path.
    current = 0;
    ++nodes_[current].subtree_count;
    for (char byte : key) {
      current = nodes_[current].children[byte_index(byte)];
      ++nodes_[current].subtree_count;
    }
    ++nodes_[current].terminal_count;
  }

  [[nodiscard]] std::size_t count(std::string_view key) const noexcept {
    const std::size_t node = find_node(key);
    return node == npos ? 0U : nodes_[node].terminal_count;
  }

  [[nodiscard]] bool contains(std::string_view key) const noexcept {
    return count(key) != 0U;
  }

  [[nodiscard]] std::size_t count_prefix(
      std::string_view prefix) const noexcept {
    const std::size_t node = find_node(prefix);
    return node == npos ? 0U : nodes_[node].subtree_count;
  }

 private:
  static constexpr std::size_t npos =
      std::numeric_limits<std::size_t>::max();

  struct Node {
    std::array<std::size_t, 256> children{};
    std::size_t terminal_count = 0;
    std::size_t subtree_count = 0;

    Node() { children.fill(npos); }
  };

  static std::size_t byte_index(char value) noexcept {
    return static_cast<std::size_t>(static_cast<unsigned char>(value));
  }

  [[nodiscard]] std::size_t find_node(std::string_view key) const noexcept {
    std::size_t current = 0;
    for (char byte : key) {
      const std::size_t child = nodes_[current].children[byte_index(byte)];
      if (child == npos) {
        return npos;
      }
      current = child;
    }
    return current;
  }

  std::vector<Node> nodes_;
};

}  // namespace algorithms::data_structures
