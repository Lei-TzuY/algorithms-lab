#pragma once

#include "algorithms/strings/incremental_aho_dictionary.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace incremental_aho_dictionary_test_detail {

using algorithms::strings::IncrementalAhoCorasickByteDictionary;
using algorithms::strings::IncrementalAhoCorasickMatch;

inline bool match_less(const IncrementalAhoCorasickMatch& first,
                       const IncrementalAhoCorasickMatch& second) {
  if (first.end != second.end) {
    return first.end < second.end;
  }
  if (first.begin != second.begin) {
    return first.begin < second.begin;
  }
  return first.pattern_id < second.pattern_id;
}

inline std::vector<IncrementalAhoCorasickMatch> naive_matches(
    const std::vector<std::string>& patterns, const std::string_view text) {
  std::vector<IncrementalAhoCorasickMatch> matches;
  for (std::size_t end = 0U; end <= text.size(); ++end) {
    for (std::size_t pattern_id = 0U; pattern_id < patterns.size();
         ++pattern_id) {
      const std::string& pattern = patterns[pattern_id];
      if (pattern.size() > end) {
        continue;
      }
      const std::size_t begin = end - pattern.size();
      if (text.substr(begin, pattern.size()) == pattern) {
        matches.push_back({pattern_id, begin, end});
      }
    }
  }
  std::sort(matches.begin(), matches.end(), match_less);
  return matches;
}

inline char byte_char(const unsigned value) {
  return static_cast<char>(static_cast<unsigned char>(value));
}

inline void require_matches(
    const IncrementalAhoCorasickByteDictionary& dictionary,
    const std::vector<std::string>& patterns, const std::string_view text) {
  REQUIRE(dictionary.valid_invariants());
  REQUIRE_EQ(dictionary.pattern_count(), patterns.size());
  REQUIRE_EQ(dictionary.active_bucket_count(),
             static_cast<std::size_t>(std::popcount(patterns.size())));
  REQUIRE(dictionary.find_all(text) == naive_matches(patterns, text));
}

}  // namespace incremental_aho_dictionary_test_detail

TEST_CASE(incremental_aho_dictionary_empty_and_binary_carry_schedule) {
  using namespace incremental_aho_dictionary_test_detail;

  IncrementalAhoCorasickByteDictionary dictionary;
  std::vector<std::string> patterns;

  require_matches(dictionary, patterns, "");
  require_matches(dictionary, patterns, "anything");

  for (std::size_t index = 0U; index < 128U; ++index) {
    std::string pattern;
    pattern.push_back(byte_char(static_cast<unsigned>(index)));
    pattern.push_back(byte_char(static_cast<unsigned>(index * 17U)));
    REQUIRE_EQ(dictionary.add_pattern(pattern), index);
    patterns.push_back(std::move(pattern));

    REQUIRE_EQ(dictionary.pattern_count(), index + 1U);
    REQUIRE_EQ(dictionary.active_bucket_count(),
               static_cast<std::size_t>(std::popcount(index + 1U)));
    REQUIRE(dictionary.valid_invariants());
  }

  std::string text;
  for (unsigned value = 0U; value < 128U; ++value) {
    text.push_back(byte_char(value));
    text.push_back(byte_char(value * 17U));
  }
  require_matches(dictionary, patterns, text);
}

TEST_CASE(incremental_aho_dictionary_preserves_duplicates_empty_patterns_and_order) {
  using namespace incremental_aho_dictionary_test_detail;

  IncrementalAhoCorasickByteDictionary dictionary;
  std::vector<std::string> patterns;

  const std::vector<std::string> inserted = {
      "he", "she", "his", "hers", "", "he", "ers", ""};
  for (const std::string& pattern : inserted) {
    const std::size_t expected_id = patterns.size();
    REQUIRE_EQ(dictionary.add_pattern(pattern), expected_id);
    patterns.push_back(pattern);
    require_matches(dictionary, patterns, "ushers");
  }

  const auto matches = dictionary.find_all("ushers");
  REQUIRE(matches == naive_matches(patterns, "ushers"));
  REQUIRE(dictionary.valid_invariants());
}

TEST_CASE(incremental_aho_dictionary_supports_arbitrary_bytes) {
  using namespace incremental_aho_dictionary_test_detail;

  IncrementalAhoCorasickByteDictionary dictionary;
  std::vector<std::string> patterns;

  std::string first;
  first.push_back(byte_char(0x00U));
  first.push_back(byte_char(0x80U));
  std::string second;
  second.push_back(byte_char(0xffU));
  second.push_back(byte_char(0x00U));
  std::string third;
  third.push_back(byte_char(0x80U));

  for (const std::string& pattern :
       std::vector<std::string>{first, second, third, first, ""}) {
    REQUIRE_EQ(dictionary.add_pattern(pattern), patterns.size());
    patterns.push_back(pattern);
  }

  std::string text;
  text.push_back(byte_char(0x00U));
  text.push_back(byte_char(0x80U));
  text.push_back(byte_char(0xffU));
  text.push_back(byte_char(0x00U));
  text.push_back(byte_char(0x80U));

  require_matches(dictionary, patterns, text);
}

TEST_CASE(incremental_aho_dictionary_randomized_online_trace_matches_naive_oracle) {
  using namespace incremental_aho_dictionary_test_detail;

  std::mt19937_64 random(0xA40D1C7ULL);
  for (std::size_t trial = 0U; trial < 50U; ++trial) {
    IncrementalAhoCorasickByteDictionary dictionary;
    std::vector<std::string> patterns;

    for (std::size_t step = 0U; step < 90U; ++step) {
      const bool insert =
          patterns.size() < 48U &&
          (patterns.empty() || (random() % 5U) < 2U);
      if (insert) {
        const std::size_t length =
            static_cast<std::size_t>(random() % 7U);
        std::string pattern;
        pattern.reserve(length);
        for (std::size_t index = 0U; index < length; ++index) {
          const unsigned selector = static_cast<unsigned>(random() % 10U);
          const unsigned value =
              selector == 0U ? 0x00U
              : selector == 1U ? 0x80U
              : selector == 2U ? 0xffU
                                : static_cast<unsigned>(random() % 8U);
          pattern.push_back(byte_char(value));
        }

        const std::size_t expected_id = patterns.size();
        REQUIRE_EQ(dictionary.add_pattern(pattern), expected_id);
        patterns.push_back(std::move(pattern));
        REQUIRE(dictionary.valid_invariants());
        REQUIRE_EQ(dictionary.active_bucket_count(),
                   static_cast<std::size_t>(
                       std::popcount(dictionary.pattern_count())));
        continue;
      }

      const std::size_t text_length =
          static_cast<std::size_t>(random() % 41U);
      std::string text;
      text.reserve(text_length);
      for (std::size_t index = 0U; index < text_length; ++index) {
        const unsigned selector = static_cast<unsigned>(random() % 10U);
        const unsigned value =
            selector == 0U ? 0x00U
            : selector == 1U ? 0x80U
            : selector == 2U ? 0xffU
                              : static_cast<unsigned>(random() % 8U);
        text.push_back(byte_char(value));
      }
      require_matches(dictionary, patterns, text);
    }

    require_matches(dictionary, patterns, "");
  }
}
