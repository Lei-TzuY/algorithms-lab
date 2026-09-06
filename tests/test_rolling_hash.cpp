#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>

#include "algorithms/strings/rolling_hash.hpp"

namespace {

using algorithms::strings::RollingFingerprint;
using algorithms::strings::RollingHash;

RollingFingerprint direct_fingerprint(std::string_view input,
                                      std::size_t begin, std::size_t end) {
  RollingFingerprint result{};
  for (std::size_t index = begin; index < end; ++index) {
    const std::uint64_t symbol =
        static_cast<std::uint64_t>(static_cast<unsigned char>(input[index])) +
        std::uint64_t{1};
    result.first =
        (result.first * RollingHash::kBase + symbol) % RollingHash::kModulus1;
    result.second =
        (result.second * RollingHash::kBase + symbol) % RollingHash::kModulus2;
  }
  return result;
}

void require_fingerprint(const RollingFingerprint& actual,
                         const RollingFingerprint& expected) {
  REQUIRE_EQ(actual.first, expected.first);
  REQUIRE_EQ(actual.second, expected.second);
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

TEST_CASE(rolling_hash_validates_ranges_and_empty_snapshot) {
  const RollingHash empty("");
  REQUIRE_EQ(empty.size(), std::size_t{0});
  require_fingerprint(empty.fingerprint(0U, 0U), RollingFingerprint{});
  REQUIRE_THROWS_AS(empty.fingerprint(0U, 1U), std::out_of_range);

  const RollingHash hash("abc");
  REQUIRE_THROWS_AS(hash.fingerprint(2U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(hash.fingerprint(0U, 4U), std::out_of_range);
}

TEST_CASE(rolling_hash_is_position_independent_and_snapshots_input) {
  std::string input = "abcXabc";
  const RollingHash hash(input);
  const RollingFingerprint first = hash.fingerprint(0U, 3U);
  const RollingFingerprint second = hash.fingerprint(4U, 7U);
  require_fingerprint(first, second);
  require_fingerprint(first, direct_fingerprint(input, 0U, 3U));

  const RollingFingerprint whole_before = hash.fingerprint(0U, input.size());
  input[0] = 'z';
  input[3] = 'y';
  require_fingerprint(hash.fingerprint(0U, hash.size()), whole_before);
}

TEST_CASE(rolling_hash_treats_all_byte_values_as_data) {
  std::string input;
  input.push_back('\0');
  input.push_back(static_cast<char>(0x80));
  input.push_back(static_cast<char>(0xFF));
  input.push_back('\0');

  const RollingHash hash(input);
  for (std::size_t begin = 0U; begin <= input.size(); ++begin) {
    for (std::size_t end = begin; end <= input.size(); ++end) {
      require_fingerprint(hash.fingerprint(begin, end),
                          direct_fingerprint(input, begin, end));
    }
  }
}

TEST_CASE(rolling_hash_matches_direct_polynomial_oracle_randomized) {
  std::mt19937_64 rng(0x524f4c4c494e4755ULL);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::string input = random_bytes(rng, 120U);
    const RollingHash hash(input);
    REQUIRE_EQ(hash.size(), input.size());

    std::uniform_int_distribution<std::size_t> boundary_distribution(
        0U, input.size());
    for (std::size_t query = 0U; query < 80U; ++query) {
      std::size_t begin = boundary_distribution(rng);
      std::size_t end = boundary_distribution(rng);
      if (begin > end) {
        std::swap(begin, end);
      }
      require_fingerprint(hash.fingerprint(begin, end),
                          direct_fingerprint(input, begin, end));
    }
  }
}
