#pragma once

#include "algorithms/data_structures/persistent_hamt_set.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace persistent_hamt_set_test_detail {

using algorithms::data_structures::PersistentHamtSet64;

inline void require_matches(
    const PersistentHamtSet64& set,
    const std::set<std::uint64_t>& oracle,
    const std::vector<std::uint64_t>& probes) {
  REQUIRE_EQ(set.size(), oracle.size());
  REQUIRE(set.empty() == oracle.empty());
  for (const std::uint64_t key : probes) {
    REQUIRE(set.contains(key) == oracle.contains(key));
  }
}

}  // namespace persistent_hamt_set_test_detail

TEST_CASE(persistent_hamt_set_empty_basic_and_branching_versions) {
  using namespace persistent_hamt_set_test_detail;

  const PersistentHamtSet64 v0;
  REQUIRE(v0.empty());
  REQUIRE_EQ(v0.size(), 0U);
  REQUIRE_EQ(v0.reachable_node_count(), 0U);
  REQUIRE(v0.valid_structure());

  const auto v1 = v0.insert(7U);
  const auto v2 = v1.insert(9U);
  const auto branch = v1.insert(11U);

  REQUIRE(!v0.contains(7U));
  REQUIRE(v1.contains(7U));
  REQUIRE(!v1.contains(9U));
  REQUIRE(v2.contains(7U));
  REQUIRE(v2.contains(9U));
  REQUIRE(!v2.contains(11U));
  REQUIRE(branch.contains(7U));
  REQUIRE(branch.contains(11U));
  REQUIRE(!branch.contains(9U));

  REQUIRE_EQ(v0.size(), 0U);
  REQUIRE_EQ(v1.size(), 1U);
  REQUIRE_EQ(v2.size(), 2U);
  REQUIRE_EQ(branch.size(), 2U);
  REQUIRE(v0.valid_structure());
  REQUIRE(v1.valid_structure());
  REQUIRE(v2.valid_structure());
  REQUIRE(branch.valid_structure());
}

TEST_CASE(persistent_hamt_set_duplicate_insert_and_absent_erase_are_stable) {
  PersistentHamtSet64 base;
  base = base.insert(5U).insert(9U).insert(13U);

  const auto duplicate = base.insert(9U);
  const auto absent = base.erase(999U);

  REQUIRE_EQ(duplicate.size(), base.size());
  REQUIRE_EQ(absent.size(), base.size());
  REQUIRE_EQ(duplicate.reachable_node_count(),
             base.reachable_node_count());
  REQUIRE_EQ(absent.reachable_node_count(),
             base.reachable_node_count());

  for (const std::uint64_t key : {5U, 9U, 13U, 999U}) {
    REQUIRE(duplicate.contains(key) == base.contains(key));
    REQUIRE(absent.contains(key) == base.contains(key));
  }
  REQUIRE(duplicate.valid_structure());
  REQUIRE(absent.valid_structure());
}

TEST_CASE(persistent_hamt_set_erase_preserves_older_versions_and_extremes) {
  const std::uint64_t maximum =
      std::numeric_limits<std::uint64_t>::max();

  PersistentHamtSet64 original;
  original = original.insert(0U)
                 .insert(maximum)
                 .insert(UINT64_C(0x8000000000000000))
                 .insert(42U);

  const auto without_zero = original.erase(0U);
  const auto without_max = original.erase(maximum);

  REQUIRE(original.contains(0U));
  REQUIRE(original.contains(maximum));
  REQUIRE(original.contains(UINT64_C(0x8000000000000000)));
  REQUIRE(original.contains(42U));

  REQUIRE(!without_zero.contains(0U));
  REQUIRE(without_zero.contains(maximum));
  REQUIRE(without_zero.contains(42U));

  REQUIRE(without_max.contains(0U));
  REQUIRE(!without_max.contains(maximum));
  REQUIRE(without_max.contains(42U));

  REQUIRE_EQ(original.size(), 4U);
  REQUIRE_EQ(without_zero.size(), 3U);
  REQUIRE_EQ(without_max.size(), 3U);
  REQUIRE(original.valid_structure());
  REQUIRE(without_zero.valid_structure());
  REQUIRE(without_max.valid_structure());
}

TEST_CASE(persistent_hamt_set_dense_insert_erase_structure_replay) {
  PersistentHamtSet64 full;
  for (std::uint64_t key = 0U; key < 2048U; ++key) {
    full = full.insert(key);
  }
  REQUIRE_EQ(full.size(), 2048U);
  REQUIRE(full.valid_structure());

  PersistentHamtSet64 evens = full;
  for (std::uint64_t key = 1U; key < 2048U; key += 2U) {
    evens = evens.erase(key);
  }

  REQUIRE_EQ(evens.size(), 1024U);
  REQUIRE(evens.valid_structure());
  REQUIRE(full.valid_structure());
  for (std::uint64_t key = 0U; key < 2048U; ++key) {
    REQUIRE(full.contains(key));
    REQUIRE(evens.contains(key) == ((key & 1U) == 0U));
  }
}

TEST_CASE(persistent_hamt_set_randomized_branching_matches_std_set) {
  using namespace persistent_hamt_set_test_detail;

  struct Version {
    PersistentHamtSet64 set;
    std::set<std::uint64_t> oracle;
  };

  std::mt19937_64 random(0x48414D745EEDULL);
  std::vector<Version> versions(1U);

  for (std::size_t step = 0U; step < 1400U; ++step) {
    const std::size_t base =
        static_cast<std::size_t>(random() % versions.size());
    Version next = versions[base];

    std::uint64_t key{};
    const std::uint64_t pick = random() % 17U;
    if (pick == 0U) {
      key = 0U;
    } else if (pick == 1U) {
      key = std::numeric_limits<std::uint64_t>::max();
    } else {
      key = random() % 257U;
    }

    if ((random() & 1ULL) == 0ULL) {
      next.set = next.set.insert(key);
      next.oracle.insert(key);
    } else {
      next.set = next.set.erase(key);
      next.oracle.erase(key);
    }

    const std::array<std::uint64_t, 8U> probes{
        0U,
        1U,
        17U,
        128U,
        256U,
        std::numeric_limits<std::uint64_t>::max(),
        random() % 257U,
        random()};

    std::vector<std::uint64_t> probe_vector(
        probes.begin(), probes.end());
    require_matches(next.set, next.oracle, probe_vector);
    if (step % 31U == 0U) {
      REQUIRE(next.set.valid_structure());
    }
    versions.push_back(std::move(next));

    if (step % 97U == 0U) {
      const std::size_t old =
          static_cast<std::size_t>(random() % versions.size());
      require_matches(versions[old].set, versions[old].oracle,
                      probe_vector);
      REQUIRE(versions[old].set.valid_structure());
    }
  }

  for (std::size_t index = 0U; index < versions.size(); index += 137U) {
    const std::vector<std::uint64_t> probes{
        0U, 42U, 128U, 256U,
        std::numeric_limits<std::uint64_t>::max()};
    require_matches(versions[index].set, versions[index].oracle,
                    probes);
    REQUIRE(versions[index].set.valid_structure());
  }
}
