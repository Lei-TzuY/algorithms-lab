#pragma once

#include "algorithms/data_structures/min_max_heap.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace min_max_heap_test_detail {

using algorithms::data_structures::MinMaxHeap;

inline void require_extrema(const MinMaxHeap& heap,
                            const std::multiset<std::int64_t>& oracle) {
  REQUIRE(heap.valid_invariants());
  REQUIRE_EQ(heap.size(), oracle.size());
  REQUIRE_EQ(heap.empty(), oracle.empty());
  if (oracle.empty()) {
    REQUIRE_THROWS_AS(heap.minimum(), std::out_of_range);
    REQUIRE_THROWS_AS(heap.maximum(), std::out_of_range);
    return;
  }

  REQUIRE_EQ(heap.minimum(), *oracle.begin());
  auto last = oracle.end();
  --last;
  REQUIRE_EQ(heap.maximum(), *last);
}

}  // namespace min_max_heap_test_detail

TEST_CASE(min_max_heap_empty_full_width_and_duplicate_boundaries) {
  using namespace min_max_heap_test_detail;

  MinMaxHeap heap;
  REQUIRE_THROWS_AS(heap.minimum(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.maximum(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.pop_minimum(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.pop_maximum(), std::out_of_range);

  std::multiset<std::int64_t> oracle;
  const std::vector<std::int64_t> values = {
      std::numeric_limits<std::int64_t>::min(),
      0,
      std::numeric_limits<std::int64_t>::max(),
      -1,
      1,
      0,
      std::numeric_limits<std::int64_t>::max(),
      std::numeric_limits<std::int64_t>::min()};

  for (const std::int64_t value : values) {
    heap.push(value);
    oracle.insert(value);
    require_extrema(heap, oracle);
  }

  while (!oracle.empty()) {
    if ((oracle.size() & 1U) == 0U) {
      const std::int64_t expected = *oracle.begin();
      REQUIRE_EQ(heap.pop_minimum(), expected);
      oracle.erase(oracle.begin());
    } else {
      auto last = oracle.end();
      --last;
      const std::int64_t expected = *last;
      REQUIRE_EQ(heap.pop_maximum(), expected);
      oracle.erase(last);
    }
    require_extrema(heap, oracle);
  }
}

TEST_CASE(min_max_heap_deep_trickle_chains_preserve_both_extrema) {
  using namespace min_max_heap_test_detail;

  MinMaxHeap heap;
  constexpr std::int64_t count = 4096;
  for (std::int64_t value = 0; value < count; ++value) {
    heap.push(value);
    if ((value & 127) == 0) {
      REQUIRE(heap.valid_invariants());
    }
  }

  std::int64_t low = 0;
  std::int64_t high = count - 1;
  bool take_minimum = true;
  while (!heap.empty()) {
    if (take_minimum) {
      REQUIRE_EQ(heap.minimum(), low);
      REQUIRE_EQ(heap.pop_minimum(), low);
      ++low;
    } else {
      REQUIRE_EQ(heap.maximum(), high);
      REQUIRE_EQ(heap.pop_maximum(), high);
      --high;
    }
    take_minimum = !take_minimum;
    if ((heap.size() & 127U) == 0U) {
      REQUIRE(heap.valid_invariants());
    }
  }
  REQUIRE_EQ(low, high + 1);
}

TEST_CASE(min_max_heap_descending_insertion_and_duplicate_plateaus) {
  using namespace min_max_heap_test_detail;

  MinMaxHeap heap;
  std::multiset<std::int64_t> oracle;
  for (std::int64_t value = 2048; value >= -2048; --value) {
    const std::int64_t inserted = value / 7;
    heap.push(inserted);
    oracle.insert(inserted);
  }
  require_extrema(heap, oracle);

  while (!oracle.empty()) {
    const bool remove_minimum = (oracle.size() % 3U) != 0U;
    if (remove_minimum) {
      const std::int64_t expected = *oracle.begin();
      REQUIRE_EQ(heap.pop_minimum(), expected);
      oracle.erase(oracle.begin());
    } else {
      auto last = oracle.end();
      --last;
      const std::int64_t expected = *last;
      REQUIRE_EQ(heap.pop_maximum(), expected);
      oracle.erase(last);
    }
    if ((oracle.size() & 63U) == 0U) {
      require_extrema(heap, oracle);
    }
  }
  require_extrema(heap, oracle);
}

TEST_CASE(min_max_heap_randomized_trace_matches_multiset_oracle) {
  using namespace min_max_heap_test_detail;

  std::mt19937_64 random(0x51A9BEEFULL);
  MinMaxHeap heap;
  std::multiset<std::int64_t> oracle;

  for (std::size_t step = 0U; step < 20000U; ++step) {
    const unsigned operation = static_cast<unsigned>(random() % 6U);
    if (operation < 2U || oracle.empty()) {
      std::int64_t value =
          static_cast<std::int64_t>(random() % 2000001ULL) - 1000000;
      if ((step % 997U) == 0U) {
        value = std::numeric_limits<std::int64_t>::min();
      } else if ((step % 991U) == 0U) {
        value = std::numeric_limits<std::int64_t>::max();
      }
      heap.push(value);
      oracle.insert(value);
    } else if (operation == 2U) {
      const std::int64_t expected = *oracle.begin();
      REQUIRE_EQ(heap.pop_minimum(), expected);
      oracle.erase(oracle.begin());
    } else if (operation == 3U) {
      auto last = oracle.end();
      --last;
      const std::int64_t expected = *last;
      REQUIRE_EQ(heap.pop_maximum(), expected);
      oracle.erase(last);
    } else {
      REQUIRE_EQ(heap.minimum(), *oracle.begin());
      auto last = oracle.end();
      --last;
      REQUIRE_EQ(heap.maximum(), *last);
    }

    require_extrema(heap, oracle);
  }
}
