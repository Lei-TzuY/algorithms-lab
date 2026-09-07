#include "algorithms/data_structures/byte_wavelet_matrix.hpp"
#include "algorithms/data_structures/packed_rank_select.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::data_structures::ByteWaveletMatrix;
using algorithms::data_structures::PackedRankSelectBitVector;

namespace {
std::size_t naive_rank(const std::vector<std::uint8_t>& values,
                       std::uint8_t value, std::size_t begin,
                       std::size_t end) {
  std::size_t result = 0U;
  for (std::size_t index = begin; index < end; ++index) {
    result += values[index] == value ? 1U : 0U;
  }
  return result;
}

std::optional<std::size_t> naive_select(
    const std::vector<std::uint8_t>& values, std::uint8_t value,
    std::size_t ordinal) {
  std::size_t seen = 0U;
  for (std::size_t index = 0U; index < values.size(); ++index) {
    if (values[index] == value) {
      if (seen == ordinal) {
        return index;
      }
      ++seen;
    }
  }
  return std::nullopt;
}

std::uint8_t naive_kth(const std::vector<std::uint8_t>& values,
                       std::size_t begin, std::size_t end,
                       std::size_t ordinal) {
  const auto first =
      values.begin() + static_cast<std::ptrdiff_t>(begin);
  const auto last = values.begin() + static_cast<std::ptrdiff_t>(end);
  std::vector<std::uint8_t> slice(first, last);
  std::sort(slice.begin(), slice.end());
  return slice[ordinal];
}
}  // namespace

TEST_CASE(byte_wavelet_known_vector) {
  const std::vector<std::uint8_t> values{3U, 1U, 4U, 1U, 5U,
                                         9U, 2U, 6U, 5U};
  ByteWaveletMatrix index(values);
  REQUIRE_EQ(index.size(), values.size());
  for (std::size_t position = 0U; position < values.size(); ++position) {
    REQUIRE_EQ(index.access(position), values[position]);
  }
  REQUIRE_EQ(index.rank(1U, values.size()), 2U);
  REQUIRE_EQ(index.rank(5U, 0U, values.size()), 2U);
  REQUIRE_EQ(index.select(1U, 0U), std::optional<std::size_t>{1U});
  REQUIRE_EQ(index.select(1U, 1U), std::optional<std::size_t>{3U});
  REQUIRE(!index.select(1U, 2U).has_value());
  REQUIRE_EQ(index.kth_smallest(0U, values.size(), 0U), 1U);
  REQUIRE_EQ(index.kth_smallest(0U, values.size(), 8U), 9U);
  REQUIRE_EQ(index.kth_smallest(2U, 8U, 2U),
             naive_kth(values, 2U, 8U, 2U));
}

