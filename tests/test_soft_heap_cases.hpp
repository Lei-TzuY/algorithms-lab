#pragma once

#include "algorithms/data_structures/soft_heap.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace soft_heap_test_detail {

using algorithms::data_structures::SoftHeap;

void verify_min_heap(const SoftHeap<int>& heap,
                     const std::multiset<int>& oracle) {
  REQUIRE_EQ(heap.size(), oracle.size());
  REQUIRE(heap.corrupted_count() <= heap.corruption_budget());
  REQUIRE(heap.valid_structure());
}

}  // namespace soft_heap_test_detail

TEST_CASE(soft_heap_parameter_validation_and_exact_low_rank_boundary) {
  using algorithms::data_structures::SoftHeap;

  REQUIRE_THROWS_AS(SoftHeap<int>(0), std::invalid_argument);
  REQUIRE_THROWS_AS(
      SoftHeap<int>(std::numeric_limits<std::size_t>::max()),
      std::invalid_argument);

  SoftHeap<int> heap(8);
  REQUIRE(heap.empty());
  REQUIRE(heap.valid_structure());
  REQUIRE_EQ(heap.corruption_budget(), 0U);
  REQUIRE_THROWS_AS(heap.top_current_key(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.pop(), std::out_of_range);

  for (int value : {9, 1, 7, 1, 3, 0, 5}) {
    heap.push(value);
    REQUIRE(heap.valid_structure());
    REQUIRE_EQ(heap.corrupted_count(), 0U);
  }

  const std::vector<int> expected{0, 1, 1, 3, 5, 7, 9};
  std::vector<int> actual;
  while (!heap.empty()) {
    auto extracted = heap.pop();
    REQUIRE(!extracted.corrupted);
    REQUIRE_EQ(extracted.value, extracted.current_key);
    actual.push_back(extracted.value);
    REQUIRE(heap.valid_structure());
  }
  REQUIRE(actual == expected);
}

TEST_CASE(soft_heap_exhibits_real_corruption_with_certified_live_budget) {
  using algorithms::data_structures::SoftHeap;

  // epsilon = 1/2. Multiplication by 73 is a deterministic permutation modulo
  // 512; it makes the approximate extraction order visibly non-exact without
  // depending on a library-specific shuffle implementation.
  SoftHeap<int> heap(1);
  for (int index = 0; index < 512; ++index) {
    heap.push((index * 73) % 512);
    REQUIRE(heap.corrupted_count() <= heap.corruption_budget());
    REQUIRE(heap.valid_structure());
  }

  REQUIRE(heap.corrupted_count() > 0U);
  bool saw_corrupt_extraction = false;
  bool saw_original_key_inversion = false;
  int previous_original = std::numeric_limits<int>::min();
  int previous_current = std::numeric_limits<int>::min();
  std::set<int> extracted_values;

  while (!heap.empty()) {
    auto extracted = heap.pop();
    REQUIRE(extracted.value <= extracted.current_key);
    REQUIRE(previous_current <= extracted.current_key);
    if (extracted.value < previous_original) {
      saw_original_key_inversion = true;
    }
    previous_original = extracted.value;
    previous_current = extracted.current_key;
    saw_corrupt_extraction = saw_corrupt_extraction || extracted.corrupted;
    extracted_values.insert(extracted.value);

    REQUIRE(heap.corrupted_count() <= heap.corruption_budget());
    REQUIRE(heap.valid_structure());
  }

  REQUIRE(saw_corrupt_extraction);
  REQUIRE(saw_original_key_inversion);
  REQUIRE_EQ(extracted_values.size(), 512U);
}

TEST_CASE(soft_heap_meld_move_error_rate_and_custom_order) {
  using algorithms::data_structures::SoftHeap;

  SoftHeap<int> first(2);
  SoftHeap<int> second(2);
  for (int value = 0; value < 300; ++value) {
    if (value % 2 == 0) {
      first.push(value);
    } else {
      second.push(value);
    }
  }
  const std::size_t total_insertions =
      first.insertion_count() + second.insertion_count();

  first.meld(std::move(second));
  REQUIRE(second.empty());
  REQUIRE_EQ(second.insertion_count(), 0U);
  REQUIRE(second.valid_structure());
  REQUIRE_EQ(first.size(), 300U);
  REQUIRE_EQ(first.insertion_count(), total_insertions);
  REQUIRE(first.valid_structure());

  first.meld(std::move(first));
  REQUIRE_EQ(first.size(), 300U);
  REQUIRE(first.valid_structure());

  SoftHeap<int> moved(std::move(first));
  REQUIRE(first.empty());
  REQUIRE(first.valid_structure());
  REQUIRE(moved.valid_structure());

  SoftHeap<int> assigned(2);
  assigned.push(-1);
  assigned = std::move(moved);
  REQUIRE(moved.empty());
  REQUIRE(moved.valid_structure());
  REQUIRE_EQ(assigned.size(), 300U);
  REQUIRE(assigned.valid_structure());

  SoftHeap<int> mismatched_rate(3);
  mismatched_rate.push(999);
  REQUIRE_THROWS_AS(assigned.meld(std::move(mismatched_rate)),
                    std::invalid_argument);
  REQUIRE_EQ(mismatched_rate.size(), 1U);

  SoftHeap<int, std::greater<int>> max_heap(1);
  for (int index = 0; index < 512; ++index) {
    max_heap.push((index * 73) % 512);
  }
  int previous_current = std::numeric_limits<int>::max();
  while (!max_heap.empty()) {
    auto extracted = max_heap.pop();
    REQUIRE(extracted.current_key <= extracted.value);
    REQUIRE(extracted.current_key <= previous_current);
    previous_current = extracted.current_key;
    REQUIRE(max_heap.valid_structure());
  }
}

TEST_CASE(soft_heap_randomized_multiset_meld_and_corruption_differential) {
  using algorithms::data_structures::SoftHeap;
  constexpr std::size_t kHeapCount = 6;

  std::vector<SoftHeap<int>> heaps;
  heaps.reserve(kHeapCount);
  for (std::size_t index = 0; index < kHeapCount; ++index) {
    heaps.emplace_back(2);
  }
  std::vector<std::multiset<int>> oracles(kHeapCount);

  std::mt19937 random(0x50F7EA9U);
  std::uniform_int_distribution<int> operation_dist(0, 99);
  std::uniform_int_distribution<int> value_dist(-2000, 2000);
  std::uniform_int_distribution<int> heap_dist(
      0, static_cast<int>(kHeapCount - 1U));

  for (int step = 0; step < 6000; ++step) {
    const auto index = static_cast<std::size_t>(heap_dist(random));
    const int operation = operation_dist(random);
    std::size_t other = index;

    if (operation < 58) {
      const int value = value_dist(random);
      heaps[index].push(value);
      oracles[index].insert(value);
    } else if (operation < 82) {
      if (!oracles[index].empty()) {
        auto extracted = heaps[index].pop();
        const auto found = oracles[index].find(extracted.value);
        REQUIRE(found != oracles[index].end());
        oracles[index].erase(found);
        REQUIRE(extracted.value <= extracted.current_key);
      }
    } else {
      other = static_cast<std::size_t>(heap_dist(random));
      if (other == index) {
        other = (other + 1U) % kHeapCount;
      }
      heaps[index].meld(std::move(heaps[other]));
      oracles[index].insert(oracles[other].begin(), oracles[other].end());
      oracles[other].clear();
    }

    for (std::size_t heap_index = 0; heap_index < kHeapCount; ++heap_index) {
      REQUIRE_EQ(heaps[heap_index].size(), oracles[heap_index].size());
      if (step % 97 == 0 || heap_index == index || heap_index == other) {
        REQUIRE(heaps[heap_index].corrupted_count() <=
                heaps[heap_index].corruption_budget());
        REQUIRE(heaps[heap_index].valid_structure());
      }
    }
  }

  for (std::size_t index = 0; index < kHeapCount; ++index) {
    std::multiset<int> extracted;
    while (!heaps[index].empty()) {
      extracted.insert(heaps[index].pop().value);
    }
    REQUIRE(extracted == oracles[index]);
    REQUIRE(heaps[index].valid_structure());
  }
}

TEST_CASE(soft_heap_corruption_bound_stress_across_error_powers) {
  using algorithms::data_structures::SoftHeap;

  for (std::size_t power : {1U, 2U, 3U, 5U}) {
    constexpr std::size_t kHeapCount = 5;
    std::vector<SoftHeap<int>> heaps;
    heaps.reserve(kHeapCount);
    for (std::size_t index = 0; index < kHeapCount; ++index) {
      heaps.emplace_back(power);
    }
    std::vector<std::multiset<int>> oracles(kHeapCount);

    std::mt19937 random(static_cast<unsigned>(0xABC000U + power));
    std::uniform_int_distribution<int> operation_dist(0, 99);
    std::uniform_int_distribution<int> value_dist(-100000, 100000);
    std::uniform_int_distribution<int> heap_dist(
        0, static_cast<int>(kHeapCount - 1U));

    for (int step = 0; step < 4000; ++step) {
      const auto index = static_cast<std::size_t>(heap_dist(random));
      const int operation = operation_dist(random);
      std::size_t other = index;

      if (operation < 62) {
        const int value = value_dist(random);
        heaps[index].push(value);
        oracles[index].insert(value);
      } else if (operation < 84) {
        if (!oracles[index].empty()) {
          const auto extracted = heaps[index].pop();
          const auto found = oracles[index].find(extracted.value);
          REQUIRE(found != oracles[index].end());
          oracles[index].erase(found);
        }
      } else {
        other = static_cast<std::size_t>(heap_dist(random));
        if (other == index) {
          other = (other + 1U) % kHeapCount;
        }
        heaps[index].meld(std::move(heaps[other]));
        oracles[index].insert(oracles[other].begin(), oracles[other].end());
        oracles[other].clear();
      }

      for (std::size_t heap_index = 0; heap_index < kHeapCount; ++heap_index) {
        REQUIRE_EQ(heaps[heap_index].size(), oracles[heap_index].size());
        if (step % 101 == 0 || heap_index == index || heap_index == other) {
          REQUIRE(heaps[heap_index].corrupted_count() <=
                  heaps[heap_index].corruption_budget());
          REQUIRE(heaps[heap_index].valid_structure());
        }
      }
    }
  }
}
