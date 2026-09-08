#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string make_run_locate_bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  result.reserve(values.size());
  for (const unsigned int value : values) {
    REQUIRE(value <= 255U);
    result.push_back(
        static_cast<char>(static_cast<unsigned char>(value)));
  }
  return result;
}

std::string random_run_locate_bytes(std::mt19937_64& rng,
                                    std::size_t length,
                                    unsigned int alphabet) {
  std::uniform_int_distribution<unsigned int> byte_dist(0U, alphabet - 1U);
  std::string result(length, '\0');
  for (std::size_t index = 0U; index < length; ++index) {
    result[index] =
        static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
  }
  return result;
}

std::vector<std::size_t> direct_run_locations(std::string_view text,
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

void require_run_sampled_locations(
    const algorithms::strings::BwtByteIndex& index, std::string_view text,
    std::string_view pattern) {
  const std::vector<std::size_t> expected =
      direct_run_locations(text, pattern);
  REQUIRE_EQ(index.locate_run_sampled(pattern), expected);
}

}  // namespace

TEST_CASE(bwt_run_sampled_locate_deterministic_and_periodic_rate_independent) {
  using algorithms::strings::BwtByteIndex;

  const BwtByteIndex empty{std::string_view{}, 17U};
  REQUIRE_EQ(empty.locate_run_sampled(std::string_view{}),
             (std::vector<std::size_t>{0U}));
  REQUIRE(empty.locate_run_sampled("x").empty());

  const std::string banana = "banana";
  const BwtByteIndex dense{banana, 1U};
  const BwtByteIndex sparse{banana, 1024U};
  REQUIRE(dense.sampled_row_count() > sparse.sampled_row_count());
  for (const std::string_view pattern :
       {std::string_view{}, std::string_view{"a"}, std::string_view{"ana"},
        std::string_view{"banana"}, std::string_view{"nana"},
        std::string_view{"x"}}) {
    const std::vector<std::size_t> expected =
        direct_run_locations(banana, pattern);
    REQUIRE_EQ(dense.locate_run_sampled(pattern), expected);
    REQUIRE_EQ(sparse.locate_run_sampled(pattern), expected);
    REQUIRE_EQ(sparse.locate_run_sampled(pattern), sparse.locate(pattern));
  }

  const std::string repetitive(96U, 'a');
  const BwtByteIndex repetitive_index{repetitive, 4096U};
  for (const std::string_view pattern :
       {std::string_view{}, std::string_view{"a"}, std::string_view{"aaaa"},
        std::string_view{"aaaaaaaaaaaaaaaa"}, std::string_view{"b"}}) {
    require_run_sampled_locations(repetitive_index, repetitive, pattern);
  }

  // Preserve the Phase-22 conceptual-sentinel run split adversary while now
  // requiring the complete occurrence set rather than one toehold.
  const std::string sentinel_split = make_run_locate_bytes({0U, 1U, 1U});
  const BwtByteIndex split_index{sentinel_split, 128U};
  for (const std::string& pattern :
       {make_run_locate_bytes({0U}), make_run_locate_bytes({1U}),
        make_run_locate_bytes({1U, 1U}), make_run_locate_bytes({0U, 1U}),
        make_run_locate_bytes({1U, 0U})}) {
    require_run_sampled_locations(split_index, sentinel_split, pattern);
  }

  const std::string arbitrary =
      make_run_locate_bytes({255U, 0U, 128U, 255U, 0U, 128U, 0U});
  const BwtByteIndex arbitrary_index{arbitrary, 257U};
  for (const std::string& pattern :
       {make_run_locate_bytes({255U}), make_run_locate_bytes({0U}),
        make_run_locate_bytes({128U}), make_run_locate_bytes({255U, 0U}),
        make_run_locate_bytes({0U, 128U}),
        make_run_locate_bytes({128U, 255U}),
        make_run_locate_bytes({255U, 255U})}) {
    require_run_sampled_locations(arbitrary_index, arbitrary, pattern);
  }
}

TEST_CASE(bwt_run_sampled_locate_randomized_differential_against_direct_scan) {
  using algorithms::strings::BwtByteIndex;

  std::mt19937_64 rng(0x23B0175AULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 80U);
  std::uniform_int_distribution<std::size_t> pattern_length_dist(0U, 16U);
  std::uniform_int_distribution<unsigned int> alphabet_choice(0U, 3U);

  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const unsigned int choice = alphabet_choice(rng);
    const unsigned int alphabet = choice == 0U ? 256U : 1U << choice;
    const std::string text =
        random_run_locate_bytes(rng, text_length_dist(rng), alphabet);

    const BwtByteIndex dense{text, 1U};
    const BwtByteIndex sparse{text, text.size() + 257U};
    if (!text.empty()) {
      REQUIRE(dense.sampled_row_count() >= sparse.sampled_row_count());
    }

    for (std::size_t query = 0U; query < 60U; ++query) {
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
        pattern = random_run_locate_bytes(
            rng, pattern_length_dist(rng), alphabet);
      }

      const std::vector<std::size_t> expected =
          direct_run_locations(text, pattern);
      REQUIRE_EQ(dense.locate_run_sampled(pattern), expected);
      REQUIRE_EQ(sparse.locate_run_sampled(pattern), expected);
      if (query % 17U == 0U) {
        REQUIRE_EQ(sparse.locate_run_sampled(pattern), sparse.locate(pattern));
      }
    }
  }
}
