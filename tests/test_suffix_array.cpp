#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "algorithms/strings/suffix_array.hpp"

namespace {

using algorithms::strings::SuffixArrayResult;
using algorithms::strings::build_suffix_array;

bool suffix_less(std::string_view input, std::size_t left,
                 std::size_t right) {
  while (left < input.size() && right < input.size()) {
    const auto left_byte = static_cast<unsigned char>(input[left]);
    const auto right_byte = static_cast<unsigned char>(input[right]);
    if (left_byte != right_byte) {
      return left_byte < right_byte;
    }
    ++left;
    ++right;
  }
  return left == input.size() && right != input.size();
}

std::size_t direct_lcp(std::string_view input, std::size_t left,
                       std::size_t right) {
  std::size_t length = 0U;
  while (length < input.size() - left &&
         length < input.size() - right &&
         input[left + length] == input[right + length]) {
    ++length;
  }
  return length;
}

SuffixArrayResult naive_suffix_array(std::string_view input) {
  SuffixArrayResult result;
  result.order.resize(input.size());
  result.rank.resize(input.size());
  result.lcp.assign(input.size(), 0U);
  std::iota(result.order.begin(), result.order.end(), std::size_t{0});

  std::sort(result.order.begin(), result.order.end(),
            [&](std::size_t left, std::size_t right) {
              return suffix_less(input, left, right);
            });

  for (std::size_t position = 0U; position < input.size(); ++position) {
    result.rank[result.order[position]] = position;
    if (position > 0U) {
      result.lcp[position] = direct_lcp(input, result.order[position - 1U],
                                        result.order[position]);
    }
  }
  return result;
}

void require_sequence(const std::vector<std::size_t>& actual,
                      const std::vector<std::size_t>& expected) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t index = 0U; index < actual.size(); ++index) {
    REQUIRE_EQ(actual[index], expected[index]);
  }
}

void require_result(const SuffixArrayResult& actual,
                    const SuffixArrayResult& expected) {
  require_sequence(actual.order, expected.order);
  require_sequence(actual.rank, expected.rank);
  require_sequence(actual.lcp, expected.lcp);
}

void require_invariants(std::string_view input,
                        const SuffixArrayResult& result) {
  REQUIRE_EQ(result.order.size(), input.size());
  REQUIRE_EQ(result.rank.size(), input.size());
  REQUIRE_EQ(result.lcp.size(), input.size());

  std::vector<bool> seen(input.size(), false);
  for (std::size_t position = 0U; position < input.size(); ++position) {
    const std::size_t start = result.order[position];
    REQUIRE(start < input.size());
    REQUIRE(!seen[start]);
    seen[start] = true;
    REQUIRE_EQ(result.rank[start], position);

    if (position == 0U) {
      REQUIRE_EQ(result.lcp[position], std::size_t{0});
    } else {
      const std::size_t previous = result.order[position - 1U];
      REQUIRE(suffix_less(input, previous, start));
      REQUIRE_EQ(result.lcp[position], direct_lcp(input, previous, start));
    }
  }
}

std::string random_bytes(std::mt19937_64& rng, std::size_t max_length) {
  std::uniform_int_distribution<std::size_t> length_distribution(0U,
                                                                 max_length);
  std::uniform_int_distribution<int> byte_distribution(0, 255);
  const std::size_t length = length_distribution(rng);
  std::string value;
  value.reserve(length);
  for (std::size_t index = 0U; index < length; ++index) {
    value.push_back(static_cast<char>(byte_distribution(rng)));
  }
  return value;
}

}  // namespace

TEST_CASE(suffix_array_handles_empty_singleton_and_known_strings) {
  require_result(build_suffix_array(""), SuffixArrayResult{});

  const SuffixArrayResult singleton = build_suffix_array("x");
  require_sequence(singleton.order, {0});
  require_sequence(singleton.rank, {0});
  require_sequence(singleton.lcp, {0});

  const SuffixArrayResult banana = build_suffix_array("banana");
  require_sequence(banana.order, {5, 3, 1, 0, 4, 2});
  require_sequence(banana.rank, {3, 2, 5, 1, 4, 0});
  require_sequence(banana.lcp, {0, 1, 3, 0, 0, 2});

  const SuffixArrayResult repeated = build_suffix_array("aaaa");
  require_sequence(repeated.order, {3, 2, 1, 0});
  require_sequence(repeated.lcp, {0, 1, 2, 3});
}

TEST_CASE(suffix_array_uses_unsigned_byte_lexicographic_order) {
  std::string input;
  input.push_back(static_cast<char>(0xFF));
  input.push_back('\0');
  input.push_back(static_cast<char>(0x80));

  const SuffixArrayResult result = build_suffix_array(input);
  require_sequence(result.order, {1, 2, 0});
  require_sequence(result.lcp, {0, 0, 0});

  const std::string with_null{"a\0a", 3};
  const SuffixArrayResult null_result = build_suffix_array(with_null);
  require_sequence(null_result.order, {1, 2, 0});
  require_sequence(null_result.lcp, {0, 0, 1});
}

TEST_CASE(suffix_array_matches_naive_suffix_and_lcp_oracles_randomized) {
  std::mt19937_64 rng(0x5355464649584152ULL);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::string input = random_bytes(rng, 60U);
    const SuffixArrayResult actual = build_suffix_array(input);
    const SuffixArrayResult expected = naive_suffix_array(input);
    require_result(actual, expected);
    require_invariants(input, actual);
  }
}
