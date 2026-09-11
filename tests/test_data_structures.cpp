#include "test_framework.hpp"

#include <algorithm>
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
#include "algorithms/data_structures/fks_static_set.hpp"
#include "algorithms/data_structures/treap_set.hpp"

using algorithms::data_structures::BinaryHeap;
using algorithms::data_structures::DisjointSetUnion;
using algorithms::data_structures::FksStaticSet32;
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

TEST_CASE(fks_static_set_empty_boundaries_and_replay) {
  FksStaticSet32 empty({}, 1U);
  REQUIRE(empty.empty());
  REQUIRE(empty.valid_structure());
  REQUIRE(!empty.contains(0U));
  REQUIRE_THROWS_AS(FksStaticSet32(std::vector<std::uint32_t>{1U}, 1U, 0U, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(FksStaticSet32(std::vector<std::uint32_t>{1U}, 1U, 1U, 0U),
                    std::invalid_argument);

  std::vector<std::uint32_t> keys{
      0U, 1U, 1U, 2U, 42U, std::numeric_limits<std::uint32_t>::max(),
      1234567890U};
  FksStaticSet32 set(keys, 0x123456789abcdef0ULL);
  REQUIRE_EQ(set.size(), 6U);
  REQUIRE(set.valid_structure());
  REQUIRE(set.secondary_slot_count() <= 4U * set.size());
  for (const std::uint32_t key : keys) {
    REQUIRE(set.contains(key));
  }
  REQUIRE(!set.contains(3U));
  REQUIRE(!set.contains(1234567891U));

  std::reverse(keys.begin(), keys.end());
  FksStaticSet32 replay(keys, 0x123456789abcdef0ULL);
  REQUIRE_EQ(set.primary_hash_parameters(), replay.primary_hash_parameters());
  REQUIRE_EQ(set.secondary_hash_parameters(), replay.secondary_hash_parameters());
  REQUIRE_EQ(set.primary_attempts(), replay.primary_attempts());
  REQUIRE_EQ(set.secondary_attempts(), replay.secondary_attempts());
  REQUIRE_EQ(set.secondary_slot_count(), replay.secondary_slot_count());
}

TEST_CASE(fks_static_set_bounded_construction_failure_is_replayable) {
  std::vector<std::uint32_t> keys;
  for (std::uint32_t i = 0U; i < 64U; ++i) {
    keys.push_back(i * 2654435761U);
  }
  REQUIRE_THROWS_AS(FksStaticSet32(keys, 20U, 1U, 128U), std::runtime_error);
  REQUIRE_THROWS_AS(FksStaticSet32(keys, 0U, 128U, 1U), std::runtime_error);
}

TEST_CASE(fks_static_set_randomized_differential_and_storage_bound) {
  std::mt19937_64 rng(0xF15CAFE123ULL);
  for (std::size_t trial = 0U; trial < 300U; ++trial) {
    const std::size_t requested = static_cast<std::size_t>(rng() % 129U);
    std::vector<std::uint32_t> input;
    input.reserve(requested + requested / 3U + 1U);
    std::set<std::uint32_t> oracle;
    for (std::size_t i = 0U; i < requested; ++i) {
      const auto key = static_cast<std::uint32_t>(rng());
      input.push_back(key);
      oracle.insert(key);
      if ((rng() & 3U) == 0U) {
        input.push_back(key);
      }
    }

    const std::uint64_t seed = rng();
    FksStaticSet32 set(input, seed);
    REQUIRE_EQ(set.size(), oracle.size());
    REQUIRE(set.valid_structure());
    REQUIRE(set.secondary_slot_count() <= 4U * set.size());
    for (const std::uint32_t key : oracle) {
      REQUIRE(set.contains(key));
    }
    for (std::size_t query = 0U; query < 256U; ++query) {
      const auto key = static_cast<std::uint32_t>(rng());
      REQUIRE_EQ(set.contains(key), oracle.count(key) != 0U);
    }

    std::shuffle(input.begin(), input.end(), rng);
    FksStaticSet32 replay(input, seed);
    REQUIRE_EQ(set.primary_hash_parameters(), replay.primary_hash_parameters());
    REQUIRE_EQ(set.secondary_hash_parameters(), replay.secondary_hash_parameters());
  }
}
