#pragma once

#include "algorithms/data_structures/cuckoo_hash_set.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

using algorithms::data_structures::CuckooHashSet;

std::vector<std::int64_t> cuckoo_reference_values(
    const std::set<std::int64_t>& reference) {
  return std::vector<std::int64_t>(reference.begin(), reference.end());
}

void require_cuckoo_matches(const CuckooHashSet& table,
                            const std::set<std::int64_t>& reference) {
  REQUIRE(table.valid_structure());
  REQUIRE_EQ(table.size(), reference.size());
  REQUIRE_EQ(table.empty(), reference.empty());
  REQUIRE(table.sorted_values() == cuckoo_reference_values(reference));
}

TEST_CASE(cuckoo_hash_set_basic_semantics_and_signed_boundaries) {
  CuckooHashSet table(0xC0C0A123ULL, 4);
  std::set<std::int64_t> reference;
  const std::vector<std::int64_t> keys = {
      std::numeric_limits<std::int64_t>::min(),
      -1,
      0,
      1,
      std::numeric_limits<std::int64_t>::max(),
  };

  for (const std::int64_t key : keys) {
    REQUIRE(table.insert(key));
    reference.insert(key);
    REQUIRE(table.contains(key));
    REQUIRE(!table.insert(key));
    require_cuckoo_matches(table, reference);
  }
  REQUIRE(!table.contains(77));
  REQUIRE(!table.erase(77));

  for (const std::int64_t key : keys) {
    REQUIRE(table.erase(key));
    reference.erase(key);
    REQUIRE(!table.contains(key));
    require_cuckoo_matches(table, reference);
  }
}

TEST_CASE(cuckoo_hash_set_growth_rebuild_and_layout_replay) {
  CuckooHashSet first(0xA11CE5EEDULL, 4);
  CuckooHashSet second(0xA11CE5EEDULL, 4);
  for (std::int64_t key = -700; key <= 700; ++key) {
    REQUIRE(first.insert(key));
    REQUIRE(second.insert(key));
  }
  REQUIRE(first.rebuild_count() > 0);
  REQUIRE(first.capacity_per_table() > 4);
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE(first.layout() == second.layout());
  REQUIRE_EQ(first.rehash_generation(), second.rehash_generation());
  REQUIRE_EQ(first.rebuild_count(), second.rebuild_count());
}

TEST_CASE(cuckoo_hash_set_displacement_cycle_rebuild_preserves_every_key) {
  CuckooHashSet table(16, 64);
  for (std::int64_t index = 0; index < 30; ++index) {
    REQUIRE(table.insert(index * 1000003 + 17));
  }
  REQUIRE_EQ(table.rebuild_count(), std::size_t{0});
  REQUIRE_EQ(table.capacity_per_table(), std::size_t{64});

  REQUIRE(table.insert(30 * 1000003 + 17));
  REQUIRE_EQ(table.rebuild_count(), std::size_t{1});
  REQUIRE_EQ(table.capacity_per_table(), std::size_t{64});
  REQUIRE_EQ(table.size(), std::size_t{31});
  REQUIRE(table.valid_structure());
  for (std::int64_t index = 0; index <= 30; ++index) {
    REQUIRE(table.contains(index * 1000003 + 17));
  }
}

TEST_CASE(cuckoo_hash_set_different_seed_preserves_exact_set_semantics) {
  CuckooHashSet first(1, 8);
  CuckooHashSet second(2, 8);
  for (std::int64_t key = 0; key < 512; ++key) {
    REQUIRE(first.insert(key * 17 - 4000));
    REQUIRE(second.insert(key * 17 - 4000));
  }
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE(first.sorted_values() == second.sorted_values());
}

TEST_CASE(cuckoo_hash_set_randomized_differential_against_std_set) {
  CuckooHashSet table(0xD00DFEED1234ULL, 8);
  std::set<std::int64_t> reference;
  std::mt19937_64 rng(0xC0111510AULL);

  constexpr std::size_t operations = 18000;
  for (std::size_t step = 0; step < operations; ++step) {
    const std::uint64_t raw = rng();
    const std::int64_t key = static_cast<std::int64_t>(raw % 5001ULL) - 2500;
    const unsigned operation = static_cast<unsigned>((raw >> 17U) % 3ULL);
    if (operation == 0U) {
      REQUIRE_EQ(table.insert(key), reference.insert(key).second);
    } else if (operation == 1U) {
      REQUIRE_EQ(table.erase(key), reference.erase(key) != 0);
    } else {
      REQUIRE_EQ(table.contains(key), reference.contains(key));
    }

    if ((step % 37U) == 0U) {
      require_cuckoo_matches(table, reference);
    }
  }
  require_cuckoo_matches(table, reference);
}

}  // namespace
