#include "algorithms/strings/aho_corasick.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {
using algorithms::strings::AhoCorasickByteMatcher;
using algorithms::strings::AhoCorasickMatch;

std::vector<AhoCorasickMatch> naive_matches(
    const std::vector<std::string>& patterns, std::string_view text) {
  std::vector<AhoCorasickMatch> result;
  for (std::size_t end = 0; end <= text.size(); ++end) {
    std::vector<std::size_t> ids;
    for (std::size_t id = 0; id < patterns.size(); ++id) {
      const std::size_t length = patterns[id].size();
      if (length <= end &&
          text.substr(end - length, length) == std::string_view(patterns[id])) {
        ids.push_back(id);
      }
    }
    std::sort(ids.begin(), ids.end(), [&](const std::size_t lhs,
                                         const std::size_t rhs) {
      if (patterns[lhs].size() != patterns[rhs].size()) {
        return patterns[lhs].size() > patterns[rhs].size();
      }
      return lhs < rhs;
    });
    for (const std::size_t id : ids) {
      result.push_back(AhoCorasickMatch{id, end - patterns[id].size(), end});
    }
  }
  return result;
}

TEST_CASE(classic_failure_links_and_suffix_outputs) {
  const std::vector<std::string> patterns{"he", "she", "his", "hers"};
  const AhoCorasickByteMatcher matcher(patterns);
  REQUIRE_EQ(matcher.pattern_count(), std::size_t{4});
  REQUIRE(matcher.state_count() > matcher.pattern_count());
  REQUIRE_EQ(matcher.find_all("ushers"), naive_matches(patterns, "ushers"));
}

TEST_CASE(overlap_duplicates_empty_patterns_and_bytes) {
  const std::vector<std::string> patterns{
      "", "a", "aa", "a", std::string("\0\xff", 2), ""};
  std::string text("aaa\0\xff", 5);
  const AhoCorasickByteMatcher matcher(patterns);
  REQUIRE_EQ(matcher.find_all(text), naive_matches(patterns, text));
  REQUIRE_EQ(AhoCorasickByteMatcher({}).find_all(text),
             std::vector<AhoCorasickMatch>{});
}

TEST_CASE(pattern_order_and_repeated_queries_are_deterministic) {
  const std::vector<std::string> patterns{"ba", "a", "ba", "", "aba"};
  const AhoCorasickByteMatcher matcher(patterns);
  const auto first = matcher.find_all("ababa");
  const auto second = matcher.find_all("ababa");
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first, naive_matches(patterns, "ababa"));
}

TEST_CASE(randomized_differential_against_naive_multi_pattern_search) {
  std::mt19937_64 rng(0xAC0AC0ULL);
  std::uniform_int_distribution<int> pattern_count_dist(0, 9);
  std::uniform_int_distribution<int> pattern_length_dist(0, 7);
  std::uniform_int_distribution<int> text_length_dist(0, 45);
  std::uniform_int_distribution<int> byte_choice_dist(0, 8);

  const std::array<unsigned char, 9> alphabet{
      0x00U, 0x01U, 0x02U, 0x61U, 0x62U, 0x63U, 0x7fU, 0x80U, 0xffU};
  auto random_string = [&](const int length) {
    std::string value;
    value.reserve(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
      const auto symbol = alphabet[static_cast<std::size_t>(byte_choice_dist(rng))];
      value.push_back(static_cast<char>(symbol));
    }
    return value;
  };

  for (int trial = 0; trial < 700; ++trial) {
    const int pattern_count = pattern_count_dist(rng);
    std::vector<std::string> patterns;
    patterns.reserve(static_cast<std::size_t>(pattern_count));
    for (int index = 0; index < pattern_count; ++index) {
      patterns.push_back(random_string(pattern_length_dist(rng)));
    }
    const std::string text = random_string(text_length_dist(rng));
    const AhoCorasickByteMatcher matcher(patterns);
    const auto actual = matcher.find_all(text);
    const auto expected = naive_matches(patterns, text);
    REQUIRE_EQ(actual, expected);
    for (const auto& match : actual) {
      REQUIRE(match.pattern_index < patterns.size());
      REQUIRE(match.begin <= match.end);
      REQUIRE(match.end <= text.size());
      REQUIRE_EQ(text.substr(match.begin, match.end - match.begin),
                 std::string_view(patterns[match.pattern_index]));
    }
  }
}
}  // namespace
