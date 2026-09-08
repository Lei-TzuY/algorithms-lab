#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <random>
#include <stdexcept>
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

std::string random_bytes(std::mt19937_64& rng, std::size_t length) {
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);
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
    std::iota(positions.begin(), positions.end(), std::size_t{0});
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

std::size_t expected_sample_count(std::size_t text_size, std::size_t rate) {
  if (text_size == 0U) {
    return 1U;
  }
  return 1U + text_size / rate + (text_size % rate != 0U ? 1U : 0U);
}

}  // namespace

TEST_CASE(bwt_index_empty_pattern_and_classic_overlaps) {
  const algorithms::strings::BwtByteIndex empty_index{std::string_view{}};
  REQUIRE_EQ(empty_index.text_size(), std::size_t{0});
  REQUIRE_EQ(empty_index.row_count(), std::size_t{1});
  REQUIRE_EQ(empty_index.count(std::string_view{}), std::size_t{1});
  REQUIRE_EQ(empty_index.locate(std::string_view{}),
             std::vector<std::size_t>{0U});
  REQUIRE_EQ(empty_index.count("x"), std::size_t{0});
  REQUIRE(empty_index.locate("x").empty());

  const std::string text = "banana";
  const algorithms::strings::BwtByteIndex index{text};
  REQUIRE_EQ(index.text_size(), text.size());
  REQUIRE_EQ(index.row_count(), text.size() + 1U);
  REQUIRE_EQ(index.count("ana"), std::size_t{2});
  REQUIRE_EQ(index.locate("ana"), (std::vector<std::size_t>{1U, 3U}));
  REQUIRE_EQ(index.locate("na"), (std::vector<std::size_t>{2U, 4U}));
  REQUIRE_EQ(index.locate("banana"), (std::vector<std::size_t>{0U}));
  REQUIRE(index.locate("bananas").empty());
  REQUIRE_EQ(index.locate(std::string_view{}),
             (std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U, 5U, 6U}));

  const algorithms::strings::BwtByteIndex repeated{"aaaaa"};
  REQUIRE_EQ(repeated.locate("aa"),
             (std::vector<std::size_t>{0U, 1U, 2U, 3U}));
  REQUIRE_EQ(repeated.locate("aaa"),
             (std::vector<std::size_t>{0U, 1U, 2U}));
}

TEST_CASE(bwt_index_preserves_all_byte_values_and_conceptual_sentinel) {
  const std::string text = make_bytes({0U, 255U, 0U, 128U, 255U, 0U});
  const algorithms::strings::BwtByteIndex index{text};

  const std::vector<std::string> patterns = {
      make_bytes({0U}), make_bytes({255U}), make_bytes({0U, 255U}),
      make_bytes({255U, 0U}), make_bytes({128U, 255U}),
      make_bytes({0U, 128U, 255U}), make_bytes({255U, 255U})};
  for (const std::string& pattern : patterns) {
    const std::vector<std::size_t> expected = direct_locations(text, pattern);
    REQUIRE_EQ(index.count(pattern), expected.size());
    REQUIRE_EQ(index.locate(pattern), expected);
  }

  std::string all_bytes;
  all_bytes.reserve(256U);
  for (unsigned int value = 0U; value <= 255U; ++value) {
    all_bytes.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  const algorithms::strings::BwtByteIndex all_index{all_bytes};
  for (const unsigned int value : {0U, 1U, 127U, 128U, 254U, 255U}) {
    const std::string pattern = make_bytes({value});
    REQUIRE_EQ(all_index.locate(pattern),
               (std::vector<std::size_t>{static_cast<std::size_t>(value)}));
  }
}

TEST_CASE(bwt_index_randomized_differential_against_direct_scan) {
  std::mt19937_64 rng(0xB17B17F19ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 96U);
  std::uniform_int_distribution<std::size_t> random_pattern_length_dist(0U,
                                                                        20U);

  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::string text = random_bytes(rng, text_length_dist(rng));
    const algorithms::strings::BwtByteIndex index{text};
    REQUIRE_EQ(index.text_size(), text.size());
    REQUIRE_EQ(index.row_count(), text.size() + 1U);

    for (std::size_t query = 0U; query < 80U; ++query) {
      std::string pattern;
      if (query % 3U == 0U) {
        std::uniform_int_distribution<std::size_t> start_dist(0U,
                                                               text.size());
        const std::size_t start = start_dist(rng);
        const std::size_t maximum_length =
            std::min<std::size_t>(20U, text.size() - start);
        std::uniform_int_distribution<std::size_t> length_dist(
            0U, maximum_length);
        pattern = text.substr(start, length_dist(rng));
      } else {
        pattern = random_bytes(rng, random_pattern_length_dist(rng));
      }

      const std::vector<std::size_t> expected = direct_locations(text, pattern);
      REQUIRE_EQ(index.count(pattern), expected.size());
      REQUIRE_EQ(index.locate(pattern), expected);
      if (query % 17U == 0U) {
        REQUIRE_EQ(index.locate(pattern), expected);
      }
    }
  }
}

