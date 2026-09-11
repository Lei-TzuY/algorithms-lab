#include "algorithms/strings/palindromic_tree.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace algorithms::strings {

namespace {

bool same_slice(const std::vector<std::uint8_t>& text,
                std::size_t first_start,
                std::size_t second_start,
                std::size_t length) {
  for (std::size_t offset = 0; offset < length; ++offset) {
    if (text[first_start + offset] != text[second_start + offset]) {
      return false;
    }
  }
  return true;
}

bool is_palindrome_slice(const std::vector<std::uint8_t>& text,
                         std::size_t start,
                         std::size_t length) {
  for (std::size_t offset = 0; offset < length / 2U; ++offset) {
    if (text[start + offset] != text[start + length - 1U - offset]) {
      return false;
    }
  }
  return true;
}

}  // namespace

PalindromicTree::PalindromicTree() {
  nodes_.reserve(2U);

  Node odd_root;
  odd_root.palindrome_length = -1;
  odd_root.suffix_link = 0U;
  nodes_.push_back(odd_root);

  Node even_root;
  even_root.palindrome_length = 0;
  even_root.suffix_link = 0U;
  nodes_.push_back(even_root);
}

PalindromicTree::PalindromicTree(std::string_view bytes) : PalindromicTree() {
  const auto maximum_signed_length =
      static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  if (bytes.size() > maximum_signed_length || bytes.size() > text_.max_size() ||
      bytes.size() > nodes_.max_size() - 2U) {
    throw std::length_error("palindromic tree input is too large");
  }

  text_.reserve(bytes.size());
  nodes_.reserve(bytes.size() + 2U);
  for (const char value : bytes) {
    append(static_cast<std::uint8_t>(static_cast<unsigned char>(value)));
  }
}

std::size_t PalindromicTree::find_extendable_suffix(std::size_t node_id,
                                                     std::size_t position,
                                                     std::uint8_t byte) const {
  while (true) {
    const std::ptrdiff_t signed_length = nodes_.at(node_id).palindrome_length;
    if (signed_length < 0) {
      return node_id;
    }

    const auto palindrome_length = static_cast<std::size_t>(signed_length);
    if (position > palindrome_length &&
        text_[position - palindrome_length - 1U] == byte) {
      return node_id;
    }
    node_id = nodes_.at(node_id).suffix_link;
  }
}

std::size_t PalindromicTree::append(std::uint8_t byte) {
  const auto maximum_signed_length =
      static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  if (text_.size() >= maximum_signed_length || text_.size() == text_.max_size()) {
    throw std::length_error("palindromic tree text length overflow");
  }

  const std::size_t position = text_.size();
  const std::size_t parent =
      find_extendable_suffix(longest_suffix_node_, position, byte);
  const std::size_t transition_index = static_cast<std::size_t>(byte);
  const std::size_t existing = nodes_[parent].transitions[transition_index];

  if (existing != 0U) {
    text_.push_back(byte);
    ++nodes_[existing].terminal_occurrences;
    longest_suffix_node_ = existing;
    return existing;
  }

  if (nodes_.size() == nodes_.max_size()) {
    throw std::length_error("palindromic tree node count overflow");
  }

  Node created;
  created.palindrome_length = nodes_[parent].palindrome_length + 2;
  created.first_end = position + 1U;
  created.terminal_occurrences = 1U;

  if (created.palindrome_length == 1) {
    created.suffix_link = 1U;
  } else {
    const std::size_t suffix_parent = find_extendable_suffix(
        nodes_[parent].suffix_link, position, byte);
    created.suffix_link = nodes_[suffix_parent].transitions[transition_index];
    if (created.suffix_link == 0U) {
      throw std::logic_error("palindromic tree suffix transition missing");
    }
  }

  // Reserve before committing the byte so allocation failure cannot leave the
  // logical text and palindrome-node state out of sync. Grow geometrically to
  // avoid turning a stream of newly discovered palindromes into quadratic
  // reallocation work.
  if (nodes_.size() == nodes_.capacity()) {
    const std::size_t maximum = nodes_.max_size();
    const std::size_t current = nodes_.capacity();
    const std::size_t required = nodes_.size() + 1U;
    std::size_t grown = current == 0U ? 4U : current;
    if (grown <= maximum / 2U) {
      grown *= 2U;
    } else {
      grown = maximum;
    }
    if (grown < required) {
      grown = required;
    }
    nodes_.reserve(grown);
  }

  text_.push_back(byte);
  const std::size_t created_id = nodes_.size();
  nodes_.push_back(created);
  nodes_[parent].transitions[transition_index] = created_id;
  longest_suffix_node_ = created_id;
  return created_id;
}

std::size_t PalindromicTree::length() const noexcept { return text_.size(); }

std::size_t PalindromicTree::distinct_palindrome_count() const noexcept {
  return nodes_.size() - 2U;
}

std::size_t PalindromicTree::longest_suffix_length() const noexcept {
  return static_cast<std::size_t>(nodes_[longest_suffix_node_].palindrome_length);
}

