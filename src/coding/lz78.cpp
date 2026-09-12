#include "algorithms/coding/lz78.hpp"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {
namespace {

constexpr std::size_t kNoTransition = std::numeric_limits<std::size_t>::max();

struct TrieNode {
  std::size_t phrase_index = 0;
  std::vector<std::pair<std::uint8_t, std::size_t>> transitions;
};

std::size_t find_transition(const TrieNode& node, std::uint8_t byte) {
  for (const auto& [label, destination] : node.transitions) {
    if (label == byte) return destination;
  }
  return kNoTransition;
}

struct DecodeEntry {
  std::size_t prefix_index;
  std::uint8_t byte;
};

void append_phrase(std::size_t phrase_index,
                   const std::vector<DecodeEntry>& dictionary,
                   std::string& output, std::vector<std::uint8_t>& scratch) {
  scratch.clear();
  while (phrase_index != 0) {
    const DecodeEntry& entry = dictionary[phrase_index - 1];
    scratch.push_back(entry.byte);
    phrase_index = entry.prefix_index;
  }
  for (auto it = scratch.rbegin(); it != scratch.rend(); ++it) {
    output.push_back(static_cast<char>(*it));
  }
}

}  // namespace

std::vector<Lz78Codeword> lz78_encode(std::string_view input) {
  std::vector<Lz78Codeword> codewords;
  if (input.empty()) return codewords;

  std::vector<TrieNode> trie(1);
  std::size_t next_phrase_index = 1;
  std::size_t position = 0;

  while (position < input.size()) {
    std::size_t node_index = 0;
    std::size_t scan = position;
    while (scan < input.size()) {
      const auto byte = static_cast<std::uint8_t>(
          static_cast<unsigned char>(input[scan]));
      const std::size_t next = find_transition(trie[node_index], byte);
      if (next == kNoTransition) break;
      node_index = next;
      ++scan;
    }

    const std::size_t prefix_index = trie[node_index].phrase_index;
    if (scan == input.size()) {
      codewords.push_back(Lz78Codeword{prefix_index, std::nullopt});
      break;
    }

    const auto next_byte = static_cast<std::uint8_t>(
        static_cast<unsigned char>(input[scan]));
    codewords.push_back(Lz78Codeword{prefix_index, next_byte});

    const std::size_t new_node = trie.size();
    trie.push_back(TrieNode{next_phrase_index, {}});
    trie[node_index].transitions.push_back({next_byte, new_node});
    ++next_phrase_index;
    position = scan + 1;
  }

  return codewords;
}

std::string lz78_decode(std::span<const Lz78Codeword> codewords) {
  std::vector<DecodeEntry> dictionary;
  dictionary.reserve(codewords.size());
  std::string output;
  std::vector<std::uint8_t> scratch;

  for (std::size_t index = 0; index < codewords.size(); ++index) {
    const Lz78Codeword& codeword = codewords[index];
    if (codeword.prefix_index > dictionary.size()) {
      throw std::invalid_argument("LZ78 prefix index references a future phrase");
    }

    if (!codeword.next_byte.has_value()) {
      if (index + 1 != codewords.size()) {
        throw std::invalid_argument("LZ78 terminal codeword must be last");
      }
      if (codeword.prefix_index == 0) {
        throw std::invalid_argument(
            "LZ78 terminal codeword must reference a non-empty phrase");
      }
      append_phrase(codeword.prefix_index, dictionary, output, scratch);
      continue;
    }

    append_phrase(codeword.prefix_index, dictionary, output, scratch);
    output.push_back(static_cast<char>(*codeword.next_byte));
    dictionary.push_back(
        DecodeEntry{codeword.prefix_index, *codeword.next_byte});
  }

  return output;
}

}  // namespace algorithms::coding
