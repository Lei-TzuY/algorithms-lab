#pragma once

#include "algorithms/data_structures/packed_memory_array.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::data_structures::PackedMemoryArraySequence;

namespace packed_memory_array_test_detail {

inline void require_matches(
    const PackedMemoryArraySequence& pma,
    const std::vector<std::int64_t>& oracle) {
  REQUIRE_EQ(pma.size(), oracle.size());
  REQUIRE(pma.empty() == oracle.empty());
  REQUIRE(pma.valid_structure());
  REQUIRE(std::has_single_bit(pma.capacity()));
  REQUIRE(pma.capacity() >= 16U);
  REQUIRE(pma.to_vector() == oracle);

  for (std::size_t rank = 0U; rank < oracle.size(); ++rank) {
    REQUIRE_EQ(pma.at(rank), oracle[rank]);
  }
}

}  // namespace packed_memory_array_test_detail

TEST_CASE(packed_memory_array_empty_and_initial_sequence_contract) {
  using namespace packed_memory_array_test_detail;

  PackedMemoryArraySequence empty;
  REQUIRE(empty.empty());
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_EQ(empty.capacity(), 16U);
  REQUIRE(empty.valid_structure());
  REQUIRE_THROWS_AS(empty.at(0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.erase(0U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.insert(1U, 7), std::out_of_range);

  const std::vector<std::int64_t> values{
      4, -2, 4, 9, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(), 0};
  const PackedMemoryArraySequence initialized(values);
  require_matches(initialized, values);
}

TEST_CASE(packed_memory_array_front_middle_back_and_duplicates) {
  using namespace packed_memory_array_test_detail;

  PackedMemoryArraySequence pma;
  std::vector<std::int64_t> oracle;

  const auto insert = [&](const std::size_t rank,
                          const std::int64_t value) {
    pma.insert(rank, value);
    oracle.insert(
        oracle.begin() + static_cast<std::ptrdiff_t>(rank),
        value);
    require_matches(pma, oracle);
  };

  insert(0U, 10);
  insert(1U, 30);
  insert(1U, 20);
  insert(0U, 5);
  insert(4U, 40);
  insert(2U, 20);
  insert(3U, std::numeric_limits<std::int64_t>::min());
  insert(5U, std::numeric_limits<std::int64_t>::max());

  REQUIRE_EQ(pma.erase(3U), oracle[3U]);
  oracle.erase(oracle.begin() + 3);
  require_matches(pma, oracle);

  REQUIRE_EQ(pma.erase(0U), oracle.front());
  oracle.erase(oracle.begin());
  require_matches(pma, oracle);

  REQUIRE_EQ(pma.erase(pma.size() - 1U), oracle.back());
  oracle.pop_back();
  require_matches(pma, oracle);
}

TEST_CASE(packed_memory_array_grows_and_shrinks_without_reordering) {
  using namespace packed_memory_array_test_detail;

  PackedMemoryArraySequence pma;
  std::vector<std::int64_t> oracle;

  for (std::size_t index = 0U; index < 1500U; ++index) {
    const std::int64_t value =
        static_cast<std::int64_t>(index) - 750;
    const std::size_t rank =
        index % 3U == 0U ? 0U : oracle.size();
    pma.insert(rank, value);
    oracle.insert(
        oracle.begin() + static_cast<std::ptrdiff_t>(rank),
        value);
  }

  REQUIRE(pma.capacity() > 16U);
  require_matches(pma, oracle);

  while (oracle.size() > 3U) {
    const std::size_t rank =
        oracle.size() % 2U == 0U ? oracle.size() / 2U : 0U;
    const std::int64_t expected = oracle[rank];
    REQUIRE_EQ(pma.erase(rank), expected);
    oracle.erase(
        oracle.begin() + static_cast<std::ptrdiff_t>(rank));
  }

  require_matches(pma, oracle);
  REQUIRE_EQ(pma.capacity(), 16U);
}

TEST_CASE(packed_memory_array_adversarial_front_and_middle_updates) {
  using namespace packed_memory_array_test_detail;

  PackedMemoryArraySequence pma;
  std::vector<std::int64_t> oracle;

  for (std::size_t step = 0U; step < 600U; ++step) {
    const std::size_t rank =
        oracle.empty() ? 0U :
        (step % 2U == 0U ? 0U : oracle.size() / 2U);
    const std::int64_t value =
        static_cast<std::int64_t>(step % 37U) - 18;
    pma.insert(rank, value);
    oracle.insert(
        oracle.begin() + static_cast<std::ptrdiff_t>(rank),
        value);

    if (step % 5U == 4U) {
      const std::size_t erase_rank = oracle.size() / 3U;
      REQUIRE_EQ(pma.erase(erase_rank), oracle[erase_rank]);
      oracle.erase(
          oracle.begin() +
          static_cast<std::ptrdiff_t>(erase_rank));
    }

    if (step % 17U == 0U) {
      require_matches(pma, oracle);
    }
  }

  require_matches(pma, oracle);
}

TEST_CASE(packed_memory_array_randomized_trace_matches_vector) {
  using namespace packed_memory_array_test_detail;

  std::mt19937_64 random(0x504D415EEDULL);
  PackedMemoryArraySequence pma;
  std::vector<std::int64_t> oracle;

  for (std::size_t step = 0U; step < 3500U; ++step) {
    const bool do_insert =
        oracle.empty() ||
        (oracle.size() < 220U && (random() % 100U) < 61U);

    if (do_insert) {
      const std::size_t rank =
          static_cast<std::size_t>(
              random() % (oracle.size() + 1U));
      std::int64_t value{};
      const std::uint64_t pick = random() % 29U;
      if (pick == 0U) {
        value = std::numeric_limits<std::int64_t>::min();
      } else if (pick == 1U) {
        value = std::numeric_limits<std::int64_t>::max();
      } else {
        value =
            static_cast<std::int64_t>(random() % 101U) - 50;
      }

      pma.insert(rank, value);
      oracle.insert(
          oracle.begin() + static_cast<std::ptrdiff_t>(rank),
          value);
    } else {
      const std::size_t rank =
          static_cast<std::size_t>(
              random() % oracle.size());
      const std::int64_t expected = oracle[rank];
      REQUIRE_EQ(pma.erase(rank), expected);
      oracle.erase(
          oracle.begin() + static_cast<std::ptrdiff_t>(rank));
    }

    require_matches(pma, oracle);
  }
}
