#include "algorithms/strings/suffix_tree.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
using algorithms::strings::SuffixTreeByteIndex;

std::vector<std::size_t> naive_locate(std::string_view text,
                                      std::string_view pattern) {
  std::vector<std::size_t> out;
  if (pattern.empty()) {
    for (std::size_t i = 0; i <= text.size(); ++i) out.push_back(i);
    return out;
  }
  if (pattern.size() > text.size()) return out;
  for (std::size_t i = 0; i + pattern.size() <= text.size(); ++i) {
    if (text.substr(i, pattern.size()) == pattern) out.push_back(i);
  }
  return out;
}

std::size_t naive_distinct(std::string_view text) {
  std::set<std::string> values;
  for (std::size_t begin = 0; begin < text.size(); ++begin) {
    for (std::size_t length = 1; begin + length <= text.size(); ++length) {
      values.emplace(text.substr(begin, length));
    }
  }
  return values.size();
}

TEST_CASE(suffix_tree_deterministic_boundaries_and_repetition) {
  SuffixTreeByteIndex empty("");
  REQUIRE(empty.valid_structure());
  REQUIRE(empty.contains(""));
  REQUIRE_EQ(empty.occurrence_count(""), 1U);
  REQUIRE_EQ(empty.locate(""), (std::vector<std::size_t>{0U}));
  REQUIRE(!empty.contains("a"));
  REQUIRE_EQ(empty.distinct_substring_count(), 0U);

  SuffixTreeByteIndex banana("banana");
  REQUIRE(banana.valid_structure());
  REQUIRE(banana.contains("ana"));
  REQUIRE_EQ(banana.locate("ana"), (std::vector<std::size_t>{1U, 3U}));
  REQUIRE_EQ(banana.occurrence_count("na"), 2U);
  REQUIRE_EQ(banana.distinct_substring_count(), 15U);
  REQUIRE(!banana.contains("ananab"));

  SuffixTreeByteIndex repeated("aaaa");
  REQUIRE(repeated.valid_structure());
  REQUIRE_EQ(repeated.occurrence_count("a"), 4U);
  REQUIRE_EQ(repeated.occurrence_count("aa"), 3U);
  REQUIRE_EQ(repeated.occurrence_count("aaaa"), 1U);
  REQUIRE_EQ(repeated.distinct_substring_count(), 4U);
}

TEST_CASE(suffix_tree_arbitrary_byte_semantics) {
  std::string bytes;
  bytes.push_back('\0');
  bytes.push_back(static_cast<char>(0xff));
  bytes.push_back('\0');
  SuffixTreeByteIndex index(bytes);
  REQUIRE(index.valid_structure());
  const std::string nul(1, '\0');
  REQUIRE_EQ(index.locate(nul), (std::vector<std::size_t>{0U, 2U}));
  std::string pair;
  pair.push_back(static_cast<char>(0xff));
  pair.push_back('\0');
  REQUIRE_EQ(index.locate(pair), (std::vector<std::size_t>{1U}));
}

TEST_CASE(suffix_tree_randomized_differential_against_naive_substrings) {
  std::mt19937_64 rng(0x5AFF17EEULL);
  constexpr std::array<unsigned char, 5> alphabet{0x00U, 0x61U, 0x62U,
                                                  0x63U, 0xffU};
  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 25U);
    std::string text(length, '\0');
    for (char& ch : text) {
      ch = static_cast<char>(
          alphabet[static_cast<std::size_t>(rng() % alphabet.size())]);
    }
    SuffixTreeByteIndex index(text);
    REQUIRE(index.valid_structure());
    REQUIRE_EQ(index.distinct_substring_count(), naive_distinct(text));
    for (int query = 0; query < 30; ++query) {
      const std::size_t pattern_length = static_cast<std::size_t>(rng() % 8U);
      std::string pattern(pattern_length, '\0');
      for (char& ch : pattern) {
        ch = static_cast<char>(
            alphabet[static_cast<std::size_t>(rng() % alphabet.size())]);
      }
      const auto expected = naive_locate(text, pattern);
      REQUIRE(index.contains(pattern) == !expected.empty());
      REQUIRE_EQ(index.occurrence_count(pattern), expected.size());
      REQUIRE_EQ(index.locate(pattern), expected);
    }
  }
}

TEST_CASE(suffix_tree_stress_adversarial_shapes) {
  for (std::size_t length = 1; length <= 96; ++length) {
    const std::string same(length, 'x');
    SuffixTreeByteIndex repeated(same);
    REQUIRE(repeated.valid_structure());

    std::string diverse;
    diverse.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
      diverse.push_back(
          static_cast<char>(static_cast<unsigned char>(i % 251U)));
    }
    SuffixTreeByteIndex distinctish(diverse);
    REQUIRE(distinctish.valid_structure());
  }

  std::mt19937_64 rng(0x5154E55ULL);
  for (int trial = 0; trial < 120; ++trial) {
    const std::size_t length = 64U + static_cast<std::size_t>(rng() % 193U);
    std::string text(length, '\0');
    for (char& ch : text) ch = static_cast<char>(rng() & 0xffU);
    SuffixTreeByteIndex index(text);
    REQUIRE(index.valid_structure());
    for (int query = 0; query < 40; ++query) {
      const std::size_t begin = static_cast<std::size_t>(rng() % text.size());
      const std::size_t max_length = text.size() - begin;
      const std::size_t pattern_length =
          1U + static_cast<std::size_t>(rng() % max_length);
      const std::string_view pattern(text.data() + begin, pattern_length);
      REQUIRE_EQ(index.locate(pattern), naive_locate(text, pattern));
    }
  }
}

}  // namespace
