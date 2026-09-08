#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Interval {
  std::size_t begin;
  std::size_t end;
};

std::string make_bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    REQUIRE(value <= 255U);
    result.push_back(static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

std::uint8_t as_byte(char value) {
  return static_cast<std::uint8_t>(static_cast<unsigned char>(value));
}

bool suffix_less(std::string_view text, std::size_t lhs, std::size_t rhs) {
  while (lhs < text.size() && rhs < text.size()) {
    const auto left = static_cast<unsigned char>(text[lhs]);
    const auto right = static_cast<unsigned char>(text[rhs]);
    if (left != right) {
      return left < right;
    }
    ++lhs;
    ++rhs;
  }
  return lhs == text.size() && rhs != text.size();
}

std::vector<std::size_t> naive_suffix_rows(std::string_view text) {
  std::vector<std::size_t> rows(text.size() + 1U);
  std::iota(rows.begin(), rows.end(), std::size_t{0});
  std::sort(rows.begin(), rows.end(), [&](std::size_t lhs, std::size_t rhs) {
    return suffix_less(text, lhs, rhs);
  });
  return rows;
}

int compare_suffix_pattern(std::string_view text, std::size_t suffix,
                           std::string_view pattern) {
  std::size_t index = 0U;
  while (index < pattern.size() && suffix + index < text.size()) {
    const auto left = static_cast<unsigned char>(text[suffix + index]);
    const auto right = static_cast<unsigned char>(pattern[index]);
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
    ++index;
  }
  return index == pattern.size() ? 0 : -1;
}

Interval naive_interval(std::string_view text,
                        const std::vector<std::size_t>& rows,
                        std::string_view pattern) {
  std::size_t begin = 0U;
  while (begin < rows.size() &&
         compare_suffix_pattern(text, rows[begin], pattern) < 0) {
    ++begin;
  }
  std::size_t end = begin;
  while (end < rows.size() &&
         compare_suffix_pattern(text, rows[end], pattern) == 0) {
    ++end;
  }
  return Interval{begin, end};
}

std::size_t direct_count(std::string_view text, std::string_view pattern) {
  if (pattern.empty()) {
    return text.size() + 1U;
  }
  if (pattern.size() > text.size()) {
    return 0U;
  }
  std::size_t count = 0U;
  const std::size_t last = text.size() - pattern.size();
  for (std::size_t start = 0U; start <= last; ++start) {
    if (text.substr(start, pattern.size()) == pattern) {
      ++count;
    }
  }
  return count;
}

void verify_state(const algorithms::strings::BidirectionalBwtState& state,
                  std::string_view text, std::string_view pattern,
                  const std::vector<std::size_t>& forward_rows,
                  std::string_view reverse_text,
                  const std::vector<std::size_t>& reverse_rows) {
  const std::string reverse_pattern{pattern.rbegin(), pattern.rend()};
  const Interval forward = naive_interval(text, forward_rows, pattern);
  const Interval reverse =
      naive_interval(reverse_text, reverse_rows, reverse_pattern);

  REQUIRE_EQ(state.forward_begin(), forward.begin);
  REQUIRE_EQ(state.forward_end(), forward.end);
  REQUIRE_EQ(state.reverse_begin(), reverse.begin);
  REQUIRE_EQ(state.reverse_end(), reverse.end);
  REQUIRE_EQ(state.match_count(), forward.end - forward.begin);
  REQUIRE_EQ(state.match_count(), reverse.end - reverse.begin);
  REQUIRE_EQ(state.match_count(), direct_count(text, pattern));
}

}  // namespace

TEST_CASE(bwt_bidirectional_empty_and_mixed_extensions) {
  const algorithms::strings::BidirectionalBwtByteIndex empty_index{
      std::string_view{}};
  auto empty_state = empty_index.empty_state();
  REQUIRE_EQ(empty_state.match_count(), std::size_t{1});
  REQUIRE_EQ(empty_state.forward_begin(), std::size_t{0});
  REQUIRE_EQ(empty_state.forward_end(), std::size_t{1});
  REQUIRE_EQ(empty_state.reverse_begin(), std::size_t{0});
  REQUIRE_EQ(empty_state.reverse_end(), std::size_t{1});
  empty_state = empty_index.extend_right(empty_state, std::uint8_t{0});
  REQUIRE_EQ(empty_state.match_count(), std::size_t{0});
  REQUIRE_EQ(empty_state.forward_begin(), std::size_t{1});
  REQUIRE_EQ(empty_state.forward_end(), std::size_t{1});
  REQUIRE_EQ(empty_state.reverse_begin(), std::size_t{1});
  REQUIRE_EQ(empty_state.reverse_end(), std::size_t{1});

  const std::string text = "banana";
  const std::string reverse_text{text.rbegin(), text.rend()};
  const std::vector<std::size_t> forward_rows = naive_suffix_rows(text);
  const std::vector<std::size_t> reverse_rows = naive_suffix_rows(reverse_text);
  const algorithms::strings::BidirectionalBwtByteIndex index{text};
  auto state = index.empty_state();
  std::string pattern;
  verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);

  for (const char value : std::string{"ana"}) {
    state = index.extend_right(state, as_byte(value));
    pattern.push_back(value);
    verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);
  }
  state = index.extend_left(state, as_byte('b'));
  pattern.insert(pattern.begin(), 'b');
  verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);

  state = index.extend_right(state, as_byte('n'));
  pattern.push_back('n');
  verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);

  state = index.extend_right(state, as_byte('x'));
  pattern.push_back('x');
  verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);

  state = index.extend_left(state, std::uint8_t{0});
  pattern.insert(pattern.begin(), '\0');
  verify_state(state, text, pattern, forward_rows, reverse_text, reverse_rows);
}

