#include "algorithms/strings/palindromic_tree.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using algorithms::strings::PalindromeRecord;
using algorithms::strings::PalindromicTree;
using Bytes = std::vector<std::uint8_t>;

struct ByteVectorLess {
  bool operator()(const Bytes& left, const Bytes& right) const noexcept {
    const std::size_t common =
        left.size() < right.size() ? left.size() : right.size();
    for (std::size_t index = 0; index < common; ++index) {
      if (left[index] < right[index]) {
        return true;
      }
      if (right[index] < left[index]) {
        return false;
      }
    }
    return left.size() < right.size();
  }
};

bool is_palindrome(const Bytes& text, std::size_t begin, std::size_t end) {
  while (begin < end) {
    --end;
    if (text[begin] != text[end]) {
      return false;
    }
    ++begin;
  }
  return true;
}

struct OracleInfo {
  std::size_t occurrences{};
  std::size_t first_start{};
  std::size_t first_end{};
};

using OracleMap = std::map<Bytes, OracleInfo, ByteVectorLess>;
using RecordMap = std::map<Bytes, PalindromeRecord, ByteVectorLess>;

OracleMap brute_palindromes(const Bytes& text) {
  OracleMap result;
  for (std::size_t begin = 0; begin < text.size(); ++begin) {
    for (std::size_t end = begin + 1U; end <= text.size(); ++end) {
      if (!is_palindrome(text, begin, end)) {
        continue;
      }
      Bytes value(text.begin() + static_cast<std::ptrdiff_t>(begin),
                  text.begin() + static_cast<std::ptrdiff_t>(end));
      auto [it, inserted] = result.try_emplace(
          value, OracleInfo{0U, begin, end});
      if (inserted) {
        it->second.first_start = begin;
        it->second.first_end = end;
      }
      ++it->second.occurrences;
    }
  }
  return result;
}

std::size_t longest_proper_palindromic_suffix_length(const Bytes& palindrome) {
  for (std::size_t length = palindrome.size(); length-- > 1U;) {
    const std::size_t start = palindrome.size() - length;
    if (is_palindrome(palindrome, start, palindrome.size())) {
      return length;
    }
  }
  return 0U;
}

Bytes slice(const Bytes& text, std::size_t begin, std::size_t end) {
  return Bytes(text.begin() + static_cast<std::ptrdiff_t>(begin),
               text.begin() + static_cast<std::ptrdiff_t>(end));
}

std::string bytes_to_string(const Bytes& bytes) {
  std::string value;
  value.reserve(bytes.size());
  for (std::uint8_t byte : bytes) {
    value.push_back(static_cast<char>(byte));
  }
  return value;
}

void verify_against_oracle(const Bytes& text) {
  const std::string storage = bytes_to_string(text);
  PalindromicTree tree{std::string_view(storage.data(), storage.size())};
  REQUIRE_EQ(tree.length(), text.size());
  REQUIRE(tree.valid_structure());

  const auto oracle = brute_palindromes(text);
  const auto records = tree.palindromes();
  REQUIRE_EQ(records.size(), oracle.size());
  REQUIRE_EQ(tree.distinct_palindrome_count(), oracle.size());

  RecordMap actual;
  for (const PalindromeRecord& record : records) {
    REQUIRE(record.node_id >= 2U);
    REQUIRE(record.first_end <= text.size());
    REQUIRE(record.first_start <= record.first_end);
    REQUIRE_EQ(record.first_end - record.first_start, record.length);
    const Bytes value = slice(text, record.first_start, record.first_end);
    REQUIRE(actual.emplace(value, record).second);
    REQUIRE_EQ(record.suffix_link_length,
               longest_proper_palindromic_suffix_length(value));
  }

  REQUIRE_EQ(actual.size(), oracle.size());
  for (const auto& [value, expected] : oracle) {
    const auto it = actual.find(value);
    REQUIRE(it != actual.end());
    REQUIRE_EQ(it->second.occurrences, expected.occurrences);
    REQUIRE_EQ(it->second.first_start, expected.first_start);
    REQUIRE_EQ(it->second.first_end, expected.first_end);
  }

  std::size_t expected_suffix = 0U;
  for (std::size_t length = text.size() + 1U; length-- > 1U;) {
    const std::size_t start = text.size() - length;
    if (is_palindrome(text, start, text.size())) {
      expected_suffix = length;
      break;
    }
  }
  REQUIRE_EQ(tree.longest_suffix_length(), expected_suffix);
}

}  // namespace

TEST_CASE(palindromic_tree_deterministic_examples) {
  verify_against_oracle({});
  verify_against_oracle({'a'});
  verify_against_oracle({'a', 'a', 'a', 'a'});
  verify_against_oracle({'a', 'b', 'a', 'b', 'a'});
  verify_against_oracle({'a', 'b', 'c', 'd'});
  verify_against_oracle({0U, 255U, 0U, 128U, 0U, 255U, 0U});
}

TEST_CASE(palindromic_tree_online_suffix_nodes) {
  PalindromicTree tree;
  const std::vector<std::uint8_t> text{'a', 'b', 'a', 'b', 'a'};
  const std::vector<std::size_t> expected_lengths{1U, 1U, 3U, 3U, 5U};
  std::vector<std::size_t> node_ids;
  for (std::size_t i = 0; i < text.size(); ++i) {
    node_ids.push_back(tree.append(text[i]));
    REQUIRE_EQ(tree.longest_suffix_length(), expected_lengths[i]);
    REQUIRE(tree.valid_structure());
  }
  REQUIRE(node_ids[2] != node_ids[4]);
  const std::size_t repeated = tree.append(static_cast<std::uint8_t>('b'));
  REQUIRE(repeated < tree.distinct_palindrome_count() + 2U);
  REQUIRE(tree.valid_structure());
}

TEST_CASE(palindromic_tree_known_occurrence_multiplicity) {
  const Bytes text{'a', 'b', 'a', 'b', 'a'};
  PalindromicTree tree("ababa");
  const auto records = tree.palindromes();
  std::map<std::string, std::size_t> counts;
  for (const auto& record : records) {
    const Bytes value = slice(text, record.first_start, record.first_end);
    counts[bytes_to_string(value)] = record.occurrences;
  }
  REQUIRE_EQ(counts.at("a"), 3U);
  REQUIRE_EQ(counts.at("b"), 2U);
  REQUIRE_EQ(counts.at("aba"), 2U);
  REQUIRE_EQ(counts.at("bab"), 1U);
  REQUIRE_EQ(counts.at("ababa"), 1U);
}

TEST_CASE(palindromic_tree_randomized_differential) {
  std::mt19937_64 rng(0xE371EEULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 25U);
    Bytes text;
    text.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
      const std::uint64_t draw = rng() % 10U;
      if (draw == 8U) {
        text.push_back(0U);
      } else if (draw == 9U) {
        text.push_back(255U);
      } else {
        text.push_back(static_cast<std::uint8_t>(draw % 5U));
      }
    }
    verify_against_oracle(text);
  }
}
