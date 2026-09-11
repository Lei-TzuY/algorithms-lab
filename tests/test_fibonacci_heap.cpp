#include "algorithms/data_structures/fibonacci_heap.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <utility>
#include <vector>

using algorithms::data_structures::FibonacciMinHeap;

TEST_CASE(fibonacci_heap_basic_handles_and_boundaries) {
  FibonacciMinHeap heap;
  REQUIRE(heap.empty());
  REQUIRE(heap.valid_structure());
  REQUIRE_THROWS_AS(heap.minimum(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.extract_min(), std::out_of_range);

  const auto high = heap.insert(5);
  const auto first_three = heap.insert(3);
  const auto second_three = heap.insert(3);
  REQUIRE_EQ(heap.size(), std::size_t{3});
  REQUIRE_EQ(heap.minimum().key, std::int64_t{3});
  REQUIRE_EQ(heap.minimum().handle, std::min(first_three, second_three));
  REQUIRE(heap.valid_structure());

  heap.decrease_key(high, std::numeric_limits<std::int64_t>::min());
  const FibonacciMinHeap::Entry expected_min{
      high, std::numeric_limits<std::int64_t>::min()};
  REQUIRE(heap.minimum() == expected_min);
  REQUIRE(heap.valid_structure());

  REQUIRE_THROWS_AS(heap.decrease_key(high, 2), std::invalid_argument);
  const auto extracted = heap.extract_min();
  REQUIRE(extracted == expected_min);
  REQUIRE(!heap.contains(high));
  REQUIRE(heap.valid_structure());
  REQUIRE_THROWS_AS(heap.decrease_key(high, 0), std::out_of_range);
}

TEST_CASE(fibonacci_heap_meld_preserves_handles_and_move_semantics) {
  FibonacciMinHeap first;
  FibonacciMinHeap second;
  const auto first_handle = first.insert(10);
  const auto second_handle = second.insert(7);
  const auto decreased_after_meld = second.insert(20);

  first.meld(std::move(second));
  REQUIRE(second.empty());
  REQUIRE_EQ(first.size(), std::size_t{3});
  REQUIRE(first.contains(first_handle));
  REQUIRE(first.contains(second_handle));
  REQUIRE(first.contains(decreased_after_meld));

  first.decrease_key(decreased_after_meld, 1);
  REQUIRE_EQ(first.minimum().handle, decreased_after_meld);
  REQUIRE(first.valid_structure());

  FibonacciMinHeap moved(std::move(first));
  REQUIRE(first.empty());
  REQUIRE(moved.contains(second_handle));
  REQUIRE(moved.valid_structure());

  FibonacciMinHeap assigned;
  const auto discarded = assigned.insert(-100);
  assigned = std::move(moved);
  REQUIRE(!assigned.contains(discarded));
  REQUIRE(assigned.contains(second_handle));
  REQUIRE(moved.empty());
  REQUIRE(assigned.valid_structure());
}

TEST_CASE(fibonacci_heap_equal_key_tie_cut_preserves_root_minimum) {
  FibonacciMinHeap heap;
  const auto early_handle = heap.insert(100);
  const auto later_parent = heap.insert(0);
  const auto consolidation_trigger = heap.insert(-1);

  REQUIRE_EQ(heap.extract_min().handle, consolidation_trigger);
  REQUIRE(heap.valid_structure());

  // Consolidation makes the lower-key later handle the parent of the earlier
  // handle. Decreasing the child to the same key must still cut it because the
  // public heap order is the full deterministic (key, handle) pair.
  heap.decrease_key(early_handle, 0);
  REQUIRE_EQ(heap.minimum().handle, early_handle);
  REQUIRE(heap.contains(later_parent));
  REQUIRE(heap.valid_structure());
}

TEST_CASE(fibonacci_heap_consolidation_cuts_and_potential_invariants) {
  FibonacciMinHeap heap;
  std::vector<FibonacciMinHeap::Handle> handles;
  handles.reserve(128);
  for (std::int64_t key = 0; key < 128; ++key) {
    handles.push_back(heap.insert(key));
  }

  REQUIRE_EQ(heap.extract_min().key, std::int64_t{0});
  REQUIRE(heap.root_count() < std::size_t{127});
  REQUIRE(heap.valid_structure());

  bool observed_mark = heap.marked_count() > 0U;
  bool observed_cascading_cut = false;
  for (std::size_t index = handles.size(); index-- > 1U;) {
    if (!heap.contains(handles[index])) {
      continue;
    }
    const std::int64_t lowered =
        -static_cast<std::int64_t>(index) - std::int64_t{1000};
    const std::size_t roots_before = heap.root_count();
    heap.decrease_key(handles[index], lowered);
    const std::size_t roots_after = heap.root_count();
    observed_mark = observed_mark || heap.marked_count() > 0U;
    observed_cascading_cut =
        observed_cascading_cut || roots_after > roots_before + 1U;
    REQUIRE(heap.valid_structure());
    REQUIRE(heap.marked_count() <= heap.size());
    REQUIRE_EQ(heap.potential(),
               heap.root_count() + 2U * heap.marked_count());
  }
  REQUIRE(observed_mark);
  REQUIRE(observed_cascading_cut);
}

TEST_CASE(fibonacci_heap_randomized_differential_against_ordered_oracle) {
  std::mt19937_64 rng(0xF1B0ACC1ULL);
  FibonacciMinHeap heap;
  std::map<FibonacciMinHeap::Handle, std::int64_t> keys;
  std::multiset<std::pair<std::int64_t, FibonacciMinHeap::Handle>> oracle;

  for (std::size_t step = 0; step < 5000U; ++step) {
    const std::uint64_t choice = rng() % 100U;
    if (keys.empty() || choice < 48U) {
      const auto key = static_cast<std::int64_t>(rng() % 20001U) - 10000;
      const auto handle = heap.insert(key);
      keys.emplace(handle, key);
      oracle.emplace(key, handle);
    } else if (choice < 78U) {
      const std::size_t offset = static_cast<std::size_t>(rng() % keys.size());
      auto iterator = keys.begin();
      std::advance(iterator, static_cast<std::ptrdiff_t>(offset));
      const auto handle = iterator->first;
      const auto old_key = iterator->second;
      const auto decrement = static_cast<std::int64_t>(rng() % 1000U);
      const auto new_key =
          old_key < std::numeric_limits<std::int64_t>::min() + decrement
              ? std::numeric_limits<std::int64_t>::min()
              : old_key - decrement;
      const auto oracle_position = oracle.find({old_key, handle});
      REQUIRE(oracle_position != oracle.end());
      oracle.erase(oracle_position);
      heap.decrease_key(handle, new_key);
      iterator->second = new_key;
      oracle.emplace(new_key, handle);
    } else {
      const auto expected = *oracle.begin();
      const auto actual = heap.extract_min();
      REQUIRE_EQ(actual.key, expected.first);
      REQUIRE_EQ(actual.handle, expected.second);
      oracle.erase(oracle.begin());
      keys.erase(actual.handle);
    }

    REQUIRE_EQ(heap.size(), keys.size());
    REQUIRE(heap.valid_structure());
    if (!keys.empty()) {
      const auto expected = *oracle.begin();
      const auto actual = heap.minimum();
      REQUIRE_EQ(actual.key, expected.first);
      REQUIRE_EQ(actual.handle, expected.second);
    }
  }

  while (!oracle.empty()) {
    const auto expected = *oracle.begin();
    const auto actual = heap.extract_min();
    REQUIRE_EQ(actual.key, expected.first);
    REQUIRE_EQ(actual.handle, expected.second);
    oracle.erase(oracle.begin());
    REQUIRE(heap.valid_structure());
  }
  REQUIRE(heap.empty());
}

TEST_CASE(fibonacci_heap_randomized_meld_preserves_foreign_handles) {
  std::mt19937_64 rng(0x4D454C44ULL);
  for (std::size_t trial = 0; trial < 120U; ++trial) {
    FibonacciMinHeap left;
    FibonacciMinHeap right;
    std::multiset<std::pair<std::int64_t, FibonacciMinHeap::Handle>> oracle;
    std::vector<FibonacciMinHeap::Handle> right_handles;

    for (std::size_t index = 0; index < 30U; ++index) {
      const auto key = static_cast<std::int64_t>(rng() % 500U) - 250;
      auto& heap = (index % 2U == 0U) ? left : right;
      const auto handle = heap.insert(key);
      oracle.emplace(key, handle);
      if (index % 2U != 0U) {
        right_handles.push_back(handle);
      }
    }

    left.meld(std::move(right));
    REQUIRE(right.empty());
    REQUIRE(left.valid_structure());

    const auto handle = right_handles[trial % right_handles.size()];
    const auto position = std::find_if(
        oracle.begin(), oracle.end(),
        [handle](const auto& entry) { return entry.second == handle; });
    REQUIRE(position != oracle.end());
    const auto old_key = position->first;
    oracle.erase(position);
    left.decrease_key(handle, old_key - 1000);
    oracle.emplace(old_key - 1000, handle);
    REQUIRE(left.valid_structure());

    while (!oracle.empty()) {
      const auto expected = *oracle.begin();
      const auto actual = left.extract_min();
      REQUIRE_EQ(actual.key, expected.first);
      REQUIRE_EQ(actual.handle, expected.second);
      oracle.erase(oracle.begin());
      REQUIRE(left.valid_structure());
    }
  }
}
