#include "algorithms/data_structures/packed_rank_select.hpp"

#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::data_structures::PackedRankSelectBitVector;

std::size_t naive_rank(const std::vector<std::uint8_t>& bits, std::size_t end,
                       std::uint8_t value) {
  return static_cast<std::size_t>(std::count(
      bits.begin(), bits.begin() + static_cast<std::ptrdiff_t>(end), value));
}

std::optional<std::size_t> naive_select(const std::vector<std::uint8_t>& bits,
                                        std::size_t ordinal,
                                        std::uint8_t value) {
  std::size_t seen = 0U;
  for (std::size_t index = 0U; index < bits.size(); ++index) {
    if (bits[index] == value) {
      if (seen == ordinal) {
        return index;
      }
      ++seen;
    }
  }
  return std::nullopt;
}

void verify_against_naive(const std::vector<std::uint8_t>& bits) {
  const PackedRankSelectBitVector index(bits);
  REQUIRE_EQ(index.size(), bits.size());
  REQUIRE_EQ(index.empty(), bits.empty());

  for (std::size_t position = 0U; position < bits.size(); ++position) {
    REQUIRE_EQ(index.bit(position), bits[position] != 0U);
  }

  for (std::size_t end = 0U; end <= bits.size(); ++end) {
    REQUIRE_EQ(index.rank1(end), naive_rank(bits, end, 1U));
    REQUIRE_EQ(index.rank0(end), naive_rank(bits, end, 0U));
  }

  const std::size_t ones = naive_rank(bits, bits.size(), 1U);
  const std::size_t zeros = bits.size() - ones;
  REQUIRE_EQ(index.one_count(), ones);
  REQUIRE_EQ(index.zero_count(), zeros);

  for (std::size_t ordinal = 0U; ordinal < ones; ++ordinal) {
    REQUIRE_EQ(index.select1(ordinal), naive_select(bits, ordinal, 1U));
  }
  for (std::size_t ordinal = 0U; ordinal < zeros; ++ordinal) {
    REQUIRE_EQ(index.select0(ordinal), naive_select(bits, ordinal, 0U));
  }
  REQUIRE_EQ(index.select1(ones), std::optional<std::size_t>{});
  REQUIRE_EQ(index.select0(zeros), std::optional<std::size_t>{});

  const std::size_t words =
      bits.size() / 64U + (bits.size() % 64U != 0U ? 1U : 0U);
  REQUIRE_EQ(index.packed_word_count(), words);
  REQUIRE_EQ(index.rank_index_entries(), words + 1U);
  REQUIRE_EQ(index.logical_payload_bytes(),
             words * sizeof(std::uint64_t) +
                 (words + 1U) * sizeof(std::size_t));
}

}  // namespace

TEST_CASE(packed_rank_select_empty_singleton_and_validation) {
  verify_against_naive({});
  verify_against_naive({0U});
  verify_against_naive({1U});

  const std::vector<std::uint8_t> invalid{0U, 2U, 1U};
  REQUIRE_THROWS_AS(PackedRankSelectBitVector(invalid), std::invalid_argument);

  const std::vector<std::uint8_t> empty_bits;
  const PackedRankSelectBitVector empty(empty_bits);
  REQUIRE_THROWS_AS(empty.rank1(1U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.rank0(1U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.bit(0U), std::out_of_range);
}

TEST_CASE(packed_rank_select_word_boundaries_and_uniform_vectors) {
  for (const std::size_t length : {63U, 64U, 65U, 127U, 128U, 129U}) {
    std::vector<std::uint8_t> patterned(length, 0U);
    for (std::size_t index = 0U; index < length; ++index) {
      patterned[index] = static_cast<std::uint8_t>(
          index % 3U == 0U || index + 1U == length);
    }
    verify_against_naive(patterned);
    verify_against_naive(std::vector<std::uint8_t>(length, 0U));
    verify_against_naive(std::vector<std::uint8_t>(length, 1U));
  }
}

TEST_CASE(packed_rank_select_randomized_naive_differential) {
  std::mt19937_64 rng(0x17A17A17ULL);
  std::uniform_int_distribution<int> length_dist(0, 2048);
  std::bernoulli_distribution bit_dist(0.37);

  for (std::size_t trial = 0U; trial < 800U; ++trial) {
    const std::size_t length =
        static_cast<std::size_t>(length_dist(rng));
    std::vector<std::uint8_t> bits(length, 0U);
    for (auto& bit : bits) {
      bit = static_cast<std::uint8_t>(bit_dist(rng));
    }
    verify_against_naive(bits);
  }
}

TEST_CASE(packed_rank_select_alternating_dense_and_sparse_queries) {
  std::vector<std::uint8_t> alternating(513U, 0U);
  std::vector<std::uint8_t> sparse(513U, 0U);
  for (std::size_t index = 0U; index < alternating.size(); ++index) {
    alternating[index] = static_cast<std::uint8_t>(index % 2U);
    if (index % 97U == 0U) {
      sparse[index] = 1U;
    }
  }
  verify_against_naive(alternating);
  verify_against_naive(sparse);
}
