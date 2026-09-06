#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "algorithms/strings/kmp.hpp"

namespace {

using algorithms::strings::kmp_find_all;
using algorithms::strings::kmp_prefix_function;

void require_sequence(const std::vector<std::size_t>& actual,
                      const std::vector<std::size_t>& expected) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t index = 0; index < actual.size(); ++index) {
    REQUIRE_EQ(actual[index], expected[index]);
  }
}

std::vector<std::size_t> naive_find_all(std::string_view text,
                                        std::string_view pattern) {
  std::vector<std::size_t> matches;
  if (pattern.empty()) {
    for (std::size_t boundary = 0; boundary <= text.size(); ++boundary) {
      matches.push_back(boundary);
    }
    return matches;
  }
  if (pattern.size() > text.size()) {
    return matches;
  }

  const std::size_t final_start = text.size() - pattern.size();
  for (std::size_t start = 0; start <= final_start; ++start) {
    if (std::equal(pattern.begin(), pattern.end(), text.begin() + start)) {
      matches.push_back(start);
    }
  }
  return matches;
}

std::vector<std::size_t> naive_prefix_function(std::string_view pattern) {
  std::vector<std::size_t> prefix(pattern.size(), 0U);
  for (std::size_t end = 1; end < pattern.size(); ++end) {
    for (std::size_t length = end; length > 0U; --length) {
      const std::size_t suffix_begin = end + 1U - length;
      if (std::equal(pattern.begin(), pattern.begin() + length,
                     pattern.begin() + suffix_begin)) {
        prefix[end] = length;
        break;
      }
    }
  }
  return prefix;
}

std::string random_bytes(std::mt19937_64& rng, std::size_t max_length) {
  std::uniform_int_distribution<std::size_t> length_distribution(0U,
                                                                 max_length);
  std::uniform_int_distribution<int> byte_distribution(0, 7);
  const std::size_t length = length_distribution(rng);
  std::string value;
  value.reserve(length);
  for (std::size_t index = 0; index < length; ++index) {
    value.push_back(static_cast<char>(byte_distribution(rng)));
  }
  return value;
}

}  // namespace

TEST_CASE(kmp_prefix_function_tracks_borders) {
  require_sequence(kmp_prefix_function(""), {});
  require_sequence(kmp_prefix_function("a"), {0});
  require_sequence(kmp_prefix_function("ababaca"), {0, 0, 1, 2, 3, 0, 1});
  require_sequence(kmp_prefix_function("aaaaa"), {0, 1, 2, 3, 4});
}

TEST_CASE(kmp_find_all_handles_empty_and_overlapping_patterns) {
  require_sequence(kmp_find_all("", ""), {0});
  require_sequence(kmp_find_all("abc", ""), {0, 1, 2, 3});
  require_sequence(kmp_find_all("", "a"), {});
  require_sequence(kmp_find_all("abc", "abc"), {0});
  require_sequence(kmp_find_all("abc", "d"), {});
  require_sequence(kmp_find_all("aaaaa", "aaa"), {0, 1, 2});
  require_sequence(kmp_find_all("ababababa", "ababa"), {0, 2, 4});
}

TEST_CASE(kmp_treats_embedded_nulls_and_high_bit_bytes_as_data) {
  const std::string text{"a\0b\0b", 5};
  const std::string pattern{"\0b", 2};
  require_sequence(kmp_find_all(text, pattern), {1, 3});

  std::string high_text;
  high_text.push_back(static_cast<char>(0xFF));
  high_text.push_back(static_cast<char>(0x80));
  high_text.push_back(static_cast<char>(0xFF));
  high_text.push_back(static_cast<char>(0x80));
  std::string high_pattern;
  high_pattern.push_back(static_cast<char>(0xFF));
  high_pattern.push_back(static_cast<char>(0x80));
  require_sequence(kmp_find_all(high_text, high_pattern), {0, 2});
}

TEST_CASE(kmp_matches_independent_naive_oracles_randomized) {
  std::mt19937_64 rng(0x4b4d505f50484153ULL);

  for (std::size_t trial = 0; trial < 500; ++trial) {
    const std::string text = random_bytes(rng, 80U);
    const std::string pattern = random_bytes(rng, 18U);

    require_sequence(kmp_prefix_function(pattern),
                     naive_prefix_function(pattern));
    const std::vector<std::size_t> expected = naive_find_all(text, pattern);
    const std::vector<std::size_t> actual = kmp_find_all(text, pattern);
    require_sequence(actual, expected);

    for (std::size_t index = 0; index < actual.size(); ++index) {
      if (index > 0U) {
        REQUIRE(actual[index - 1U] < actual[index]);
      }
      REQUIRE(actual[index] <= text.size());
      if (!pattern.empty()) {
        REQUIRE(actual[index] <= text.size() - pattern.size());
        REQUIRE(std::equal(pattern.begin(), pattern.end(),
                           text.begin() + actual[index]));
      }
    }
  }
}
