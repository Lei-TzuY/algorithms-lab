#pragma once

#include "algorithms/number_theory/karatsuba.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace karatsuba_test_detail {

inline std::string canonical_decimal(std::string_view input) {
  std::size_t first_nonzero = 0U;
  while (first_nonzero < input.size() && input[first_nonzero] == '0') {
    ++first_nonzero;
  }
  if (first_nonzero == input.size()) {
    return "0";
  }
  return std::string(input.substr(first_nonzero));
}

inline std::string schoolbook_decimal(std::string_view first,
                                      std::string_view second) {
  const std::string left = canonical_decimal(first);
  const std::string right = canonical_decimal(second);
  if (left == "0" || right == "0") {
    return "0";
  }
  std::vector<unsigned> digits(left.size() + right.size(), 0U);
  for (std::size_t left_offset = 0; left_offset < left.size(); ++left_offset) {
    const unsigned lhs =
        static_cast<unsigned>(left[left.size() - 1U - left_offset] - '0');
    unsigned carry = 0U;
    for (std::size_t right_offset = 0; right_offset < right.size(); ++right_offset) {
      const unsigned rhs = static_cast<unsigned>(
          right[right.size() - 1U - right_offset] - '0');
      const std::size_t index = left_offset + right_offset;
      const unsigned current = digits[index] + lhs * rhs + carry;
      digits[index] = current % 10U;
      carry = current / 10U;
    }
    std::size_t index = left_offset + right.size();
    while (carry != 0U) {
      const unsigned current = digits[index] + carry;
      digits[index] = current % 10U;
      carry = current / 10U;
      ++index;
    }
  }
  while (digits.size() > 1U && digits.back() == 0U) {
    digits.pop_back();
  }
  std::string output;
  output.reserve(digits.size());
  for (auto iterator = digits.rbegin(); iterator != digits.rend(); ++iterator) {
    output.push_back(static_cast<char>('0' + *iterator));
  }
  return output;
}

inline std::string random_decimal(std::mt19937_64& generator,
                                  std::size_t digits) {
  std::string value(digits, '0');
  for (char& ch : value) {
    ch = static_cast<char>('0' + static_cast<char>(generator() % 10U));
  }
  return value;
}

}  // namespace karatsuba_test_detail

TEST_CASE(karatsuba_decimal_validation_and_canonical_zero) {
  using algorithms::number_theory::karatsuba_multiply_decimal;
  REQUIRE_THROWS_AS(karatsuba_multiply_decimal("", "1"), std::invalid_argument);
  REQUIRE_THROWS_AS(karatsuba_multiply_decimal("1", ""), std::invalid_argument);
  REQUIRE_THROWS_AS(karatsuba_multiply_decimal("12x", "3"), std::invalid_argument);
  REQUIRE_THROWS_AS(karatsuba_multiply_decimal("+12", "3"), std::invalid_argument);
  REQUIRE_EQ(karatsuba_multiply_decimal("0", "999").product, std::string("0"));
  REQUIRE_EQ(karatsuba_multiply_decimal("000000", "00042").product,
             std::string("0"));
  REQUIRE_EQ(karatsuba_multiply_decimal("000123", "000045").product,
             std::string("5535"));
}

TEST_CASE(karatsuba_known_products_and_commutativity) {
  using algorithms::number_theory::karatsuba_multiply_decimal;
  REQUIRE_EQ(karatsuba_multiply_decimal("123456789", "987654321").product,
             std::string("121932631112635269"));
  const std::string first =
      "3141592653589793238462643383279502884197169399375105820974944592";
  const std::string second =
      "2718281828459045235360287471352662497757247093699959574966967627";
  const auto forward = karatsuba_multiply_decimal(first, second);
  const auto reverse = karatsuba_multiply_decimal(second, first);
  REQUIRE_EQ(forward.product, reverse.product);
  REQUIRE_EQ(forward.product,
             karatsuba_test_detail::schoolbook_decimal(first, second));
}

TEST_CASE(karatsuba_large_inputs_exercise_recursive_path_deterministically) {
  using algorithms::number_theory::karatsuba_multiply_decimal;
  std::string first;
  std::string second;
  for (std::size_t index = 0; index < 240U; ++index) {
    first.push_back(static_cast<char>('1' + static_cast<char>(index % 9U)));
    second.push_back(static_cast<char>('9' - static_cast<char>(index % 9U)));
  }
  const auto result = karatsuba_multiply_decimal(first, second);
  const auto replay = karatsuba_multiply_decimal(first, second);
  REQUIRE_EQ(result.product, karatsuba_test_detail::schoolbook_decimal(first, second));
  REQUIRE(result.karatsuba_nodes > 0U);
  REQUIRE(result.recursive_calls > 1U);
  REQUIRE(result.schoolbook_limb_products > 0U);
  REQUIRE_EQ(result.product, replay.product);
  REQUIRE_EQ(result.recursive_calls, replay.recursive_calls);
  REQUIRE_EQ(result.karatsuba_nodes, replay.karatsuba_nodes);
  REQUIRE_EQ(result.schoolbook_limb_products, replay.schoolbook_limb_products);
}

TEST_CASE(karatsuba_randomized_differential_against_decimal_schoolbook) {
  using algorithms::number_theory::karatsuba_multiply_decimal;
  std::mt19937_64 generator(0x4B41524154535542ULL);
  for (std::size_t trial = 0; trial < 600U; ++trial) {
    const std::size_t first_digits = 1U + static_cast<std::size_t>(generator() % 320U);
    const std::size_t second_digits = 1U + static_cast<std::size_t>(generator() % 320U);
    std::string first = karatsuba_test_detail::random_decimal(generator, first_digits);
    std::string second = karatsuba_test_detail::random_decimal(generator, second_digits);
    if ((trial % 5U) == 0U) {
      first.insert(0U, trial % 7U, '0');
      second.insert(0U, (trial + 3U) % 7U, '0');
    }
    const auto result = karatsuba_multiply_decimal(first, second);
    const std::string expected = karatsuba_test_detail::schoolbook_decimal(first, second);
    REQUIRE_EQ(result.product, expected);
  }
}
