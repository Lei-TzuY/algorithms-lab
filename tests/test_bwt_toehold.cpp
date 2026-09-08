#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string make_bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    REQUIRE(value <= 255U);
    result.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

std::string random_bytes(std::mt19937_64& rng, std::size_t length,
                         unsigned int alphabet) {
  std::uniform_int_distribution<unsigned int> byte_dist(0U, alphabet - 1U);
  std::string result(length, '\0');
  for (std::size_t index = 0U; index < length; ++index) {
    result[index] =
        static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
  }
  return result;
}

std::vector<std::size_t> direct_locations(std::string_view text,
                                          std::string_view pattern) {
  if (pattern.empty()) {
    std::vector<std::size_t> positions(text.size() + 1U);
    for (std::size_t position = 0U; position <= text.size(); ++position) {
      positions[position] = position;
    }
    return positions;
  }
  if (pattern.size() > text.size()) {
    return {};
  }

  std::vector<std::size_t> positions;
  const std::size_t last = text.size() - pattern.size();
  for (std::size_t start = 0U; start <= last; ++start) {
    if (text.substr(start, pattern.size()) == pattern) {
      positions.push_back(start);
    }
  }
  return positions;
}

void require_toehold_matches_direct(
    const algorithms::strings::BwtByteIndex& index, std::string_view text,
    std::string_view pattern) {
  const std::vector<std::size_t> expected = direct_locations(text, pattern);
  const std::optional<std::size_t> first = index.locate_one_toehold(pattern);
  const std::optional<std::size_t> second = index.locate_one_toehold(pattern);
  REQUIRE_EQ(first, second);

  if (expected.empty()) {
    REQUIRE(!first.has_value());
    return;
  }
  REQUIRE(first.has_value());
  REQUIRE(std::binary_search(expected.begin(), expected.end(), *first));
}

void require_run_sample_bound(const algorithms::strings::BwtByteIndex& index) {
  REQUIRE(index.run_toehold_sample_count() <=
          std::size_t{2} * (index.bwt_run_count() + std::size_t{1}));
  REQUIRE_EQ(index.run_toehold_sample_payload_bytes(),
             index.run_toehold_sample_count() *
                 (std::size_t{2} * sizeof(std::size_t)));
}

}  // namespace

TEST_CASE(bwt_toehold_deterministic_witnesses_and_sentinel_split) {
  using algorithms::strings::BwtByteIndex;

  const BwtByteIndex empty{std::string_view{}};
  REQUIRE_EQ(empty.run_toehold_sample_count(), std::size_t{0});
  REQUIRE_EQ(empty.run_toehold_sample_payload_bytes(), std::size_t{0});
  REQUIRE_EQ(empty.locate_one_toehold(std::string_view{}),
             (std::optional<std::size_t>{0U}));
  REQUIRE(!empty.locate_one_toehold("x").has_value());

  const std::string banana = "banana";
  const BwtByteIndex banana_rate_one{banana, 1U};
  const BwtByteIndex banana_rate_large{banana, 64U};
  for (const std::string_view pattern :
       {std::string_view{"a"}, std::string_view{"ana"},
        std::string_view{"banana"}, std::string_view{"nana"},
        std::string_view{"x"}, std::string_view{}}) {
    require_toehold_matches_direct(banana_rate_one, banana, pattern);
    require_toehold_matches_direct(banana_rate_large, banana, pattern);
    REQUIRE_EQ(banana_rate_one.locate_one_toehold(pattern),
               banana_rate_large.locate_one_toehold(pattern));
  }
  require_run_sample_bound(banana_rate_one);
  require_run_sample_bound(banana_rate_large);

  // The full conceptual BWT for bytes {0,1,1} is {1,$,1,0}. Deleting the
  // sentinel makes the two 1-runs look adjacent, so Phase 22 must retain the
  // sentinel as an explicit run break when deciding suffix-position samples.
  const std::string sentinel_split = make_bytes({0U, 1U, 1U});
  const BwtByteIndex split_index{sentinel_split, 32U};
  REQUIRE_EQ(split_index.bwt_run_count(), std::size_t{2});
  REQUIRE_EQ(split_index.run_toehold_sample_count(), std::size_t{3});
  require_run_sample_bound(split_index);
  for (const std::string& pattern :
       {make_bytes({0U}), make_bytes({1U}), make_bytes({1U, 1U}),
        make_bytes({0U, 1U}), make_bytes({1U, 0U})}) {
    require_toehold_matches_direct(split_index, sentinel_split, pattern);
  }

  const std::string arbitrary =
      make_bytes({255U, 0U, 128U, 255U, 0U, 128U, 0U});
  const BwtByteIndex arbitrary_index{arbitrary, 5U};
  for (const std::string& pattern :
       {make_bytes({255U}), make_bytes({0U}), make_bytes({128U}),
        make_bytes({255U, 0U}), make_bytes({0U, 128U}),
        make_bytes({128U, 255U}), make_bytes({255U, 255U})}) {
    require_toehold_matches_direct(arbitrary_index, arbitrary, pattern);
  }
}

TEST_CASE(bwt_toehold_randomized_differential_against_direct_scan) {
  using algorithms::strings::BwtByteIndex;

  std::mt19937_64 rng(0x22A11B17ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 80U);
  std::uniform_int_distribution<std::size_t> rate_dist(1U, 64U);
  std::uniform_int_distribution<std::size_t> pattern_length_dist(0U, 16U);
  std::uniform_int_distribution<unsigned int> alphabet_choice(0U, 3U);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const unsigned int alphabet =
        alphabet_choice(rng) == 0U ? 256U : 1U + alphabet_choice(rng) * 2U;
    const std::string text =
        random_bytes(rng, text_length_dist(rng), alphabet);
    const BwtByteIndex index{text, rate_dist(rng)};
    require_run_sample_bound(index);

    for (std::size_t query = 0U; query < 80U; ++query) {
      std::string pattern;
      if (query % 3U == 0U) {
        std::uniform_int_distribution<std::size_t> start_dist(0U,
                                                               text.size());
        const std::size_t start = start_dist(rng);
        const std::size_t maximum =
            std::min<std::size_t>(16U, text.size() - start);
        std::uniform_int_distribution<std::size_t> length_dist(0U, maximum);
        pattern = text.substr(start, length_dist(rng));
      } else {
        pattern = random_bytes(rng, pattern_length_dist(rng), alphabet);
      }

      const std::vector<std::size_t> expected = direct_locations(text, pattern);
      const std::optional<std::size_t> got = index.locate_one_toehold(pattern);
      if (expected.empty()) {
        REQUIRE(!got.has_value());
      } else {
        REQUIRE(got.has_value());
        REQUIRE(std::binary_search(expected.begin(), expected.end(), *got));
      }
      if (query % 19U == 0U) {
        REQUIRE_EQ(index.locate(pattern), expected);
      }
    }
  }
}
