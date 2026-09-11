#include "algorithms/coding/huffman_byte_code.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {
using algorithms::coding::HuffmanByteCodebook;

std::uint64_t checked_oracle_cost(const std::vector<std::uint64_t>& weights,
                                  const std::vector<std::size_t>& depths) {
  std::uint64_t cost = 0U;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    REQUIRE(depths[i] == 0U || weights[i] <= std::numeric_limits<std::uint64_t>::max() / depths[i]);
    const std::uint64_t contribution = weights[i] * static_cast<std::uint64_t>(depths[i]);
    REQUIRE(contribution <= std::numeric_limits<std::uint64_t>::max() - cost);
    cost += contribution;
  }
  return cost;
}

std::vector<std::vector<std::size_t>> full_tree_depth_multisets(std::size_t leaves) {
  if (leaves == 1U) return {{0U}};
  std::vector<std::vector<std::size_t>> all;
  for (std::size_t left_count = 1U; left_count < leaves; ++left_count) {
    const auto lefts = full_tree_depth_multisets(left_count);
    const auto rights = full_tree_depth_multisets(leaves - left_count);
    for (const auto& left : lefts) {
      for (const auto& right : rights) {
        std::vector<std::size_t> depths;
        depths.reserve(leaves);
        for (const auto depth : left) depths.push_back(depth + 1U);
        for (const auto depth : right) depths.push_back(depth + 1U);
        std::sort(depths.begin(), depths.end());
        all.push_back(std::move(depths));
      }
    }
  }
  std::sort(all.begin(), all.end());
  all.erase(std::unique(all.begin(), all.end()), all.end());
  return all;
}

std::uint64_t exhaustive_optimal_cost(std::vector<std::uint64_t> weights) {
  if (weights.empty()) return 0U;
  if (weights.size() == 1U) return weights.front();
  const auto depth_sets = full_tree_depth_multisets(weights.size());
  std::sort(weights.begin(), weights.end());
  std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
  do {
    for (const auto& depths : depth_sets) {
      best = std::min(best, checked_oracle_cost(weights, depths));
    }
  } while (std::next_permutation(weights.begin(), weights.end()));
  return best;
}

std::uint64_t replay_weighted_cost(const HuffmanByteCodebook& codebook) {
  std::uint64_t cost = 0U;
  for (std::size_t symbol = 0; symbol < 256U; ++symbol) {
    const auto code = codebook.code(static_cast<std::uint8_t>(symbol));
    if (!code.has_value()) continue;
    const std::uint64_t frequency = codebook.frequencies()[symbol];
    REQUIRE(frequency <= std::numeric_limits<std::uint64_t>::max() / code->size());
    cost += frequency * static_cast<std::uint64_t>(code->size());
  }
  return cost;
}
}

TEST_CASE(huffman_empty_singleton_and_invalid_streams) {
  std::array<std::uint64_t,256> empty{};
  HuffmanByteCodebook no_symbols(empty);
  REQUIRE_EQ(no_symbols.symbol_count(), 0U);
  REQUIRE_EQ(no_symbols.weighted_bit_count(), 0U);
  REQUIRE(no_symbols.valid_codebook());
  REQUIRE(no_symbols.encode_bits(std::span<const std::uint8_t>{}).empty());
  REQUIRE(no_symbols.decode_bits("").empty());
  REQUIRE_THROWS_AS(no_symbols.decode_bits("0"), std::invalid_argument);

  std::array<std::uint64_t,256> one{};
  one[0xffU] = 7U;
  HuffmanByteCodebook singleton(one);
  REQUIRE_EQ(singleton.symbol_count(), 1U);
  REQUIRE_EQ(singleton.weighted_bit_count(), 7U);
  REQUIRE(singleton.valid_codebook());
  REQUIRE_EQ(singleton.code(0xffU).value(), std::string_view("0"));
  const std::vector<std::uint8_t> payload{0xffU,0xffU,0xffU};
  REQUIRE_EQ(singleton.encode_bits(payload), std::string("000"));
  REQUIRE_EQ(singleton.decode_bits("000"), payload);
  REQUIRE_THROWS_AS(singleton.decode_bits("1"), std::invalid_argument);
  REQUIRE_THROWS_AS(singleton.decode_bits("x"), std::invalid_argument);
  const std::vector<std::uint8_t> absent{0x01U};
  REQUIRE_THROWS_AS(singleton.encode_bits(absent), std::invalid_argument);
}