TEST_CASE(byte_wavelet_empty_and_bounds) {
  const std::vector<std::uint8_t> empty;
  ByteWaveletMatrix index(empty);
  REQUIRE(index.empty());
  REQUIRE_EQ(index.rank(42U, 0U), 0U);
  REQUIRE(!index.select(42U, 0U).has_value());
  REQUIRE_THROWS_AS(index.access(0U), std::out_of_range);
  REQUIRE_THROWS_AS(index.rank(1U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(index.rank(1U, 1U, 0U), std::invalid_argument);
  REQUIRE_THROWS_AS(index.kth_smallest(0U, 0U, 0U), std::out_of_range);
}

TEST_CASE(byte_wavelet_all_byte_values) {
  std::vector<std::uint8_t> values(256U);
  for (std::size_t position = 0U; position < values.size(); ++position) {
    values[position] = static_cast<std::uint8_t>(position);
  }
  ByteWaveletMatrix index(values);
  for (std::size_t position = 0U; position < values.size(); ++position) {
    const auto value = static_cast<std::uint8_t>(position);
    REQUIRE_EQ(index.access(position), value);
    REQUIRE_EQ(index.rank(value, values.size()), 1U);
    REQUIRE_EQ(index.select(value, 0U),
               std::optional<std::size_t>{position});
    REQUIRE_EQ(index.kth_smallest(0U, values.size(), position), value);
  }
}

TEST_CASE(byte_wavelet_binary_cross_layer_rank_select) {
  std::mt19937_64 rng(0xB17B17ULL);
  std::vector<std::uint8_t> bits(1025U);
  for (auto& bit : bits) {
    bit = static_cast<std::uint8_t>(rng() & 1U);
  }
  PackedRankSelectBitVector packed(bits);
  ByteWaveletMatrix index(bits);
  for (std::size_t end = 0U; end <= bits.size(); ++end) {
    REQUIRE_EQ(index.rank(1U, end), packed.rank1(end));
    REQUIRE_EQ(index.rank(0U, end), packed.rank0(end));
  }
  for (std::size_t ordinal = 0U; ordinal < packed.one_count(); ++ordinal) {
    REQUIRE_EQ(index.select(1U, ordinal), packed.select1(ordinal));
  }
  for (std::size_t ordinal = 0U; ordinal < packed.zero_count(); ++ordinal) {
    REQUIRE_EQ(index.select(0U, ordinal), packed.select0(ordinal));
  }
}

TEST_CASE(byte_wavelet_logical_payload_accounting) {
  for (const std::size_t length :
       {0U, 1U, 63U, 64U, 65U, 129U, 1024U}) {
    std::vector<std::uint8_t> values(length, 7U);
    ByteWaveletMatrix index(values);
    const std::size_t words =
        length / 64U + (length % 64U != 0U ? 1U : 0U);
    const std::size_t per_level =
        words * sizeof(std::uint64_t) +
        (words + 1U) * sizeof(std::size_t);
    const std::size_t expected =
        ByteWaveletMatrix::kLevels * per_level +
        ByteWaveletMatrix::kLevels * sizeof(std::size_t);
    REQUIRE_EQ(index.logical_payload_bytes(), expected);
  }
}

TEST_CASE(byte_wavelet_randomized_differential) {
  std::mt19937_64 rng(0x18A7E17ULL);
  std::uniform_int_distribution<std::size_t> length_dist(0U, 768U);
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::size_t length = length_dist(rng);
    std::vector<std::uint8_t> values(length);
    for (auto& value : values) {
      value = static_cast<std::uint8_t>(byte_dist(rng));
    }
    ByteWaveletMatrix index(values);
    REQUIRE_EQ(index.size(), length);
    for (std::size_t position = 0U; position < length; ++position) {
      REQUIRE_EQ(index.access(position), values[position]);
    }

    for (std::size_t query = 0U; query < 80U; ++query) {
      const std::uint8_t value =
          static_cast<std::uint8_t>(byte_dist(rng));
      const std::size_t end =
          length == 0U
              ? 0U
              : static_cast<std::size_t>(rng() % (length + 1U));
      REQUIRE_EQ(index.rank(value, end),
                 naive_rank(values, value, 0U, end));

      std::size_t begin =
          length == 0U
              ? 0U
              : static_cast<std::size_t>(rng() % (length + 1U));
      std::size_t range_end =
          length == 0U
              ? 0U
              : static_cast<std::size_t>(rng() % (length + 1U));
      if (begin > range_end) {
        std::swap(begin, range_end);
      }
      REQUIRE_EQ(index.rank(value, begin, range_end),
                 naive_rank(values, value, begin, range_end));

      const std::size_t count = naive_rank(values, value, 0U, length);
      const std::size_t ordinal =
          count == 0U
              ? 0U
              : static_cast<std::size_t>(rng() % (count + 2U));
      REQUIRE_EQ(index.select(value, ordinal),
                 naive_select(values, value, ordinal));

      if (begin < range_end) {
        const std::size_t kth =
            static_cast<std::size_t>(rng() % (range_end - begin));
        REQUIRE_EQ(index.kth_smallest(begin, range_end, kth),
                   naive_kth(values, begin, range_end, kth));
      }
    }
  }
}
