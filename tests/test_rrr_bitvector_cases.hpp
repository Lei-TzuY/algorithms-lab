#pragma once

#include "algorithms/data_structures/rrr_bitvector.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::data_structures::RrrBitVector15;

namespace rrr_bitvector_test_detail {

inline std::size_t brute_rank1(
    const std::vector<std::uint8_t>& bits,
    const std::size_t end) {
  std::size_t total = 0U;
  for (std::size_t index = 0U; index < end; ++index) {
    total += bits[index] != 0U ? 1U : 0U;
  }
  return total;
}

inline std::optional<std::size_t> brute_select(
    const std::vector<std::uint8_t>& bits,
    const bool value, std::size_t ordinal) {
  for (std::size_t index = 0U; index < bits.size(); ++index) {
    if ((bits[index] != 0U) == value) {
      if (ordinal == 0U) {
        return index;
      }
      --ordinal;
    }
  }
  return std::nullopt;
}

inline void require_matches(
    const std::vector<std::uint8_t>& bits) {
  const RrrBitVector15 index(bits);
  REQUIRE(index.valid_structure());
  REQUIRE_EQ(index.size(), bits.size());
  REQUIRE(index.empty() == bits.empty());

  const std::size_t expected_blocks =
      bits.size() / RrrBitVector15::kBlockBits +
      ((bits.size() % RrrBitVector15::kBlockBits) != 0U ? 1U : 0U);
  REQUIRE_EQ(index.block_count(), expected_blocks);

  for (std::size_t position = 0U; position < bits.size(); ++position) {
    REQUIRE(index.access(position) == (bits[position] != 0U));
  }

  for (std::size_t end = 0U; end <= bits.size(); ++end) {
    const std::size_t one_rank = brute_rank1(bits, end);
    REQUIRE_EQ(index.rank1(end), one_rank);
    REQUIRE_EQ(index.rank0(end), end - one_rank);
  }

  const std::size_t ones = brute_rank1(bits, bits.size());
  const std::size_t zeros = bits.size() - ones;
  REQUIRE_EQ(index.ones(), ones);
  REQUIRE_EQ(index.zeros(), zeros);

  for (std::size_t ordinal = 0U; ordinal < ones; ++ordinal) {
    REQUIRE(index.select1(ordinal) ==
            brute_select(bits, true, ordinal));
  }
  for (std::size_t ordinal = 0U; ordinal < zeros; ++ordinal) {
    REQUIRE(index.select0(ordinal) ==
            brute_select(bits, false, ordinal));
  }
  REQUIRE(!index.select1(ones).has_value());
  REQUIRE(!index.select0(zeros).has_value());
}

}  // namespace rrr_bitvector_test_detail

TEST_CASE(rrr_bitvector_empty_and_validation_contract) {
  using namespace rrr_bitvector_test_detail;

  const std::vector<std::uint8_t> empty;
  const RrrBitVector15 index(empty);

  REQUIRE(index.empty());
  REQUIRE_EQ(index.size(), 0U);
  REQUIRE_EQ(index.ones(), 0U);
  REQUIRE_EQ(index.zeros(), 0U);
  REQUIRE_EQ(index.block_count(), 0U);
  REQUIRE_EQ(index.superblock_count(), 0U);
  REQUIRE(index.valid_structure());
  REQUIRE_EQ(index.rank1(0U), 0U);
  REQUIRE_EQ(index.rank0(0U), 0U);
  REQUIRE(!index.select1(0U).has_value());
  REQUIRE(!index.select0(0U).has_value());
  REQUIRE_THROWS_AS(index.access(0U), std::out_of_range);
  REQUIRE_THROWS_AS(index.rank1(1U), std::out_of_range);
  REQUIRE_THROWS_AS(index.rank0(1U), std::out_of_range);

  const std::vector<std::uint8_t> invalid{0U, 1U, 2U};
  REQUIRE_THROWS_AS(RrrBitVector15(invalid), std::invalid_argument);

  require_matches(empty);
}

TEST_CASE(rrr_bitvector_known_pattern_rank_and_select) {
  using namespace rrr_bitvector_test_detail;

  const std::vector<std::uint8_t> bits{
      1U, 0U, 1U, 1U, 0U, 0U, 1U, 0U,
      1U, 0U, 1U, 0U, 0U, 1U, 1U, 0U,
      1U, 1U, 0U, 0U, 1U, 0U, 1U, 0U};
  require_matches(bits);

  const RrrBitVector15 index(bits);
  REQUIRE(index.select1(0U) == std::optional<std::size_t>(0U));
  REQUIRE(index.select1(1U) == std::optional<std::size_t>(2U));
  REQUIRE(index.select0(0U) == std::optional<std::size_t>(1U));
  REQUIRE_EQ(index.rank1(15U), 8U);
  REQUIRE_EQ(index.rank1(bits.size()), index.ones());
}

TEST_CASE(rrr_bitvector_block_and_superblock_boundaries) {
  using namespace rrr_bitvector_test_detail;

  for (const std::size_t length :
       {1U, 14U, 15U, 16U, 29U, 30U, 31U,
        239U, 240U, 241U, 255U, 256U}) {
    std::vector<std::uint8_t> bits(length, 0U);
    for (std::size_t index = 0U; index < length; ++index) {
      bits[index] =
          ((index * 7U + length * 3U) % 11U) < 5U ? 1U : 0U;
    }
    require_matches(bits);

    std::fill(bits.begin(), bits.end(), 0U);
    require_matches(bits);
    std::fill(bits.begin(), bits.end(), 1U);
    require_matches(bits);
  }
}

TEST_CASE(rrr_bitvector_exhaustive_short_bitstrings) {
  using namespace rrr_bitvector_test_detail;

  for (std::size_t length = 0U; length <= 10U; ++length) {
    const std::size_t patterns = std::size_t{1} << length;
    for (std::size_t pattern = 0U; pattern < patterns; ++pattern) {
      std::vector<std::uint8_t> bits(length, 0U);
      for (std::size_t index = 0U; index < length; ++index) {
        bits[index] =
            ((pattern >> index) & std::size_t{1}) != 0U ? 1U : 0U;
      }
      require_matches(bits);
    }
  }
}

TEST_CASE(rrr_bitvector_random_differential_rank_select) {
  using namespace rrr_bitvector_test_detail;

  std::mt19937_64 random(0x525252B17ULL);
  for (std::size_t trial = 0U; trial < 350U; ++trial) {
    const std::size_t length =
        static_cast<std::size_t>(random() % 401U);
    std::vector<std::uint8_t> bits(length, 0U);

    for (std::size_t index = 0U; index < length; ++index) {
      const std::uint64_t mode = random() % 7U;
      if (mode == 0U) {
        bits[index] = 1U;
      } else if (mode == 1U) {
        bits[index] = 0U;
      } else {
        bits[index] = (random() & 1ULL) != 0ULL ? 1U : 0U;
      }
    }

    require_matches(bits);
  }
}
