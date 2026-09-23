#pragma once

#include "algorithms/randomized/knuth_yao_sampler.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::randomized::DyadicKnuthYaoSampler;
using algorithms::randomized::KnuthYaoSample;

namespace knuth_yao_sampler_test_detail {

inline KnuthYaoSample sample_word(
    const DyadicKnuthYaoSampler& sampler,
    const std::uint64_t word,
    const std::size_t width) {
  std::size_t position = 0U;
  return sampler.sample([&]() {
    if (position >= width) {
      throw std::logic_error(
          "sampler requested more bits than dyadic precision");
    }
    const std::size_t shift = width - position - 1U;
    ++position;
    return ((word >> shift) & UINT64_C(1)) != 0U;
  });
}

inline void require_exact_full_word_distribution(
    const std::vector<std::uint64_t>& weights) {
  const DyadicKnuthYaoSampler sampler(weights);
  REQUIRE(sampler.valid_structure());

  const std::size_t width = sampler.max_bits_per_sample();
  const std::uint64_t total = sampler.total_weight();
  std::vector<std::uint64_t> counts(weights.size(), 0U);

  for (std::uint64_t word = 0U; word < total; ++word) {
    const KnuthYaoSample sample =
        sample_word(sampler, word, width);
    REQUIRE(sample.symbol < weights.size());
    REQUIRE(sample.bits_consumed <= width);
    ++counts[sample.symbol];
  }

  REQUIRE(counts == weights);
}

}  // namespace knuth_yao_sampler_test_detail

TEST_CASE(knuth_yao_rejects_invalid_weight_domains) {
  REQUIRE_THROWS_AS(
      DyadicKnuthYaoSampler({}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      DyadicKnuthYaoSampler({0U, 0U, 0U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      DyadicKnuthYaoSampler({1U, 2U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      DyadicKnuthYaoSampler({
          std::numeric_limits<std::uint64_t>::max(), 1U}),
      std::overflow_error);
}

TEST_CASE(knuth_yao_unit_mass_consumes_zero_bits) {
  const DyadicKnuthYaoSampler sampler({0U, 8U, 0U});
  std::size_t calls = 0U;
  const KnuthYaoSample result =
      sampler.sample([&]() {
        ++calls;
        return false;
      });

  REQUIRE_EQ(result.symbol, 1U);
  REQUIRE_EQ(result.bits_consumed, 0U);
  REQUIRE_EQ(calls, 0U);
  REQUIRE_EQ(sampler.total_weight(), 8U);
  REQUIRE_EQ(sampler.max_bits_per_sample(), 0U);
  REQUIRE(sampler.valid_structure());
}

TEST_CASE(knuth_yao_early_leaf_uses_only_needed_prefix) {
  const DyadicKnuthYaoSampler sampler({3U, 1U});

  std::vector<bool> bits{false, true};
  std::size_t position = 0U;
  const KnuthYaoSample first =
      sampler.sample([&]() {
        return bits[position++];
      });

  REQUIRE_EQ(first.symbol, 0U);
  REQUIRE_EQ(first.bits_consumed, 1U);
  REQUIRE_EQ(position, 1U);

  position = 0U;
  bits = {true, true};
  const KnuthYaoSample second =
      sampler.sample([&]() {
        return bits[position++];
      });
  REQUIRE_EQ(second.symbol, 1U);
  REQUIRE_EQ(second.bits_consumed, 2U);
  REQUIRE_EQ(position, 2U);
}

TEST_CASE(knuth_yao_exact_known_dyadic_distributions) {
  using namespace knuth_yao_sampler_test_detail;

  require_exact_full_word_distribution({1U, 1U});
  require_exact_full_word_distribution({1U, 3U});
  require_exact_full_word_distribution({2U, 6U});
  require_exact_full_word_distribution({1U, 1U, 2U});
  require_exact_full_word_distribution({0U, 1U, 0U, 3U});
  require_exact_full_word_distribution({2U, 0U, 5U, 1U});
  require_exact_full_word_distribution({8U, 4U, 2U, 1U, 1U});
}

TEST_CASE(knuth_yao_zero_weight_symbols_are_unreachable) {
  using namespace knuth_yao_sampler_test_detail;

  const std::vector<std::uint64_t> weights{
      0U, 0U, 3U, 0U, 5U, 0U};
  const DyadicKnuthYaoSampler sampler(weights);

  for (std::uint64_t word = 0U;
       word < sampler.total_weight(); ++word) {
    const auto result =
        sample_word(sampler, word, sampler.max_bits_per_sample());
    REQUIRE(weights[result.symbol] != 0U);
  }
  require_exact_full_word_distribution(weights);
}

TEST_CASE(knuth_yao_random_small_distributions_match_weights_exactly) {
  using namespace knuth_yao_sampler_test_detail;

  std::mt19937_64 random(0x4B4E55544859414FULL);

  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const std::size_t symbols =
        1U + static_cast<std::size_t>(random() % 7U);
    const std::size_t precision =
        static_cast<std::size_t>(random() % 6U);
    const std::uint64_t total =
        UINT64_C(1) << precision;

    std::vector<std::uint64_t> weights(symbols, 0U);
    for (std::uint64_t unit = 0U; unit < total; ++unit) {
      const std::size_t symbol =
          static_cast<std::size_t>(random() % symbols);
      ++weights[symbol];
    }

    require_exact_full_word_distribution(weights);
  }
}
