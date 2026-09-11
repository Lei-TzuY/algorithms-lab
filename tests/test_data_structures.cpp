#include "test_framework.hpp"

#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/data_structures/disjoint_set_union.hpp"
#include "algorithms/data_structures/treap_set.hpp"

using algorithms::data_structures::BinaryHeap;
using algorithms::data_structures::DisjointSetUnion;
using algorithms::data_structures::TreapSet;

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

namespace {

void require_treap_matches_reference(
    const TreapSet& treap, const std::set<std::int64_t>& reference) {
  REQUIRE(treap.valid_invariants());
  REQUIRE_EQ(treap.size(), reference.size());

  const std::vector<std::int64_t> expected(reference.begin(), reference.end());
  REQUIRE_EQ(treap.inorder_keys(), expected);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    REQUIRE_EQ(treap.kth(index), std::optional<std::int64_t>{expected[index]});
  }
  REQUIRE(!treap.kth(expected.size()).has_value());

  for (std::int64_t probe = -550; probe <= 550; probe += 25) {
    const auto lower = reference.lower_bound(probe);
    const auto expected_rank =
        static_cast<std::size_t>(std::distance(reference.begin(), lower));
    REQUIRE_EQ(treap.rank(probe), expected_rank);
    REQUIRE_EQ(treap.contains(probe), reference.contains(probe));
  }
}

}  // namespace

TEST_CASE(treap_set_boundaries_replay_and_order_statistics) {
  TreapSet treap(1);
  REQUIRE(treap.empty());
  REQUIRE(treap.valid_invariants());
  REQUIRE(!treap.root_key().has_value());

  REQUIRE(treap.insert(0));
  REQUIRE(treap.insert(std::numeric_limits<std::int64_t>::min()));
  REQUIRE(treap.insert(std::numeric_limits<std::int64_t>::max()));
  REQUIRE(!treap.insert(0));
  REQUIRE_EQ(treap.rank(std::numeric_limits<std::int64_t>::min()),
             std::size_t{0});
  REQUIRE_EQ(treap.rank(0), std::size_t{1});
  REQUIRE_EQ(treap.kth(0),
             std::optional<std::int64_t>{
                 std::numeric_limits<std::int64_t>::min()});
  REQUIRE_EQ(treap.kth(1), std::optional<std::int64_t>{0});
  REQUIRE_EQ(treap.kth(2),
             std::optional<std::int64_t>{
                 std::numeric_limits<std::int64_t>::max()});
  REQUIRE(!treap.kth(3).has_value());
  REQUIRE(!treap.erase(7));
  REQUIRE(treap.erase(0));
  REQUIRE(treap.valid_invariants());

  TreapSet first(0x12345678ULL);
  TreapSet second(0x12345678ULL);
  const std::vector<std::int64_t> keys = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
  for (const auto key : keys) {
    REQUIRE(first.insert(key));
    REQUIRE(second.insert(key));
  }
  REQUIRE(!first.insert(5));
  REQUIRE_EQ(first.preorder_keys(), second.preorder_keys());
  REQUIRE_EQ(first.root_key(), second.root_key());
  REQUIRE(first.valid_invariants());
  REQUIRE(second.valid_invariants());
}

TEST_CASE(treap_set_randomized_differential_against_std_set) {
  TreapSet treap(0xA17C0DEULL);
  std::set<std::int64_t> reference;
  std::mt19937_64 rng(0xD00DFEEDULL);
  std::uniform_int_distribution<std::int64_t> key_dist(-500, 500);
  std::uniform_int_distribution<int> operation_dist(0, 4);

  for (int step = 0; step < 20000; ++step) {
    const auto key = key_dist(rng);
    const int operation = operation_dist(rng);
    if (operation <= 1) {
      REQUIRE_EQ(treap.insert(key), reference.insert(key).second);
    } else if (operation == 2) {
      REQUIRE_EQ(treap.erase(key), reference.erase(key) != 0);
    } else if (operation == 3) {
      REQUIRE_EQ(treap.contains(key), reference.contains(key));
    } else {
      const auto lower = reference.lower_bound(key);
      const auto expected_rank =
          static_cast<std::size_t>(std::distance(reference.begin(), lower));
      REQUIRE_EQ(treap.rank(key), expected_rank);
    }

    if ((step % 97) == 0) {
      require_treap_matches_reference(treap, reference);
    }
  }
  require_treap_matches_reference(treap, reference);
}

TEST_CASE(treap_set_sorted_insertion_and_repeated_root_erasure) {
  TreapSet treap(42);
  for (std::int64_t key = 0; key < 5000; ++key) {
    REQUIRE(treap.insert(key));
  }
  REQUIRE(treap.valid_invariants());

  for (int step = 0; step < 5000; ++step) {
    const auto root = treap.root_key();
    REQUIRE(root.has_value());
    REQUIRE(treap.erase(*root));
    if ((step % 101) == 0) {
      REQUIRE(treap.valid_invariants());
    }
  }
  REQUIRE(treap.empty());
  REQUIRE(treap.valid_invariants());
}