std::vector<PalindromeRecord> PalindromicTree::palindromes() const {
  std::vector<std::size_t> occurrences(nodes_.size(), 0U);
  for (std::size_t node_id = 2U; node_id < nodes_.size(); ++node_id) {
    occurrences[node_id] = nodes_[node_id].terminal_occurrences;
  }

  for (std::size_t node_id = nodes_.size(); node_id-- > 2U;) {
    occurrences[nodes_[node_id].suffix_link] += occurrences[node_id];
  }

  std::vector<PalindromeRecord> records;
  records.reserve(distinct_palindrome_count());
  for (std::size_t node_id = 2U; node_id < nodes_.size(); ++node_id) {
    const Node& node = nodes_[node_id];
    const std::size_t palindrome_length =
        static_cast<std::size_t>(node.palindrome_length);
    const std::size_t suffix_length = static_cast<std::size_t>(
        nodes_[node.suffix_link].palindrome_length < 0
            ? 0
            : nodes_[node.suffix_link].palindrome_length);
    records.push_back(PalindromeRecord{
        node_id,
        palindrome_length,
        node.first_end - palindrome_length,
        node.first_end,
        occurrences[node_id],
        suffix_length,
    });
  }
  return records;
}

bool PalindromicTree::valid_structure() const {
  if (nodes_.size() < 2U || longest_suffix_node_ >= nodes_.size()) {
    return false;
  }
  if (nodes_[0].palindrome_length != -1 || nodes_[0].suffix_link != 0U ||
      nodes_[1].palindrome_length != 0 || nodes_[1].suffix_link != 0U) {
    return false;
  }
  if (nodes_[longest_suffix_node_].palindrome_length < 0) {
    return false;
  }

  std::size_t expected_longest_node = 1U;
  std::size_t expected_longest_length = 0U;

  for (std::size_t node_id = 2U; node_id < nodes_.size(); ++node_id) {
    const Node& node = nodes_[node_id];
    if (node.palindrome_length <= 0 || node.suffix_link >= node_id ||
        node.first_end > text_.size() || node.terminal_occurrences == 0U) {
      return false;
    }
    const std::size_t palindrome_length =
        static_cast<std::size_t>(node.palindrome_length);
    if (node.first_end < palindrome_length) {
      return false;
    }
    const std::size_t start = node.first_end - palindrome_length;
    if (!is_palindrome_slice(text_, start, palindrome_length)) {
      return false;
    }

    std::size_t expected_suffix = 1U;
    std::size_t expected_suffix_length = 0U;
    for (std::size_t other = 2U; other < node_id; ++other) {
      const Node& candidate = nodes_[other];
      const std::size_t candidate_length =
          static_cast<std::size_t>(candidate.palindrome_length);

      if (candidate_length == palindrome_length) {
        const std::size_t candidate_start =
            candidate.first_end - candidate_length;
        if (same_slice(text_, start, candidate_start, palindrome_length)) {
          return false;
        }
      }

      if (candidate_length < palindrome_length &&
          candidate_length > expected_suffix_length) {
        const std::size_t candidate_start =
            candidate.first_end - candidate_length;
        if (same_slice(text_, start + palindrome_length - candidate_length,
                       candidate_start, candidate_length)) {
          expected_suffix = other;
          expected_suffix_length = candidate_length;
        }
      }
    }
    if (node.suffix_link != expected_suffix) {
      return false;
    }

    if (palindrome_length <= text_.size()) {
      const std::size_t suffix_start = text_.size() - palindrome_length;
      if (same_slice(text_, suffix_start, start, palindrome_length) &&
          palindrome_length > expected_longest_length) {
        expected_longest_node = node_id;
        expected_longest_length = palindrome_length;
      }
    }
  }

  if (longest_suffix_node_ != expected_longest_node) {
    return false;
  }

  for (std::size_t parent = 0U; parent < nodes_.size(); ++parent) {
    for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
      const std::size_t child = nodes_[parent].transitions[symbol];
      if (child == 0U) {
        continue;
      }
      if (child >= nodes_.size() || child <= parent ||
          nodes_[child].palindrome_length !=
              nodes_[parent].palindrome_length + 2) {
        return false;
      }

      const Node& child_node = nodes_[child];
      const std::size_t child_length =
          static_cast<std::size_t>(child_node.palindrome_length);
      const std::size_t child_start = child_node.first_end - child_length;
      const auto byte = static_cast<std::uint8_t>(symbol);
      if (text_[child_start] != byte ||
          text_[child_node.first_end - 1U] != byte) {
        return false;
      }

      if (nodes_[parent].palindrome_length > 0) {
        const std::size_t parent_length =
            static_cast<std::size_t>(nodes_[parent].palindrome_length);
        const std::size_t parent_start =
            nodes_[parent].first_end - parent_length;
        if (!same_slice(text_, child_start + 1U, parent_start, parent_length)) {
          return false;
        }
      }
    }
  }

  return true;
}

}  // namespace algorithms::strings
