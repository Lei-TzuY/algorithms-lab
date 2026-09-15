#pragma once

#include "algorithms/data_structures/pairing_heap.hpp"

#include <cstddef>
#include <functional>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pairing_heap_test_detail {

using algorithms::data_structures::PairingHeap;

void verify_heap(const PairingHeap<int>& heap, const std::multiset<int>& oracle) {
  REQUIRE(heap.valid_structure());
  REQUIRE_EQ(heap.size(), oracle.size());
  REQUIRE(heap.empty() == oracle.empty());
  if (!oracle.empty()) {
    REQUIRE_EQ(heap.top(), *oracle.begin());
  }
}

}  // namespace pairing_heap_test_detail

TEST_CASE(pairing_heap_empty_order_and_duplicates) {
  using algorithms::data_structures::PairingHeap;
  PairingHeap<int> heap;
  REQUIRE(heap.empty());
  REQUIRE_EQ(heap.size(), 0U);
  REQUIRE(heap.valid_structure());
  REQUIRE_THROWS_AS(heap.top(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.pop(), std::out_of_range);

  for (int value : {5, 1, 7, 1, 3, 9, 0, 0, 4}) {
    heap.push(value);
    REQUIRE(heap.valid_structure());
  }
  const std::vector<int> expected{0, 0, 1, 1, 3, 4, 5, 7, 9};
  std::vector<int> actual;
  while (!heap.empty()) {
    actual.push_back(heap.pop());
    REQUIRE(heap.valid_structure());
  }
  REQUIRE(actual == expected);
}

TEST_CASE(pairing_heap_meld_move_and_self_meld) {
  using algorithms::data_structures::PairingHeap;
  PairingHeap<int> first;
  PairingHeap<int> second;
  for (int value : {8, 2, 6}) {
    first.push(value);
  }
  for (int value : {7, 1, 5, 3}) {
    second.push(value);
  }

  first.meld(std::move(second));
  REQUIRE(second.empty());
  REQUIRE_EQ(second.size(), 0U);
  REQUIRE_EQ(first.size(), 7U);
  REQUIRE_EQ(first.top(), 1);
  REQUIRE(first.valid_structure());

  first.meld(std::move(first));
  REQUIRE_EQ(first.size(), 7U);
  REQUIRE_EQ(first.top(), 1);
  REQUIRE(first.valid_structure());

  PairingHeap<int> moved(std::move(first));
  REQUIRE(first.empty());
  REQUIRE_EQ(moved.size(), 7U);
  REQUIRE(moved.valid_structure());

  PairingHeap<int> assigned;
  assigned.push(99);
  assigned = std::move(moved);
  REQUIRE(moved.empty());
  REQUIRE_EQ(assigned.size(), 7U);
  REQUIRE(assigned.valid_structure());

  const std::vector<int> expected{1, 2, 3, 5, 6, 7, 8};
  std::vector<int> actual;
  while (!assigned.empty()) {
    actual.push_back(assigned.pop());
  }
  REQUIRE(actual == expected);
}

TEST_CASE(pairing_heap_custom_max_order) {
  algorithms::data_structures::PairingHeap<int, std::greater<int>> heap;
  for (int value : {1, 9, 3, 7, 9}) {
    heap.push(value);
  }
  const std::vector<int> expected{9, 9, 7, 3, 1};
  std::vector<int> actual;
  while (!heap.empty()) {
    REQUIRE(heap.valid_structure());
    actual.push_back(heap.pop());
  }
  REQUIRE(actual == expected);
}

TEST_CASE(pairing_heap_deep_ownership_cleanup) {
  // Strictly decreasing inserts form a deep first-child chain. This regression
  // exercises the heap's iterative destruction path instead of recursive
  // unique_ptr teardown.
  {
    algorithms::data_structures::PairingHeap<int> heap;
    for (int value = 20000; value >= 0; --value) {
      heap.push(value);
    }
    REQUIRE_EQ(heap.size(), 20001U);
    REQUIRE_EQ(heap.top(), 0);
    REQUIRE(heap.valid_structure());
  }
  REQUIRE(true);
}

TEST_CASE(pairing_heap_randomized_multiset_and_meld_differential) {
  using algorithms::data_structures::PairingHeap;
  constexpr std::size_t kHeapCount = 8;
  std::vector<PairingHeap<int>> heaps(kHeapCount);
  std::vector<std::multiset<int>> oracles(kHeapCount);
  std::mt19937 random(0x5EED1234U);
  std::uniform_int_distribution<int> value_dist(-100, 100);
  std::uniform_int_distribution<int> heap_dist(
      0, static_cast<int>(kHeapCount - 1U));
  std::uniform_int_distribution<int> op_dist(0, 99);

  for (int step = 0; step < 8000; ++step) {
    const auto index = static_cast<std::size_t>(heap_dist(random));
    const int op = op_dist(random);
    std::size_t other = index;

    if (op < 55) {
      const int value = value_dist(random);
      heaps[index].push(value);
      oracles[index].insert(value);
    } else if (op < 80) {
      if (!oracles[index].empty()) {
        const int expected = *oracles[index].begin();
        REQUIRE_EQ(heaps[index].top(), expected);
        REQUIRE_EQ(heaps[index].pop(), expected);
        oracles[index].erase(oracles[index].begin());
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

    pairing_heap_test_detail::verify_heap(heaps[index], oracles[index]);
    if (other != index) {
      pairing_heap_test_detail::verify_heap(heaps[other], oracles[other]);
    }
    if (step % 97 == 0) {
      for (std::size_t heap_index = 0; heap_index < kHeapCount; ++heap_index) {
        pairing_heap_test_detail::verify_heap(heaps[heap_index],
                                              oracles[heap_index]);
      }
    }
  }
}
