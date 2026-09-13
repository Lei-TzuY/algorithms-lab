#pragma once

#include "algorithms/dynamic_programming/hirschberg_lcs.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

using algorithms::dynamic_programming::LcsMatch;
using algorithms::dynamic_programming::hirschberg_lcs;

[[nodiscard]] std::size_t hirschberg_oracle_length(std::string_view first,
                                                   std::string_view second) {
  std::vector<std::vector<std::size_t>> dp(
      first.size() + 1U, std::vector<std::size_t>(second.size() + 1U, 0U));
  for (std::size_t row = 0U; row < first.size(); ++row) {
    for (std::size_t column = 0U; column < second.size(); ++column) {
      if (first[row] == second[column]) {
        dp[row + 1U][column + 1U] = dp[row][column] + 1U;
      } else {
        dp[row + 1U][column + 1U] =
            std::max(dp[row][column + 1U], dp[row + 1U][column]);
      }
    }
  }
  return dp.back().back();
}

void hirschberg_replay_witness(std::string_view first, std::string_view second,
                               const std::vector<LcsMatch>& matches) {
  bool have_previous = false;
  std::size_t previous_first = 0U;
  std::size_t previous_second = 0U;
  for (const LcsMatch match : matches) {
    REQUIRE(match.first_index < first.size());
    REQUIRE(match.second_index < second.size());
    REQUIRE(first[match.first_index] == second[match.second_index]);
    if (have_previous) {
      REQUIRE(previous_first < match.first_index);
      REQUIRE(previous_second < match.second_index);
    }
    have_previous = true;
    previous_first = match.first_index;
    previous_second = match.second_index;
  }
}

[[nodiscard]] std::string hirschberg_bytes(
    const std::vector<unsigned int>& bytes) {
  std::string result;
  result.reserve(bytes.size());
  for (const unsigned int value : bytes) {
    REQUIRE(value <= 255U);
    result.push_back(static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

TEST_CASE(hirschberg_lcs_known_and_boundary_cases) {
  REQUIRE(hirschberg_lcs("", "abc").empty());
  REQUIRE(hirschberg_lcs("abc", "").empty());

  const auto classical = hirschberg_lcs("ABCBDAB", "BDCABA");
  hirschberg_replay_witness("ABCBDAB", "BDCABA", classical);
  REQUIRE_EQ(classical.size(), 4U);

  const auto identical = hirschberg_lcs("recovery", "recovery");
  hirschberg_replay_witness("recovery", "recovery", identical);
  REQUIRE_EQ(identical.size(), 8U);

  const auto disjoint = hirschberg_lcs("abc", "XYZ");
  REQUIRE(disjoint.empty());
}

TEST_CASE(hirschberg_lcs_arbitrary_bytes_and_swapped_dimensions) {
  const std::string first = hirschberg_bytes({0U, 128U, 255U, 1U, 128U});
  const std::string second = hirschberg_bytes({255U, 0U, 128U, 1U});
  const auto matches = hirschberg_lcs(first, second);
  hirschberg_replay_witness(first, second, matches);
  REQUIRE_EQ(matches.size(), hirschberg_oracle_length(first, second));

  const std::string long_first(257U, 'a');
  const std::string short_second = "baaa";
  const auto forward = hirschberg_lcs(long_first, short_second);
  hirschberg_replay_witness(long_first, short_second, forward);
  REQUIRE_EQ(forward.size(), 3U);

  const auto swapped = hirschberg_lcs(short_second, long_first);
  hirschberg_replay_witness(short_second, long_first, swapped);
  REQUIRE_EQ(swapped.size(), 3U);
}

TEST_CASE(hirschberg_lcs_exhaustive_binary_small_length_oracle) {
  std::size_t checked = 0U;
  for (std::size_t first_length = 0U; first_length <= 6U; ++first_length) {
    const std::size_t first_count = std::size_t{1} << first_length;
    for (std::size_t second_length = 0U; second_length <= 6U;
         ++second_length) {
      const std::size_t second_count = std::size_t{1} << second_length;
      for (std::size_t first_mask = 0U; first_mask < first_count; ++first_mask) {
        std::string first(first_length, 'a');
        for (std::size_t index = 0U; index < first_length; ++index) {
          if ((first_mask & (std::size_t{1} << index)) != 0U) first[index] = 'b';
        }
        for (std::size_t second_mask = 0U; second_mask < second_count;
             ++second_mask) {
          std::string second(second_length, 'a');
          for (std::size_t index = 0U; index < second_length; ++index) {
            if ((second_mask & (std::size_t{1} << index)) != 0U) {
              second[index] = 'b';
            }
          }
          const auto actual = hirschberg_lcs(first, second);
          hirschberg_replay_witness(first, second, actual);
          REQUIRE_EQ(actual.size(), hirschberg_oracle_length(first, second));
          ++checked;
        }
      }
    }
  }
  REQUIRE_EQ(checked, 16129U);
}

TEST_CASE(hirschberg_lcs_randomized_full_table_differential) {
  std::mt19937_64 rng(0x4849525343484245ULL);
  for (std::size_t trial = 0U; trial < 1400U; ++trial) {
    const std::size_t first_length = static_cast<std::size_t>(rng() % 29U);
    const std::size_t second_length = static_cast<std::size_t>(rng() % 29U);
    std::string first;
    std::string second;
    first.reserve(first_length);
    second.reserve(second_length);
    for (std::size_t index = 0U; index < first_length; ++index) {
      first.push_back(
          static_cast<char>(static_cast<unsigned char>(rng() & 0xFFU)));
    }
    for (std::size_t index = 0U; index < second_length; ++index) {
      second.push_back(
          static_cast<char>(static_cast<unsigned char>(rng() & 0xFFU)));
    }
    const auto actual = hirschberg_lcs(first, second);
    hirschberg_replay_witness(first, second, actual);
    REQUIRE_EQ(actual.size(), hirschberg_oracle_length(first, second));
  }
}

}  // namespace
