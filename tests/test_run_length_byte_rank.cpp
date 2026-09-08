#include "algorithms/data_structures/run_length_byte_rank.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace {

std::size_t naive_rank(const std::vector<std::uint8_t>& values,
                       std::uint8_t value, std::size_t end) {
  std::size_t count = 0U;
  for (std::size_t index = 0U; index < end; ++index) {
    if (values[index] == value) {
      ++count;
    }
  }
  return count;
}

std::size_t naive_run_count(const std::vector<std::uint8_t>& values) {
  if (values.empty()) {
    return 0U;
  }
  std::size_t runs = 1U;
  for (std::size_t index = 1U; index < values.size(); ++index) {
    if (values[index] != values[index - 1U]) {
      ++runs;
    }
  }
  return runs;
}

}  // namespace

TEST_CASE(run_length_byte_rank_empty_and_deterministic_runs) {
  using algorithms::data_structures::RunLengthByteRankIndex;

  const std::vector<std::uint8_t> empty;
  const RunLengthByteRankIndex empty_index{empty};
  REQUIRE(empty_index.empty());
  REQUIRE_EQ(empty_index.size(), std::size_t{0});
  REQUIRE_EQ(empty_index.run_count(), std::size_t{0});
  REQUIRE_EQ(empty_index.logical_payload_bytes(), std::size_t{0});
  REQUIRE_EQ(empty_index.rank(std::uint8_t{0}, 0U), std::size_t{0});
  REQUIRE_THROWS_AS(empty_index.access(0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty_index.rank(std::uint8_t{0}, 1U),
                    std::out_of_range);

  const std::vector<std::uint8_t> values{1U, 1U, 1U, 2U, 2U, 1U, 255U,
                                         255U};
  const RunLengthByteRankIndex index{values};
  REQUIRE_EQ(index.size(), values.size());
  REQUIRE_EQ(index.run_count(), std::size_t{4});
  REQUIRE_EQ(index.logical_payload_bytes(),
             std::size_t{4} *
                 (std::size_t{3} * sizeof(std::size_t) +
                  sizeof(std::uint8_t)));

  for (std::size_t position = 0U; position < values.size(); ++position) {
    REQUIRE_EQ(index.access(position), values[position]);
  }
  REQUIRE_THROWS_AS(index.access(values.size()), std::out_of_range);

  for (std::size_t end = 0U; end <= values.size(); ++end) {
    for (const std::uint8_t value :
         {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{255}}) {
      REQUIRE_EQ(index.rank(value, end), naive_rank(values, value, end));
    }
  }
  REQUIRE_EQ(index.rank(std::uint8_t{1}, 2U, 6U), std::size_t{2});
  REQUIRE_THROWS_AS(index.rank(std::uint8_t{1}, 5U, 4U),
                    std::out_of_range);
  REQUIRE_THROWS_AS(index.rank(std::uint8_t{1}, 0U, values.size() + 1U),
                    std::out_of_range);
}

TEST_CASE(run_length_byte_rank_randomized_differential_against_naive_sequence) {
  using algorithms::data_structures::RunLengthByteRankIndex;

  std::mt19937_64 rng(0x21B17B17ULL);
  std::uniform_int_distribution<std::size_t> length_dist(0U, 300U);
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);

  for (std::size_t trial = 0U; trial < 1000U; ++trial) {
    const std::size_t length = length_dist(rng);
    std::vector<std::uint8_t> values(length);
    for (std::uint8_t& value : values) {
      value = static_cast<std::uint8_t>(byte_dist(rng));
    }

    const RunLengthByteRankIndex index{values};
    REQUIRE_EQ(index.size(), values.size());
    REQUIRE_EQ(index.run_count(), naive_run_count(values));
    REQUIRE_EQ(index.logical_payload_bytes(),
               index.run_count() *
                   (std::size_t{3} * sizeof(std::size_t) +
                    sizeof(std::uint8_t)));

    for (std::size_t position = 0U; position < values.size(); ++position) {
      REQUIRE_EQ(index.access(position), values[position]);
    }

    std::uniform_int_distribution<std::size_t> endpoint_dist(0U, length);
    for (std::size_t query = 0U; query < 200U; ++query) {
      const std::uint8_t value = static_cast<std::uint8_t>(byte_dist(rng));
      const std::size_t first = endpoint_dist(rng);
      const std::size_t second = endpoint_dist(rng);
      const std::size_t begin = std::min(first, second);
      const std::size_t end = std::max(first, second);
      const std::size_t expected_end = naive_rank(values, value, end);
      const std::size_t expected_begin = naive_rank(values, value, begin);
      REQUIRE_EQ(index.rank(value, end), expected_end);
      REQUIRE_EQ(index.rank(value, begin, end),
                 expected_end - expected_begin);
    }
  }
}