TEST_CASE(bwt_bidirectional_full_byte_and_repeated_boundaries) {
  const std::string bytes = make_bytes({0U, 255U, 0U, 128U, 255U, 0U});
  const std::string reverse_bytes{bytes.rbegin(), bytes.rend()};
  const auto forward_rows = naive_suffix_rows(bytes);
  const auto reverse_rows = naive_suffix_rows(reverse_bytes);
  const algorithms::strings::BidirectionalBwtByteIndex byte_index{bytes};

  for (const unsigned int raw : {0U, 128U, 255U}) {
    const std::uint8_t value = static_cast<std::uint8_t>(raw);
    const char character = static_cast<char>(static_cast<unsigned char>(raw));

    auto left = byte_index.extend_left(byte_index.empty_state(), value);
    std::string left_pattern(1U, character);
    verify_state(left, bytes, left_pattern, forward_rows, reverse_bytes,
                 reverse_rows);

    auto right = byte_index.extend_right(byte_index.empty_state(), value);
    std::string right_pattern(1U, character);
    verify_state(right, bytes, right_pattern, forward_rows, reverse_bytes,
                 reverse_rows);
  }

  const std::string repeated = "aaaaa";
  const std::string repeated_reverse{repeated.rbegin(), repeated.rend()};
  const auto repeated_rows = naive_suffix_rows(repeated);
  const auto repeated_reverse_rows = naive_suffix_rows(repeated_reverse);
  const algorithms::strings::BidirectionalBwtByteIndex repeated_index{repeated};
  auto state = repeated_index.empty_state();
  std::string pattern;
  for (std::size_t index = 0U; index < 4U; ++index) {
    if (index % 2U == 0U) {
      state = repeated_index.extend_left(state, as_byte('a'));
      pattern.insert(pattern.begin(), 'a');
    } else {
      state = repeated_index.extend_right(state, as_byte('a'));
      pattern.push_back('a');
    }
    verify_state(state, repeated, pattern, repeated_rows, repeated_reverse,
                 repeated_reverse_rows);
  }
}

TEST_CASE(bwt_bidirectional_every_single_byte_matches_exact_interval) {
  std::string text;
  text.reserve(256U);
  for (unsigned int value = 0U; value <= 255U; ++value) {
    text.push_back(static_cast<char>(static_cast<unsigned char>(value)));
  }
  const std::string reverse_text{text.rbegin(), text.rend()};
  const auto forward_rows = naive_suffix_rows(text);
  const auto reverse_rows = naive_suffix_rows(reverse_text);
  const algorithms::strings::BidirectionalBwtByteIndex index{text};

  for (unsigned int raw = 0U; raw <= 255U; ++raw) {
    const std::uint8_t value = static_cast<std::uint8_t>(raw);
    const std::string pattern(
        1U, static_cast<char>(static_cast<unsigned char>(raw)));
    const auto left = index.extend_left(index.empty_state(), value);
    const auto right = index.extend_right(index.empty_state(), value);
    verify_state(left, text, pattern, forward_rows, reverse_text, reverse_rows);
    verify_state(right, text, pattern, forward_rows, reverse_text, reverse_rows);
  }
}

TEST_CASE(bwt_bidirectional_randomized_mixed_extension_differential) {
  std::mt19937_64 rng(0xB1D1B17A25ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 40U);
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);

  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    std::string text(text_length_dist(rng), '\0');
    for (char& value : text) {
      value = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
    }

    const std::string reverse_text{text.rbegin(), text.rend()};
    const auto forward_rows = naive_suffix_rows(text);
    const auto reverse_rows = naive_suffix_rows(reverse_text);
    const algorithms::strings::BidirectionalBwtByteIndex index{text};

    for (std::size_t query = 0U; query < 24U; ++query) {
      auto state = index.empty_state();
      std::string pattern;
      verify_state(state, text, pattern, forward_rows, reverse_text,
                   reverse_rows);

      for (std::size_t step = 0U; step < 8U; ++step) {
        std::uint8_t value = static_cast<std::uint8_t>(byte_dist(rng));
        if (!text.empty() && step % 3U == 0U) {
          const std::size_t position =
              static_cast<std::size_t>(rng() % text.size());
          value = as_byte(text[position]);
        }

        const char character =
            static_cast<char>(static_cast<unsigned char>(value));
        if ((rng() & 1ULL) == 0ULL) {
          state = index.extend_left(state, value);
          pattern.insert(pattern.begin(), character);
        } else {
          state = index.extend_right(state, value);
          pattern.push_back(character);
        }
        verify_state(state, text, pattern, forward_rows, reverse_text,
                     reverse_rows);
      }
    }
  }
}
