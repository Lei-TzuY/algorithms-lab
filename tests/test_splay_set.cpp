#include "algorithms/data_structures/splay_set.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {
using algorithms::data_structures::SplaySet;

TEST_CASE(splay_set_insert_access_and_duplicate_behavior) {
  SplaySet set;
  REQUIRE(set.empty());
  REQUIRE(set.insert(10));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(10));
  REQUIRE(set.insert(5));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(5));
  REQUIRE(set.insert(15));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(15));
  REQUIRE(!set.insert(10));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(10));
  REQUIRE(set.contains(5));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(5));
  REQUIRE(!set.contains(7));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(10));
  REQUIRE(set.valid_invariants());
  REQUIRE_EQ(set.inorder_keys(), std::vector<SplaySet::Key>({5, 10, 15}));
}

TEST_CASE(splay_set_rotation_patterns_preserve_order) {
  {
    SplaySet zig_zig;
    REQUIRE(zig_zig.insert(10));
    REQUIRE(zig_zig.insert(20));
    REQUIRE(zig_zig.insert(30));
    REQUIRE(zig_zig.contains(10));
    REQUIRE_EQ(zig_zig.root_key(), std::optional<SplaySet::Key>(10));
    REQUIRE(zig_zig.valid_invariants());
    REQUIRE_EQ(zig_zig.inorder_keys(),
               std::vector<SplaySet::Key>({10, 20, 30}));
  }

  {
    SplaySet zig_zag;
    REQUIRE(zig_zag.insert(10));
    REQUIRE(zig_zag.insert(30));
    REQUIRE(zig_zag.insert(20));
    REQUIRE_EQ(zig_zag.root_key(), std::optional<SplaySet::Key>(20));
    REQUIRE(zig_zag.valid_invariants());
    REQUIRE_EQ(zig_zag.inorder_keys(),
               std::vector<SplaySet::Key>({10, 20, 30}));
  }
}

TEST_CASE(splay_set_supports_full_int64_key_domain) {
  SplaySet set;
  const auto low = std::numeric_limits<SplaySet::Key>::min();
  const auto high = std::numeric_limits<SplaySet::Key>::max();
  REQUIRE(set.insert(low));
  REQUIRE(set.insert(0));
  REQUIRE(set.insert(high));
  REQUIRE(set.contains(low));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(low));
  REQUIRE(set.contains(high));
  REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(high));
  REQUIRE(set.erase(0));
  REQUIRE(set.valid_invariants());
  REQUIRE_EQ(set.inorder_keys(), std::vector<SplaySet::Key>({low, high}));
}

TEST_CASE(splay_set_erase_join_and_missing_access) {
  SplaySet set;
  for (const auto key : {40, 20, 60, 10, 30, 50, 70}) {
    REQUIRE(set.insert(key));
  }
  REQUIRE(set.erase(40));
  REQUIRE(!set.contains(40));
  REQUIRE(set.valid_invariants());
  REQUIRE(set.erase(10));
  REQUIRE(set.erase(70));
  REQUIRE(!set.erase(999));
  REQUIRE(set.valid_invariants());
  REQUIRE_EQ(set.inorder_keys(), std::vector<SplaySet::Key>({20, 30, 50, 60}));
}

TEST_CASE(splay_set_sequential_access_splays_each_hit_to_root) {
  SplaySet set;
  for (SplaySet::Key key = 0; key < 64; ++key) {
    REQUIRE(set.insert(key));
  }
  for (SplaySet::Key key = 0; key < 64; ++key) {
    REQUIRE(set.contains(key));
    REQUIRE_EQ(set.root_key(), std::optional<SplaySet::Key>(key));
    REQUIRE(set.valid_invariants());
  }
}

TEST_CASE(splay_set_randomized_differential_against_std_set) {
  SplaySet actual;
  std::set<SplaySet::Key> oracle;
  std::mt19937_64 rng(0x5A1A7EULL);
  std::uniform_int_distribution<int> op_dist(0, 2);
  std::uniform_int_distribution<std::int64_t> key_dist(-150, 150);

  for (int step = 0; step < 20000; ++step) {
    const auto key = key_dist(rng);
    const int op = op_dist(rng);
    if (op == 0) {
      REQUIRE_EQ(actual.insert(key), oracle.insert(key).second);
    } else if (op == 1) {
      REQUIRE_EQ(actual.erase(key), oracle.erase(key) != 0);
    } else {
      REQUIRE_EQ(actual.contains(key), oracle.contains(key));
      if (oracle.contains(key)) {
        REQUIRE_EQ(actual.root_key(), std::optional<SplaySet::Key>(key));
      }
    }
    REQUIRE_EQ(actual.size(), oracle.size());
    REQUIRE(actual.valid_invariants());
    if (step % 37 == 0) {
      REQUIRE_EQ(actual.inorder_keys(),
                 std::vector<SplaySet::Key>(oracle.begin(), oracle.end()));
    }
  }
  REQUIRE_EQ(actual.inorder_keys(),
             std::vector<SplaySet::Key>(oracle.begin(), oracle.end()));
}

}  // namespace
