#include "test_framework.hpp"

#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "algorithms/strings/z_function.hpp"

namespace {

using algorithms::strings::z_function;

void require_sequence(const std::vector<std::size_t>& actual,
                      const std::vector<std::size_t>& expected) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t index = 0U; index < actual.size(); ++index) {
    REQUIRE_EQ(actual[index], expected[index]);
  }
}

std::vector<std::size_t> naive_z_function(std::string_view input) {
  std::vector<std::size_t> z(input.size(), 0U);
  if (input.empty()) {
    return z;
  }

  z[0] = input.size();
  for (std::size_t index = 1U; index < input.size(); ++index) {
    while (z[index] < input.size() - index &&
           input[z[index]] == input[index + z[index]]) {
      ++z[index];
    }
  }
  return z;
}

std::string random_bytes(std::mt19937_64& rng, std::size_t max_length) {
  std::uniform_int_distribution<std::size_t> length_distribution(0U,
                                                                 max_length);
  std::uniform_int_distribution<int> byte_distribution(0, 7);
  const std::size_t length = length_distribution(rng);
  std::string value;
  value.reserve(length);
  for (std::size_t index = 0U; index < length; ++index) {
    value.push_back(static_cast<char>(byte_distribution(rng)));
  }
  return value;
}

void require_z_invariants(std::string_view input,
                          const std::vector<std::size_t>& z) {
  REQUIRE_EQ(z.size(), input.size());
  if (input.empty()) {
    return;
  }

  REQUIRE_EQ(z[0], input.size());
  for (std::size_t index = 1U; index < input.size(); ++index) {
    REQUIRE(z[index] <= input.size() - index);
    for (std::size_t offset = 0U; offset < z[index]; ++offset) {
      REQUIRE(input[offset] == input[index + offset]);
    }
    if (z[index] < input.size() - index) {
      REQUIRE(input[z[index]] != input[index + z[index]]);
    }
  }
}

}  // namespace

TEST_CASE(z_function_handles_empty_singleton_and_simple_shapes) {
  require_sequence(z_function(""), {});
  require_sequence(z_function("a"), {1});
  require_sequence(z_function("aaaaa"), {5, 4, 3, 2, 1});
  require_sequence(z_function("abcde"), {5, 0, 0, 0, 0});
  require_sequence(z_function("abacaba"), {7, 0, 1, 0, 3, 0, 1});
  require_sequence(z_function("abababab"), {8, 0, 6, 0, 4, 0, 2, 0});
}

TEST_CASE(z_function_treats_embedded_nulls_and_high_bit_bytes_as_data) {
  const std::string with_nulls{"a\0a\0a", 5};
  require_sequence(z_function(with_nulls), {5, 0, 3, 0, 1});

  std::string high_bytes;
  high_bytes.push_back(static_cast<char>(0xFF));
  high_bytes.push_back(static_cast<char>(0x80));
  high_bytes.push_back(static_cast<char>(0xFF));
  high_bytes.push_back(static_cast<char>(0x80));
  require_sequence(z_function(high_bytes), {4, 0, 2, 0});
}

TEST_CASE(z_function_matches_independent_naive_lcp_oracle_randomized) {
  std::mt19937_64 rng(0x5a5f46554e435449ULL);

  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::string input = random_bytes(rng, 100U);
    const std::vector<std::size_t> actual = z_function(input);
    const std::vector<std::size_t> expected = naive_z_function(input);
    require_sequence(actual, expected);
    require_z_invariants(input, actual);
  }
}