TEST_CASE(huffman_textbook_bytes_and_deterministic_round_trip) {
  std::array<std::uint64_t,256> frequencies{};
  frequencies[static_cast<std::uint8_t>('A')] = 45U;
  frequencies[static_cast<std::uint8_t>('B')] = 13U;
  frequencies[static_cast<std::uint8_t>('C')] = 12U;
  frequencies[static_cast<std::uint8_t>('D')] = 16U;
  frequencies[static_cast<std::uint8_t>('E')] = 9U;
  frequencies[static_cast<std::uint8_t>('F')] = 5U;
  HuffmanByteCodebook first(frequencies);
  HuffmanByteCodebook second(frequencies);
  REQUIRE_EQ(first.weighted_bit_count(), 224U);
  REQUIRE(first.valid_codebook());
  REQUIRE(second.valid_codebook());
  for (std::size_t symbol=0; symbol<256U; ++symbol) REQUIRE_EQ(first.code(static_cast<std::uint8_t>(symbol)), second.code(static_cast<std::uint8_t>(symbol)));
  const std::vector<std::uint8_t> payload{'F','A','C','E','D','B','A','D'};
  const std::string bits=first.encode_bits(payload);
  REQUIRE_EQ(first.decode_bits(bits), payload);
  REQUIRE_EQ(replay_weighted_cost(first), first.weighted_bit_count());

  const auto small_code = first.code(static_cast<std::uint8_t>('F'));
  REQUIRE(small_code.has_value());
  REQUIRE(small_code->size() > 1U);
  REQUIRE_THROWS_AS(first.decode_bits(small_code->substr(0U, small_code->size()-1U)), std::invalid_argument);
  REQUIRE_THROWS_AS(first.decode_bits(bits + "z"), std::invalid_argument);
}

TEST_CASE(huffman_arbitrary_bytes_ties_and_overflow_boundaries) {
  std::array<std::uint64_t,256> ties{};
  ties[0x00U]=1U; ties[0x7fU]=1U; ties[0x80U]=1U; ties[0xffU]=1U;
  HuffmanByteCodebook codebook(ties);
  REQUIRE(codebook.valid_codebook());
  REQUIRE_EQ(codebook.weighted_bit_count(), 8U);
  const std::vector<std::uint8_t> payload{0x00U,0xffU,0x80U,0x7fU,0x00U};
  REQUIRE_EQ(codebook.decode_bits(codebook.encode_bits(payload)), payload);

  std::array<std::uint64_t,256> merge_overflow{};
  merge_overflow[0]=std::numeric_limits<std::uint64_t>::max();
  merge_overflow[1]=1U;
  REQUIRE_THROWS_AS(HuffmanByteCodebook(merge_overflow), std::overflow_error);

  std::array<std::uint64_t,256> cost_overflow{};
  const std::uint64_t q=std::numeric_limits<std::uint64_t>::max()/3U;
  cost_overflow[0]=q; cost_overflow[1]=q; cost_overflow[2]=q;
  REQUIRE_THROWS_AS(HuffmanByteCodebook(cost_overflow), std::overflow_error);
}

TEST_CASE(huffman_randomized_optimality_prefix_and_round_trip) {
  std::mt19937_64 rng(0x485546464D414EULL);
  for (std::size_t trial=0; trial<700U; ++trial) {
    const std::size_t active=static_cast<std::size_t>(rng()%7U);
    std::vector<std::uint8_t> symbols;
    while (symbols.size()<active) {
      const auto candidate=static_cast<std::uint8_t>(rng()%256U);
      if (std::find(symbols.begin(), symbols.end(), candidate)==symbols.end()) symbols.push_back(candidate);
    }
    std::sort(symbols.begin(), symbols.end());
    std::array<std::uint64_t,256> frequencies{};
    std::vector<std::uint64_t> weights;
    for (const auto symbol:symbols) {
      const std::uint64_t weight=1U+(rng()%20U);
      frequencies[symbol]=weight;
      weights.push_back(weight);
    }
    HuffmanByteCodebook codebook(frequencies);
    REQUIRE(codebook.valid_codebook());
    REQUIRE_EQ(codebook.symbol_count(), active);
    REQUIRE_EQ(codebook.weighted_bit_count(), exhaustive_optimal_cost(weights));
    REQUIRE_EQ(codebook.weighted_bit_count(), replay_weighted_cost(codebook));

    if (!symbols.empty()) {
      std::vector<std::uint8_t> payload;
      const std::size_t length=static_cast<std::size_t>(rng()%80U);
      payload.reserve(length);
      for (std::size_t i=0;i<length;++i) payload.push_back(symbols[static_cast<std::size_t>(rng()%symbols.size())]);
      const std::string bits=codebook.encode_bits(payload);
      REQUIRE_EQ(codebook.decode_bits(bits), payload);
    }
  }
}
