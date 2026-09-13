#pragma once

#include "algorithms/strings/manacher.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

using algorithms::strings::ManacherPalindromeRadii;

ManacherPalindromeRadii manacher_naive_palindrome_radii(std::string_view bytes) {
  ManacherPalindromeRadii result;
  const std::size_t n = bytes.size();
  result.odd_radius.assign(n, 0U);
  result.even_radius.assign(n, 0U);

  auto consider = [&result](std::size_t start, std::size_t length) {
    if (length > result.longest_length ||
        (length == result.longest_length && start < result.longest_start)) {
      result.longest_start = start;
      result.longest_length = length;
    }
  };

  for (std::size_t center = 0U; center < n; ++center) {
    std::size_t radius = 1U;
    while (radius <= center && radius < n - center &&
           bytes[center - radius] == bytes[center + radius]) {
      ++radius;
    }
    result.odd_radius[center] = radius;
    consider(center - (radius - 1U), 2U * radius - 1U);
  }

  for (std::size_t center = 0U; center < n; ++center) {
    std::size_t radius = 0U;
    while (radius < center && radius < n - center &&
           bytes[center - radius - 1U] == bytes[center + radius]) {
      ++radius;
    }
    result.even_radius[center] = radius;
    if (radius != 0U) {
      consider(center - radius, 2U * radius);
    }
  }

  return result;
}

bool manacher_is_palindrome(std::string_view bytes, std::size_t start,
                            std::size_t length) {
  for (std::size_t offset = 0U; offset < length / 2U; ++offset) {
    if (bytes[start + offset] != bytes[start + length - 1U - offset]) {
      return false;
    }
  }
  return true;
}

void require_manacher_maximal_radii(
    std::string_view bytes, const ManacherPalindromeRadii& result) {
  REQUIRE_EQ(result.odd_radius.size(), bytes.size());
  REQUIRE_EQ(result.even_radius.size(), bytes.size());

  for (std::size_t center = 0U; center < bytes.size(); ++center) {
    const std::size_t odd = result.odd_radius[center];
    REQUIRE(odd >= 1U);
    const std::size_t odd_start = center - (odd - 1U);
    const std::size_t odd_length = 2U * odd - 1U;
    REQUIRE(manacher_is_palindrome(bytes, odd_start, odd_length));
    REQUIRE(odd_start == 0U || odd_start + odd_length == bytes.size() ||
            bytes[odd_start - 1U] != bytes[odd_start + odd_length]);

    const std::size_t even = result.even_radius[center];
    const std::size_t even_start = center - even;
    const std::size_t even_length = 2U * even;
    REQUIRE(manacher_is_palindrome(bytes, even_start, even_length));
    REQUIRE(even_start == 0U || even_start + even_length == bytes.size() ||
            bytes[even_start - 1U] != bytes[even_start + even_length]);
  }

  REQUIRE(result.longest_start + result.longest_length <= bytes.size());
  REQUIRE(manacher_is_palindrome(bytes, result.longest_start,
                                result.longest_length));
}

TEST_CASE(manacher_known_odd_even_and_tie_cases) {
  using algorithms::strings::manacher_palindrome_radii;

  const auto empty = manacher_palindrome_radii("");
  REQUIRE(empty.odd_radius.empty());
  REQUIRE(empty.even_radius.empty());
  REQUIRE_EQ(empty.longest_start, std::size_t{0});
  REQUIRE_EQ(empty.longest_length, std::size_t{0});

  const auto single = manacher_palindrome_radii("a");
  REQUIRE_EQ(single.odd_radius, std::vector<std::size_t>({1}));
  REQUIRE_EQ(single.even_radius, std::vector<std::size_t>({0}));
  REQUIRE_EQ(single.longest_start, std::size_t{0});
  REQUIRE_EQ(single.longest_length, std::size_t{1});

  const auto odd = manacher_palindrome_radii("abacaba");
  REQUIRE_EQ(odd.odd_radius,
             std::vector<std::size_t>({1, 2, 1, 4, 1, 2, 1}));
  REQUIRE_EQ(odd.even_radius,
             std::vector<std::size_t>({0, 0, 0, 0, 0, 0, 0}));
  REQUIRE_EQ(odd.longest_start, std::size_t{0});
  REQUIRE_EQ(odd.longest_length, std::size_t{7});

  const auto even = manacher_palindrome_radii("abba");
  REQUIRE_EQ(even.even_radius,
             std::vector<std::size_t>({0, 0, 2, 0}));
  REQUIRE_EQ(even.longest_start, std::size_t{0});
  REQUIRE_EQ(even.longest_length, std::size_t{4});

  const auto tied = manacher_palindrome_radii("abacdfgdcaba");
  REQUIRE_EQ(tied.longest_start, std::size_t{0});
  REQUIRE_EQ(tied.longest_length, std::size_t{3});
}

TEST_CASE(manacher_supports_arbitrary_bytes) {
  using algorithms::strings::manacher_palindrome_radii;
  const std::string bytes({static_cast<char>(0x00), static_cast<char>(0xFF),
                           static_cast<char>(0x00), static_cast<char>(0xFF),
                           static_cast<char>(0x00)});
  const auto result = manacher_palindrome_radii(bytes);
  REQUIRE_EQ(result.longest_start, std::size_t{0});
  REQUIRE_EQ(result.longest_length, std::size_t{5});
  REQUIRE_EQ(result.odd_radius[2], std::size_t{3});
  require_manacher_maximal_radii(bytes, result);
}

TEST_CASE(manacher_repeated_and_boundary_shapes) {
  using algorithms::strings::manacher_palindrome_radii;
  for (const std::string& input : {std::string("aaaaaa"), std::string("abcdef"),
                                   std::string("aabbaa"), std::string("racecarx")}) {
    const auto actual = manacher_palindrome_radii(input);
    REQUIRE_EQ(actual, manacher_naive_palindrome_radii(input));
    require_manacher_maximal_radii(input, actual);
  }
}

TEST_CASE(manacher_randomized_differential_against_naive_expansion) {
  using algorithms::strings::manacher_palindrome_radii;
  std::mt19937_64 rng(0x4D414E4143484552ULL);

  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 161U);
    std::string input(length, '\0');
    for (char& byte : input) {
      byte = static_cast<char>(static_cast<unsigned char>(rng() % 9U));
    }

    const auto actual = manacher_palindrome_radii(input);
    const auto expected = manacher_naive_palindrome_radii(input);
    REQUIRE_EQ(actual, expected);
    require_manacher_maximal_radii(input, actual);
  }
}

}  // namespace
