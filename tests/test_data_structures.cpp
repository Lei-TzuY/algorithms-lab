#include "test_framework.hpp"

#include <random>
#include <set>
#include <stdexcept>

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/data_structures/disjoint_set_union.hpp"

using algorithms::data_structures::BinaryHeap;
using algorithms::data_structures::DisjointSetUnion;

TEST_CASE(binary_heap_basic_and_duplicates) {
  BinaryHeap<int> heap;
  REQUIRE(heap.empty());
  REQUIRE_THROWS_AS(heap.top(), std::out_of_range);
  REQUIRE_THROWS_AS(heap.pop(), std::out_of_range);

  for (int value : {5, 1, 3, 1, -2, 9, 0}) {
    heap.push(value);
    REQUIRE(heap.valid_heap_property());
  }

  for (int expected : {-2, 0, 1, 1, 3, 5, 9}) {
    REQUIRE_EQ(heap.top(), expected);
    REQUIRE_EQ(heap.pop(), expected);
    REQUIRE(heap.valid_heap_property());
  }
  REQUIRE(heap.empty());
}

TEST_CASE(binary_heap_randomized_differential_against_multiset) {
  std::mt19937 rng(0xC001D00Du);
  std::uniform_int_distribution<int> operation_dist(0, 2);
  std::uniform_int_distribution<int> value_dist(-1000, 1000);
  BinaryHeap<int> heap;
  std::multiset<int> reference;

  for (int step = 0; step < 5000; ++step) {
    const bool should_push = reference.empty() || operation_dist(rng) != 0;
    if (should_push) {
      const int value = value_dist(rng);
      heap.push(value);
      reference.insert(value);
    } else {
      REQUIRE_EQ(heap.pop(), *reference.begin());
      reference.erase(reference.begin());
    }
    REQUIRE_EQ(heap.size(), reference.size());
    REQUIRE(heap.valid_heap_property());
    if (!reference.empty()) {
      REQUIRE_EQ(heap.top(), *reference.begin());
    }
  }
}

TEST_CASE(dsu_repeated_union_find_and_sizes) {
  DisjointSetUnion dsu(8);
  REQUIRE_EQ(dsu.components(), std::size_t{8});
  REQUIRE(dsu.unite(0, 1));
  REQUIRE(dsu.unite(1, 2));
  REQUIRE(!dsu.unite(0, 2));
  REQUIRE(dsu.connected(0, 2));
  REQUIRE_EQ(dsu.component_size(1), std::size_t{3});
  REQUIRE_EQ(dsu.components(), std::size_t{6});

  const auto representative = dsu.find(2);
  for (int repeat = 0; repeat < 20; ++repeat) {
    REQUIRE_EQ(dsu.find(2), representative);
  }

  REQUIRE(dsu.unite(3, 4));
  REQUIRE(dsu.unite(4, 5));
  REQUIRE(dsu.unite(0, 5));
  REQUIRE_EQ(dsu.component_size(3), std::size_t{6});
  REQUIRE_EQ(dsu.components(), std::size_t{3});
  REQUIRE_THROWS_AS(dsu.find(8), std::out_of_range);
}
