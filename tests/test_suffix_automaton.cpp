#include "algorithms/strings/suffix_automaton.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::map<std::string, std::size_t> enumerate_substrings(const std::string& text) {
  std::map<std::string, std::size_t> counts;
  counts[""] = text.size() + 1U;
  for (std::size_t begin = 0; begin < text.size(); ++begin) {
    for (std::size_t end = begin + 1U; end <= text.size(); ++end) {
      ++counts[text.substr(begin, end - begin)];
    }
  }
  return counts;
}

std::size_t naive_occurrence_count(std::string_view text,
                                   std::string_view pattern) {
  if (pattern.empty()) {
    return text.size() + 1U;
  }
  if (pattern.size() > text.size()) {
    return 0U;
  }
  std::size_t count = 0U;
  for (std::size_t begin = 0; begin + pattern.size() <= text.size(); ++begin) {
    if (text.substr(begin, pattern.size()) == pattern) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST_CASE(suffix_automaton_empty_and_classic_counts) {
  using algorithms::strings::SuffixAutomatonByteIndex;

  const SuffixAutomatonByteIndex empty("");
  REQUIRE_EQ(empty.state_count(), 1U);
  REQUIRE_EQ(empty.text_length(), 0U);
  REQUIRE_EQ(empty.distinct_substring_count(), 0U);
  REQUIRE(empty.contains(""));
  REQUIRE_EQ(empty.occurrence_count(""), 1U);
  REQUIRE(!empty.contains("a"));
  REQUIRE_EQ(empty.occurrence_count("a"), 0U);

  const SuffixAutomatonByteIndex banana("banana");
  REQUIRE_EQ(banana.text_length(), 6U);
  REQUIRE_EQ(banana.distinct_substring_count(), 15U);
  REQUIRE_EQ(banana.occurrence_count(""), 7U);
  REQUIRE_EQ(banana.occurrence_count("a"), 3U);
  REQUIRE_EQ(banana.occurrence_count("ana"), 2U);
  REQUIRE_EQ(banana.occurrence_count("na"), 2U);
  REQUIRE_EQ(banana.occurrence_count("banana"), 1U);
  REQUIRE_EQ(banana.occurrence_count("bananas"), 0U);
  REQUIRE(banana.state_count() <= 2U * banana.text_length());
}

TEST_CASE(suffix_automaton_repeated_and_arbitrary_bytes) {
  using algorithms::strings::SuffixAutomatonByteIndex;

  const SuffixAutomatonByteIndex repeated("aaaa");
  REQUIRE_EQ(repeated.distinct_substring_count(), 4U);
  REQUIRE_EQ(repeated.occurrence_count("a"), 4U);
  REQUIRE_EQ(repeated.occurrence_count("aa"), 3U);
  REQUIRE_EQ(repeated.occurrence_count("aaa"), 2U);
  REQUIRE_EQ(repeated.occurrence_count("aaaa"), 1U);

  std::string bytes;
  bytes.push_back('\0');
  bytes.push_back(static_cast<char>(0xff));
  bytes.push_back('\0');
  bytes.push_back(static_cast<char>(0x80));
  bytes.push_back(static_cast<char>(0xff));
  bytes.push_back('\0');
  const SuffixAutomatonByteIndex index(bytes);
  const auto oracle = enumerate_substrings(bytes);
  REQUIRE_EQ(index.distinct_substring_count(), oracle.size() - 1U);
  for (const auto& [pattern, count] : oracle) {
    REQUIRE(index.contains(pattern));
    REQUIRE_EQ(index.occurrence_count(pattern), count);
  }
}

TEST_CASE(suffix_automaton_deterministic_state_shape_and_queries) {
  using algorithms::strings::SuffixAutomatonByteIndex;

  const std::string text = "abracadabra";
  const SuffixAutomatonByteIndex first(text);
  const SuffixAutomatonByteIndex second(text);
  REQUIRE_EQ(first.state_count(), second.state_count());
  REQUIRE_EQ(first.distinct_substring_count(), second.distinct_substring_count());

  const std::vector<std::string> patterns = {
      "", "abra", "cad", "ra", "abracadabra", "xyz", "a", "br"};
  for (const auto& pattern : patterns) {
    REQUIRE_EQ(first.contains(pattern), second.contains(pattern));
    REQUIRE_EQ(first.occurrence_count(pattern), second.occurrence_count(pattern));
    REQUIRE_EQ(first.occurrence_count(pattern),
               naive_occurrence_count(text, pattern));
  }
}

TEST_CASE(suffix_automaton_randomized_against_exhaustive_substring_map) {
  using algorithms::strings::SuffixAutomatonByteIndex;

  std::mt19937_64 random(0x5A17A0ULL);
  std::uniform_int_distribution<std::size_t> length_distribution(0U, 18U);
  std::uniform_int_distribution<unsigned int> alphabet_distribution(0U, 7U);
  std::uniform_int_distribution<unsigned int> query_length_distribution(0U, 8U);

  for (std::size_t trial = 0; trial < 900U; ++trial) {
    const std::size_t length = length_distribution(random);
    std::string text;
    text.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
      const unsigned int symbol = alphabet_distribution(random);
      unsigned char byte = 0U;
      if (symbol < 4U) {
        byte = static_cast<unsigned char>('a' + symbol);
      } else if (symbol == 4U) {
        byte = 0U;
      } else if (symbol == 5U) {
        byte = 0x80U;
      } else if (symbol == 6U) {
        byte = 0xffU;
      } else {
        byte = 0x7fU;
      }
      text.push_back(static_cast<char>(byte));
    }

    const SuffixAutomatonByteIndex index(text);
    const auto oracle = enumerate_substrings(text);
    REQUIRE_EQ(index.text_length(), text.size());
    REQUIRE_EQ(index.distinct_substring_count(), oracle.size() - 1U);
    if (text.empty()) {
      REQUIRE_EQ(index.state_count(), 1U);
    } else {
      REQUIRE(index.state_count() <= 2U * text.size());
    }

    for (const auto& [pattern, count] : oracle) {
      REQUIRE(index.contains(pattern));
      REQUIRE_EQ(index.occurrence_count(pattern), count);
    }

    for (std::size_t query = 0; query < 40U; ++query) {
      const std::size_t query_length = query_length_distribution(random);
      std::string pattern;
      pattern.reserve(query_length);
      for (std::size_t offset = 0; offset < query_length; ++offset) {
        const unsigned int symbol = alphabet_distribution(random);
        const unsigned char byte = symbol < 4U
                                       ? static_cast<unsigned char>('a' + symbol)
                                       : static_cast<unsigned char>(0xf8U + (symbol - 4U));
        pattern.push_back(static_cast<char>(byte));
      }
      const std::size_t expected = naive_occurrence_count(text, pattern);
      REQUIRE_EQ(index.occurrence_count(pattern), expected);
      REQUIRE_EQ(index.contains(pattern), expected != 0U);
    }
  }
}