TEST_CASE(bwt_index_sampled_locate_rates_and_storage_tradeoff) {
  const std::string text = "banana";
  REQUIRE_THROWS_AS(algorithms::strings::BwtByteIndex(text, 0U),
                    std::invalid_argument);

  for (const std::size_t rate : {1U, 2U, 3U, 4U, 7U, 32U, 100U}) {
    const algorithms::strings::BwtByteIndex index{text, rate};
    REQUIRE_EQ(index.locate_sample_rate(), rate);
    REQUIRE_EQ(index.max_lf_steps_per_locate(), rate - 1U);
    REQUIRE_EQ(index.sampled_row_count(),
               expected_sample_count(text.size(), rate));
    REQUIRE_EQ(index.sampled_position_payload_bytes(),
               index.sampled_row_count() * sizeof(std::size_t));
    REQUIRE_EQ(index.sampled_locate_payload_bytes(),
               index.sampled_membership_payload_bytes() +
                   index.sampled_position_payload_bytes());
    REQUIRE_EQ(index.locate(std::string_view{}),
               direct_locations(text, std::string_view{}));
    REQUIRE_EQ(index.locate("ana"), direct_locations(text, "ana"));
  }

  std::string storage_text(2048U, '\0');
  for (std::size_t index = 0U; index < storage_text.size(); ++index) {
    storage_text[index] = static_cast<char>(static_cast<unsigned char>(
        (index * 73U) & std::size_t{255}));
  }
  const algorithms::strings::BwtByteIndex sampled{storage_text, 32U};
  REQUIRE_EQ(sampled.sampled_row_count(), std::size_t{65});
  const std::size_t full_row_position_bytes =
      (storage_text.size() + 1U) * sizeof(std::size_t);
  REQUIRE(sampled.sampled_locate_payload_bytes() < full_row_position_bytes);
  REQUIRE_EQ(sampled.locate(std::string_view{}),
             direct_locations(storage_text, std::string_view{}));
}

TEST_CASE(bwt_index_randomized_sample_rates_match_direct_scan) {
  std::mt19937_64 rng(0x520B17A20ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 96U);
  std::uniform_int_distribution<std::size_t> rate_dist(1U, 64U);
  std::uniform_int_distribution<std::size_t> random_pattern_length_dist(0U,
                                                                        20U);

  for (std::size_t trial = 0U; trial < 250U; ++trial) {
    const std::string text = random_bytes(rng, text_length_dist(rng));
    const std::size_t rate = rate_dist(rng);
    const algorithms::strings::BwtByteIndex index{text, rate};
    REQUIRE_EQ(index.sampled_row_count(),
               expected_sample_count(text.size(), rate));
    REQUIRE_EQ(index.locate(std::string_view{}),
               direct_locations(text, std::string_view{}));

    for (std::size_t query = 0U; query < 60U; ++query) {
      std::string pattern;
      if (query % 3U == 0U) {
        std::uniform_int_distribution<std::size_t> start_dist(0U,
                                                               text.size());
        const std::size_t start = start_dist(rng);
        const std::size_t maximum_length =
            std::min<std::size_t>(20U, text.size() - start);
        std::uniform_int_distribution<std::size_t> length_dist(
            0U, maximum_length);
        pattern = text.substr(start, length_dist(rng));
      } else {
        pattern = random_bytes(rng, random_pattern_length_dist(rng));
      }

      const std::vector<std::size_t> expected = direct_locations(text, pattern);
      REQUIRE_EQ(index.count(pattern), expected.size());
      REQUIRE_EQ(index.locate(pattern), expected);
    }
  }
}
